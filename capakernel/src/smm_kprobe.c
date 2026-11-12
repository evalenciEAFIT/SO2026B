// SPDX-License-Identifier: GPL-2.0
/*
 * smm_kprobe.c - Auditoria de mmap() sobre archivos REALES del sistema
 *
 * La boveda (/dev/secure_mmap) protege un archivo que el modulo controla.
 * Para archivos ordinarios (ext4, tmpfs, ...) no podemos reemplazar sus
 * file_operations, pero si podemos OBSERVAR cada mmap() en el mismo punto
 * por donde pasan los Linux Security Modules: security_mmap_file().
 *
 *     sys_mmap -> ksys_mmap_pgoff -> vm_mmap_pgoff
 *              -> security_mmap_file(file, prot, flags)   <-- kprobe aqui
 *              -> do_mmap -> mmap_region -> file->f_op->mmap
 *
 * Por que kprobe y no un LSM:
 *   - Desde Linux 2.6.24 los LSM no se pueden cargar como modulo (deben estar
 *     compilados en el kernel). Un kprobe se instala en caliente.
 *   - Limitacion: un kprobe pre_handler NO puede cambiar el valor de retorno,
 *     asi que aqui solo AUDITAMOS. Para BLOQUEAR archivos reales sin recompilar
 *     el kernel la opcion es BPF-LSM (programa eBPF en lsm/mmap_file), que
 *     queda como trabajo futuro (ver README).
 *
 * El pre_handler corre con la apropiacion (preemption) deshabilitada: no puede
 * dormir ni reservar memoria; solo lee punteros y registra el evento.
 */
#include <linux/module.h>
#include <linux/kprobes.h>
#include <linux/fs.h>
#include <linux/dcache.h>
#include <linux/mman.h>
#include <linux/cred.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/ptrace.h>

#include "smm.h"

static char *watch_name = "secreto.txt";
module_param(watch_name, charp, 0444);
MODULE_PARM_DESC(watch_name,
		 "Nombre (basename) del archivo real a vigilar con el kprobe (por defecto secreto.txt)");

/* Ojo: "enable_kprobe" ya es una funcion del kernel (linux/kprobes.h). */
static bool use_kprobe = true;
module_param(use_kprobe, bool, 0444);
MODULE_PARM_DESC(use_kprobe, "Instalar kprobe en security_mmap_file (por defecto si)");

static bool kp_registered;

static int smm_kp_pre(struct kprobe *p, struct pt_regs *regs)
{
	/* int security_mmap_file(struct file *file, unsigned long prot, unsigned long flags) */
	struct file *file = (struct file *)regs_get_kernel_argument(regs, 0);
	unsigned long prot = regs_get_kernel_argument(regs, 1);
	unsigned long flags = regs_get_kernel_argument(regs, 2);
	const unsigned char *name;
	bool shared_write, exec;

	if (!file || !watch_name || !*watch_name)
		return 0;

	name = file->f_path.dentry->d_name.name;
	if (!name || strcmp((const char *)name, watch_name))
		return 0;

	smm_count(SMM_C_KPROBE_HITS);
	shared_write = (prot & PROT_WRITE) && (flags & MAP_SHARED);
	exec = prot & PROT_EXEC;

	if (shared_write || exec) {
		smm_count(SMM_C_KPROBE_ALERTS);
		smm_event("ALERTA kprobe: %s pid=%d uid=%u mmap(%s) prot=%s%s%s flags=%s",
			  current->comm, current->pid,
			  from_kuid_munged(&init_user_ns, current_euid()), name,
			  (prot & PROT_READ) ? "R" : "-", (prot & PROT_WRITE) ? "W" : "-",
			  (prot & PROT_EXEC) ? "X" : "-",
			  (flags & MAP_SHARED) ? "SHARED" : "PRIVATE");
	} else {
		smm_event("kprobe: %s pid=%d uid=%u mmap(%s) prot=%s%s%s flags=%s",
			  current->comm, current->pid,
			  from_kuid_munged(&init_user_ns, current_euid()), name,
			  (prot & PROT_READ) ? "R" : "-", (prot & PROT_WRITE) ? "W" : "-",
			  (prot & PROT_EXEC) ? "X" : "-",
			  (flags & MAP_SHARED) ? "SHARED" : "PRIVATE");
	}
	return 0;
}

static struct kprobe smm_kp = {
	.symbol_name = "security_mmap_file",
	.pre_handler = smm_kp_pre,
};

int smm_kprobe_init(void)
{
	int ret;

	if (!use_kprobe)
		return 0;
	ret = register_kprobe(&smm_kp);
	if (ret) {
		/* No es fatal: la boveda sigue funcionando sin el kprobe. */
		pr_warn(SMM_TAG "no se pudo instalar kprobe en security_mmap_file (%d)\n", ret);
		return 0;
	}
	kp_registered = true;
	pr_info(SMM_TAG "kprobe en security_mmap_file activo; vigilando '%s'\n", watch_name);
	return 0;
}

void smm_kprobe_exit(void)
{
	if (kp_registered) {
		unregister_kprobe(&smm_kp);
		kp_registered = false;
	}
}

bool smm_kprobe_active(void)
{
	return kp_registered;
}
