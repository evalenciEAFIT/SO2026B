/* SPDX-License-Identifier: GPL-2.0 */
/*
 * smm.h - Declaraciones comunes del modulo secure_mmap
 *
 * Parcial 3 - Sistemas Operativos (EAFIT 2026-2)
 * Seguridad de archivos mediante intervencion en la capa del kernel y mmap()
 *
 * El modulo se divide en cuatro unidades:
 *   smm_main.c   -> dispositivo /dev/secure_mmap, politica de mmap(), vm_ops (.fault)
 *   smm_vault.c  -> "boveda": paginas cifradas con AES-CTR + checksum de integridad
 *   smm_audit.c  -> contadores, bitacora de eventos y /proc/secure_mmap
 *   smm_kprobe.c -> auditoria de mmap() sobre archivos REALES via kprobe en
 *                   security_mmap_file (el mismo punto donde actuan los LSM)
 */
#ifndef SMM_H
#define SMM_H

#include <linux/types.h>
#include <linux/mm.h>
#include <linux/version.h>

#define SMM_NAME "secure_mmap"
#define SMM_TAG  "[secure_mmap] "

/* ------------------------------------------------------------------------- */
/* Compatibilidad 5.x / 6.x                                                  */
/* ------------------------------------------------------------------------- */
/*
 * Desde Linux 6.3 vma->vm_flags es de solo lectura (const) y DEBE modificarse
 * con vm_flags_set()/vm_flags_clear(), que ademas toman el lock por-VMA.
 * En kernels 5.x se modifica el campo directamente.
 */
static inline void smm_vm_flags_set(struct vm_area_struct *vma, vm_flags_t f)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 3, 0)
	vm_flags_set(vma, f);
#else
	vma->vm_flags |= f;
#endif
}

static inline void smm_vm_flags_clear(struct vm_area_struct *vma, vm_flags_t f)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 3, 0)
	vm_flags_clear(vma, f);
#else
	vma->vm_flags &= ~f;
#endif
}

/* ------------------------------------------------------------------------- */
/* Parametros del modulo (definidos en smm_main.c)                           */
/* ------------------------------------------------------------------------- */
extern int smm_allowed_uid;      /* unico UID autorizado a mapear/leer        */
extern bool smm_allow_write;     /* permite MAP_SHARED + PROT_WRITE           */
extern bool smm_verbose;         /* registra cada page fault en dmesg         */

/* ------------------------------------------------------------------------- */
/* Boveda cifrada (smm_vault.c)                                              */
/* ------------------------------------------------------------------------- */
int smm_vault_init(unsigned int npages);
void smm_vault_exit(void);
unsigned int smm_vault_npages(void);
/* Descifra la pagina idx en dst (PAGE_SIZE). -EBADMSG si falla integridad. */
int smm_vault_decrypt_page(unsigned long idx, void *dst);
/* Cifra src (PAGE_SIZE) como nuevo contenido de la pagina idx. */
int smm_vault_encrypt_page(unsigned long idx, const void *src);
/* true si el texto plano src difiere del ultimo contenido cifrado de idx. */
bool smm_vault_page_changed(unsigned long idx, const void *src);
/* Lectura-modificacion-escritura atomica de un rango dentro de una pagina. */
int smm_vault_update(unsigned long idx, size_t off, const void *src,
		     size_t len, void *scratch);
/* Simula manipulacion del texto cifrado (XOR 0xFF: aplicar dos veces revierte). */
int smm_vault_corrupt(unsigned long idx);

/* ------------------------------------------------------------------------- */
/* Auditoria (smm_audit.c)                                                   */
/* ------------------------------------------------------------------------- */
enum smm_counter {
	SMM_C_OPEN = 0,
	SMM_C_MMAP_OK,
	SMM_C_DENY_UID,
	SMM_C_DENY_FLAGS,
	SMM_C_DENY_RANGE,
	SMM_C_FAULTS,
	SMM_C_FAULT_REUSE,
	SMM_C_INTEGRITY,
	SMM_C_REENCRYPT,
	SMM_C_READ,
	SMM_C_WRITE,
	SMM_C_SPLIT_DENIED,
	SMM_C_KPROBE_HITS,
	SMM_C_KPROBE_ALERTS,
	SMM_C_MAX
};

void smm_count(enum smm_counter c);
void smm_fault_time(u64 ns);
/* Registra un evento en la bitacora circular (/proc) y en dmesg. */
__printf(1, 2) void smm_event(const char *fmt, ...);
int smm_audit_init(void);
void smm_audit_exit(void);

/* ------------------------------------------------------------------------- */
/* Kprobe sobre security_mmap_file (smm_kprobe.c)                            */
/* ------------------------------------------------------------------------- */
int smm_kprobe_init(void);
void smm_kprobe_exit(void);
bool smm_kprobe_active(void);

#endif /* SMM_H */
