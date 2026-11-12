// SPDX-License-Identifier: GPL-2.0
/*
 * smm_main.c - /dev/secure_mmap: archivo confidencial protegido en la capa kernel
 *
 * Basado en el Paso 6 del repositorio SO2026B/USOS/CapaKernel (paginacion por
 * demanda con vm_operations_struct->fault), endurecido con:
 *
 *  1. CONTROL DE ACCESO EN EL ALTA DEL VMA (.mmap):
 *     - Solo el UID autorizado (parametro allowed_uid) puede mapear, leer o
 *       escribir. Se valida con las credenciales del proceso que llama a
 *       mmap(), no con los permisos POSIX del nodo (que son 0666 a proposito).
 *     - PROT_EXEC prohibido (W^X) y se retira VM_MAYEXEC.
 *     - MAP_PRIVATE + PROT_WRITE prohibido: elimina Copy-On-Write sobre la boveda.
 *     - MAP_SHARED + PROT_WRITE solo si allow_write=1.
 *     - Si el mapeo no es escribible se retira VM_MAYWRITE: mprotect(PROT_WRITE)
 *       falla con EACCES (no hay escalada de permisos despues del mmap) y
 *       FOLL_FORCE (/proc/self/mem, el vector de Dirty COW) no puede forzar COW.
 *
 *  2. AISLAMIENTO DEL TEXTO PLANO:
 *     - Cada mapeo recibe SUS PROPIAS paginas descifradas (no hay pagina de
 *       texto plano compartida entre procesos).
 *     - VM_IO: get_user_pages() rechaza la region -> process_vm_readv() y
 *       /proc/<pid>/mem no pueden leerla desde otro proceso.
 *     - VM_DONTCOPY: un fork() no hereda el mapeo.
 *     - VM_DONTDUMP: el texto plano no aparece en core dumps.
 *     - VM_DONTEXPAND + .may_split: no se puede crecer ni partir el VMA.
 *
 *  3. CIFRADO TRANSPARENTE (.fault): descifra la pagina pedida en el momento
 *     del page fault, verificando su CRC; si no coincide -> SIGBUS.
 *
 *  4. RE-CIFRADO AL CERRAR (.close): si la pagina cambio (MAP_SHARED escribible)
 *     se vuelve a cifrar con una nueva generacion de IV. Luego el texto plano
 *     se borra con memzero_explicit() antes de liberar la pagina.
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/kref.h>
#include <linux/cred.h>
#include <linux/uaccess.h>
#include <linux/ktime.h>
#include <linux/sched.h>

#include "smm.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sara Isabel Vasquez - SO 2026-2 EAFIT");
MODULE_DESCRIPTION("Boveda cifrada con control de acceso y auditoria sobre mmap()");
MODULE_VERSION("1.0");

/* ------------------------------------------------------------------------- */
/* Parametros                                                                */
/* ------------------------------------------------------------------------- */
int smm_allowed_uid = 1000;
module_param_named(allowed_uid, smm_allowed_uid, int, 0644);
MODULE_PARM_DESC(allowed_uid, "UID autorizado a usar la boveda (por defecto 1000)");

bool smm_allow_write;
module_param_named(allow_write, smm_allow_write, bool, 0644);
MODULE_PARM_DESC(allow_write, "Permitir MAP_SHARED|PROT_WRITE y write() al UID autorizado (por defecto no)");

bool smm_verbose;
module_param_named(verbose, smm_verbose, bool, 0644);
MODULE_PARM_DESC(verbose, "Registrar cada page fault en la bitacora (afecta el rendimiento)");

static unsigned int vault_pages = 256;
module_param(vault_pages, uint, 0444);
MODULE_PARM_DESC(vault_pages, "Tamano de la boveda en paginas de 4 KiB (por defecto 256 = 1 MiB)");

/* ------------------------------------------------------------------------- */
/* Contexto por mapeo                                                        */
/* ------------------------------------------------------------------------- */
struct smm_map {
	struct kref ref;        /* VMAs que comparten el contexto (mremap)  */
	struct mutex lock;      /* protege pages[]                          */
	struct page **pages;    /* texto plano, indice relativo a first     */
	unsigned long first;    /* vm_pgoff del mapeo                       */
	unsigned long npages;
	bool writable;
	pid_t pid;
	uid_t uid;
};

static dev_t smm_devt;
static struct cdev smm_cdev;
static struct class *smm_class;
static struct device *smm_device;

static uid_t smm_caller_uid(void)
{
	return from_kuid_munged(&init_user_ns, current_euid());
}

static bool smm_authorized(void)
{
	return smm_allowed_uid >= 0 && smm_caller_uid() == (uid_t)smm_allowed_uid;
}

/* ------------------------------------------------------------------------- */
/* vm_operations_struct                                                      */
/* ------------------------------------------------------------------------- */
static void smm_map_release(struct kref *kref)
{
	struct smm_map *m = container_of(kref, struct smm_map, ref);
	unsigned long i, reenc = 0;

	for (i = 0; i < m->npages; i++) {
		struct page *p = m->pages[i];
		void *va;

		if (!p)
			continue;
		va = page_address(p);
		if (m->writable && smm_vault_page_changed(m->first + i, va)) {
			if (!smm_vault_encrypt_page(m->first + i, va)) {
				reenc++;
				smm_count(SMM_C_REENCRYPT);
			}
		}
		memzero_explicit(va, PAGE_SIZE);  /* no dejar texto plano en RAM libre */
		put_page(p);
	}
	if (reenc)
		smm_event("pid=%d uid=%u: %lu pagina(s) modificadas re-cifradas al cerrar el mapeo",
			  m->pid, m->uid, reenc);
	kvfree(m->pages);
	kfree(m);
}

static void smm_vma_open(struct vm_area_struct *vma)
{
	struct smm_map *m = vma->vm_private_data;

	kref_get(&m->ref);
}

static void smm_vma_close(struct vm_area_struct *vma)
{
	struct smm_map *m = vma->vm_private_data;

	kref_put(&m->ref, smm_map_release);
}

/* Prohibe partir el VMA (munmap/mprotect parcial): la boveda se mapea completa o nada. */
static int smm_vma_may_split(struct vm_area_struct *vma, unsigned long addr)
{
	smm_count(SMM_C_SPLIT_DENIED);
	smm_event("pid=%d intento partir el VMA en 0x%lx (munmap/mprotect parcial) -> EINVAL",
		  current->pid, addr);
	return -EINVAL;
}

/*
 * smm_fault - Manejador de page fault de la boveda.
 *
 * La MMU no encontro una PTE valida para vmf->address; handle_mm_fault() llego
 * hasta aqui por medio de vma->vm_ops->fault. Entregamos una pagina NUEVA con
 * el texto plano de ESTE mapeo. Si ya la habiamos descifrado (por ejemplo,
 * despues de un madvise(MADV_DONTNEED) que borro la PTE) se reutiliza: asi
 * las escrituras de un MAP_SHARED no se pierden en la carrera DONTNEED/escritura.
 */
static vm_fault_t smm_fault(struct vm_fault *vmf)
{
	struct vm_area_struct *vma = vmf->vma;
	struct smm_map *m = vma->vm_private_data;
	unsigned long rel;
	struct page *page;
	u64 t0 = ktime_get_ns();
	int ret;

	if (vmf->pgoff < m->first || vmf->pgoff - m->first >= m->npages)
		return VM_FAULT_SIGBUS;
	rel = vmf->pgoff - m->first;

	mutex_lock(&m->lock);
	page = m->pages[rel];
	if (page) {
		smm_count(SMM_C_FAULT_REUSE);
	} else {
		page = alloc_page(GFP_KERNEL);
		if (!page) {
			mutex_unlock(&m->lock);
			return VM_FAULT_OOM;
		}
		ret = smm_vault_decrypt_page(vmf->pgoff, page_address(page));
		if (ret) {
			mutex_unlock(&m->lock);
			__free_page(page);
			if (ret == -EBADMSG) {
				smm_count(SMM_C_INTEGRITY);
				smm_event("INTEGRIDAD VIOLADA pagina %lu (pid=%d %s): CRC no coincide -> SIGBUS",
					  vmf->pgoff, current->pid, current->comm);
			}
			return VM_FAULT_SIGBUS;
		}
		m->pages[rel] = page;
	}
	get_page(page);     /* la referencia pasa a la PTE; zap_pte_range() la soltara */
	vmf->page = page;
	mutex_unlock(&m->lock);

	smm_count(SMM_C_FAULTS);
	smm_fault_time(ktime_get_ns() - t0);
	if (smm_verbose)
		smm_event("fault pid=%d %s pagina %lu addr=0x%lx %s",
			  current->pid, current->comm, vmf->pgoff, vmf->address,
			  (vmf->flags & FAULT_FLAG_WRITE) ? "ESCRITURA" : "lectura");
	return 0;
}

static const struct vm_operations_struct smm_vm_ops = {
	.open      = smm_vma_open,
	.close     = smm_vma_close,
	.fault     = smm_fault,
	.may_split = smm_vma_may_split,
};

/* ------------------------------------------------------------------------- */
/* file_operations                                                           */
/* ------------------------------------------------------------------------- */
static int smm_open(struct inode *inode, struct file *filp)
{
	smm_count(SMM_C_OPEN);
	return 0;
}

static int smm_deny(enum smm_counter c, int err, const char *why)
{
	smm_count(c);
	smm_event("DENEGADO pid=%d %s uid=%u: %s", current->pid, current->comm,
		  smm_caller_uid(), why);
	return err;
}

static int smm_mmap(struct file *filp, struct vm_area_struct *vma)
{
	unsigned long npages = vma_pages(vma);
	unsigned long vault_n = smm_vault_npages();
	vm_flags_t fl = vma->vm_flags;
	struct smm_map *m;

	/* 1. Identidad: credenciales del proceso que mapea, no permisos del nodo. */
	if (!smm_authorized())
		return smm_deny(SMM_C_DENY_UID, -EACCES, "mmap() de UID no autorizado");

	/* 2. Rango dentro de la boveda. */
	if (vma->vm_pgoff >= vault_n || npages > vault_n - vma->vm_pgoff)
		return smm_deny(SMM_C_DENY_RANGE, -EINVAL, "mmap() fuera de la boveda");

	/* 3. W^X: la boveda contiene datos, nunca codigo. */
	if (fl & VM_EXEC)
		return smm_deny(SMM_C_DENY_FLAGS, -EPERM, "mmap() con PROT_EXEC (W^X)");

	/* 4. Escritura. */
	if (fl & VM_WRITE) {
		if (!(fl & VM_SHARED))
			return smm_deny(SMM_C_DENY_FLAGS, -EPERM,
					"MAP_PRIVATE|PROT_WRITE (Copy-On-Write no permitido)");
		if (!smm_allow_write)
			return smm_deny(SMM_C_DENY_FLAGS, -EPERM,
					"MAP_SHARED|PROT_WRITE con allow_write=0");
	}

	m = kzalloc(sizeof(*m), GFP_KERNEL);
	if (!m)
		return -ENOMEM;
	m->pages = kvcalloc(npages, sizeof(*m->pages), GFP_KERNEL);
	if (!m->pages) {
		kfree(m);
		return -ENOMEM;
	}
	kref_init(&m->ref);
	mutex_init(&m->lock);
	m->first = vma->vm_pgoff;
	m->npages = npages;
	m->writable = !!(fl & VM_WRITE);
	m->pid = current->pid;
	m->uid = smm_caller_uid();

	/* 5. Endurecimiento del VMA. */
	smm_vm_flags_set(vma, VM_IO | VM_DONTEXPAND | VM_DONTDUMP | VM_DONTCOPY);
	smm_vm_flags_clear(vma, VM_MAYEXEC);
	if (!m->writable)
		smm_vm_flags_clear(vma, VM_MAYWRITE);

	vma->vm_ops = &smm_vm_ops;
	vma->vm_private_data = m;

	smm_count(SMM_C_MMAP_OK);
	smm_event("mmap OK pid=%d %s uid=%u paginas=%lu..%lu %s %s",
		  current->pid, current->comm, m->uid, m->first, m->first + npages - 1,
		  (fl & VM_SHARED) ? "SHARED" : "PRIVATE", m->writable ? "RW" : "RO");
	return 0;
}

/* read(): ruta clasica por syscall (sirve de linea base contra mmap en el benchmark). */
static ssize_t smm_read(struct file *filp, char __user *ubuf, size_t len, loff_t *ppos)
{
	loff_t size = (loff_t)smm_vault_npages() << PAGE_SHIFT;
	void *tmp;
	ssize_t done = 0;
	int ret = 0;

	if (!smm_authorized())
		return smm_deny(SMM_C_DENY_UID, -EACCES, "read() de UID no autorizado");
	if (*ppos >= size)
		return 0;
	len = min_t(loff_t, len, size - *ppos);

	tmp = (void *)__get_free_page(GFP_KERNEL);
	if (!tmp)
		return -ENOMEM;

	while (len) {
		unsigned long idx = *ppos >> PAGE_SHIFT;
		size_t off = *ppos & ~PAGE_MASK;
		size_t chunk = min_t(size_t, len, PAGE_SIZE - off);

		ret = smm_vault_decrypt_page(idx, tmp);
		if (ret) {
			if (ret == -EBADMSG) {
				smm_count(SMM_C_INTEGRITY);
				smm_event("INTEGRIDAD VIOLADA pagina %lu en read() pid=%d", idx,
					  current->pid);
			}
			ret = -EIO;
			break;
		}
		/* copy_to_user SIN locks tomados (el destino podria ser nuestra region mmap). */
		if (copy_to_user(ubuf + done, (u8 *)tmp + off, chunk)) {
			ret = -EFAULT;
			break;
		}
		*ppos += chunk;
		done += chunk;
		len -= chunk;
	}
	memzero_explicit(tmp, PAGE_SIZE);
	free_page((unsigned long)tmp);
	if (done)
		smm_count(SMM_C_READ);
	return done ? done : ret;
}

/* write(): permite al UID autorizado cargar su propio contenido en la boveda. */
static ssize_t smm_write(struct file *filp, const char __user *ubuf, size_t len,
			 loff_t *ppos)
{
	loff_t size = (loff_t)smm_vault_npages() << PAGE_SHIFT;
	void *data, *scratch;
	ssize_t done = 0;
	int ret = 0;

	if (!smm_authorized())
		return smm_deny(SMM_C_DENY_UID, -EACCES, "write() de UID no autorizado");
	if (!smm_allow_write)
		return smm_deny(SMM_C_DENY_FLAGS, -EPERM, "write() con allow_write=0");
	if (*ppos >= size)
		return -ENOSPC;
	len = min_t(loff_t, len, size - *ppos);

	data = (void *)__get_free_page(GFP_KERNEL);
	scratch = (void *)__get_free_page(GFP_KERNEL);
	if (!data || !scratch) {
		ret = -ENOMEM;
		goto out;
	}

	while (len) {
		unsigned long idx = *ppos >> PAGE_SHIFT;
		size_t off = *ppos & ~PAGE_MASK;
		size_t chunk = min_t(size_t, len, PAGE_SIZE - off);

		/* Copiar del usuario ANTES de tomar el lock de la boveda (evita deadlock). */
		if (copy_from_user(data, ubuf + done, chunk)) {
			ret = -EFAULT;
			break;
		}
		ret = smm_vault_update(idx, off, data, chunk, scratch);
		if (ret) {
			if (ret == -EBADMSG)
				smm_count(SMM_C_INTEGRITY);
			ret = -EIO;
			break;
		}
		*ppos += chunk;
		done += chunk;
		len -= chunk;
	}
out:
	if (data) {
		memzero_explicit(data, PAGE_SIZE);
		free_page((unsigned long)data);
	}
	if (scratch)
		free_page((unsigned long)scratch);
	if (done) {
		smm_count(SMM_C_WRITE);
		smm_event("write() pid=%d uid=%u: %zd bytes cifrados en la boveda", current->pid,
			  smm_caller_uid(), done);
	}
	return done ? done : ret;
}

static loff_t smm_llseek(struct file *filp, loff_t off, int whence)
{
	return fixed_size_llseek(filp, off, whence,
				 (loff_t)smm_vault_npages() << PAGE_SHIFT);
}

static const struct file_operations smm_fops = {
	.owner  = THIS_MODULE,
	.open   = smm_open,
	.read   = smm_read,
	.write  = smm_write,
	.llseek = smm_llseek,
	.mmap   = smm_mmap,
};

/* ------------------------------------------------------------------------- */
/* init / exit                                                               */
/* ------------------------------------------------------------------------- */
static int __init smm_init(void)
{
	int ret;

	ret = smm_vault_init(vault_pages);
	if (ret)
		return ret;

	ret = smm_audit_init();
	if (ret)
		goto err_vault;

	ret = alloc_chrdev_region(&smm_devt, 0, 1, SMM_NAME);
	if (ret)
		goto err_audit;

	cdev_init(&smm_cdev, &smm_fops);
	smm_cdev.owner = THIS_MODULE;
	ret = cdev_add(&smm_cdev, smm_devt, 1);
	if (ret)
		goto err_region;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
	smm_class = class_create(SMM_NAME);
#else
	smm_class = class_create(THIS_MODULE, SMM_NAME);
#endif
	if (IS_ERR(smm_class)) {
		ret = PTR_ERR(smm_class);
		goto err_cdev;
	}

	smm_device = device_create(smm_class, NULL, smm_devt, NULL, SMM_NAME);
	if (IS_ERR(smm_device)) {
		ret = PTR_ERR(smm_device);
		goto err_class;
	}

	smm_kprobe_init();

	pr_info(SMM_TAG "cargado: /dev/%s, allowed_uid=%d, allow_write=%d, %u paginas\n",
		SMM_NAME, smm_allowed_uid, smm_allow_write, vault_pages);
	smm_event("modulo cargado (allowed_uid=%d allow_write=%d paginas=%u)",
		  smm_allowed_uid, smm_allow_write, vault_pages);
	return 0;

err_class:
	class_destroy(smm_class);
err_cdev:
	cdev_del(&smm_cdev);
err_region:
	unregister_chrdev_region(smm_devt, 1);
err_audit:
	smm_audit_exit();
err_vault:
	smm_vault_exit();
	return ret;
}

static void __exit smm_exit(void)
{
	/*
	 * Mientras exista un mapeo, el VMA mantiene una referencia al archivo y
	 * por lo tanto al modulo (cdev.owner): rmmod falla con "in use", asi que
	 * aqui ya no puede quedar ningun smm_map vivo.
	 */
	smm_kprobe_exit();
	device_destroy(smm_class, smm_devt);
	class_destroy(smm_class);
	cdev_del(&smm_cdev);
	unregister_chrdev_region(smm_devt, 1);
	smm_audit_exit();
	smm_vault_exit();
	pr_info(SMM_TAG "descargado; boveda y llave destruidas\n");
}

module_init(smm_init);
module_exit(smm_exit);
