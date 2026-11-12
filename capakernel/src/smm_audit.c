// SPDX-License-Identifier: GPL-2.0
/*
 * smm_audit.c - Auditoria: contadores atomicos, bitacora circular y /proc
 *
 *   cat /proc/secure_mmap                -> estado, contadores, latencia de faults
 *                                           y ultimos 32 eventos de seguridad
 *   echo "corrupt N" > /proc/secure_mmap -> (root) altera 1 byte cifrado de la
 *                                           pagina N para probar la integridad;
 *                                           repetirlo revierte el cambio
 *   echo reset > /proc/secure_mmap       -> (root) reinicia contadores
 *
 * smm_event() puede llamarse desde el kprobe (contexto atomico), por eso la
 * bitacora usa un spinlock con IRQs deshabilitadas y buffers estaticos (no
 * reserva memoria).
 */
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/atomic.h>
#include <linux/capability.h>
#include <linux/ktime.h>
#include <linux/math64.h>
#include <linux/string.h>

#include "smm.h"

#define SMM_EV_N   32
#define SMM_EV_LEN 192

static atomic64_t counters[SMM_C_MAX];
static atomic64_t fault_ns_total;
static atomic64_t fault_ns_max;

static char ev_ring[SMM_EV_N][SMM_EV_LEN];
static unsigned int ev_head;   /* proxima posicion a escribir */
static unsigned int ev_count;
static DEFINE_SPINLOCK(ev_lock);

static const char *const counter_names[SMM_C_MAX] = {
	[SMM_C_OPEN]          = "open()",
	[SMM_C_MMAP_OK]       = "mmap() permitidos",
	[SMM_C_DENY_UID]      = "DENEGADO por UID",
	[SMM_C_DENY_FLAGS]    = "DENEGADO por banderas",
	[SMM_C_DENY_RANGE]    = "DENEGADO por rango",
	[SMM_C_FAULTS]        = "page faults atendidos",
	[SMM_C_FAULT_REUSE]   = "faults reusando pagina",
	[SMM_C_INTEGRITY]     = "VIOLACIONES integridad",
	[SMM_C_REENCRYPT]     = "paginas re-cifradas",
	[SMM_C_READ]          = "read() autorizados",
	[SMM_C_WRITE]         = "write() autorizados",
	[SMM_C_SPLIT_DENIED]  = "split de VMA negado",
	[SMM_C_KPROBE_HITS]   = "kprobe: mmap archivo vigilado",
	[SMM_C_KPROBE_ALERTS] = "kprobe: ALERTAS (W/X)",
};

void smm_count(enum smm_counter c)
{
	if (c < SMM_C_MAX)
		atomic64_inc(&counters[c]);
}

void smm_fault_time(u64 ns)
{
	s64 old = atomic64_read(&fault_ns_max);

	atomic64_add(ns, &fault_ns_total);
	while ((s64)ns > old) {
		s64 prev = atomic64_cmpxchg(&fault_ns_max, old, ns);

		if (prev == old)
			break;
		old = prev;
	}
}

void smm_event(const char *fmt, ...)
{
	char *slot;
	unsigned long flags;
	va_list args;
	u64 t = ktime_get_ns();
	u32 rem;
	u64 sec = div_u64_rem(t, NSEC_PER_SEC, &rem);
	int n;

	spin_lock_irqsave(&ev_lock, flags);
	slot = ev_ring[ev_head];
	n = scnprintf(slot, SMM_EV_LEN, "[%5llu.%06u] ", sec, rem / 1000);
	va_start(args, fmt);
	vscnprintf(slot + n, SMM_EV_LEN - n, fmt, args);
	va_end(args);
	ev_head = (ev_head + 1) % SMM_EV_N;
	if (ev_count < SMM_EV_N)
		ev_count++;
	spin_unlock_irqrestore(&ev_lock, flags);

	/* Copia en dmesg (limitada para que un atacante no inunde el log). */
	printk_ratelimited(KERN_NOTICE SMM_TAG "AUDIT %s\n", slot + n);
}

static int smm_proc_show(struct seq_file *m, void *v)
{
	unsigned long flags;
	unsigned int i, start;
	u64 faults = atomic64_read(&counters[SMM_C_FAULTS]);
	u64 total = atomic64_read(&fault_ns_total);
	static char snapshot[SMM_EV_N][SMM_EV_LEN];
	static DEFINE_MUTEX(snap_lock);
	unsigned int count;

	seq_puts(m, "================ secure_mmap: estado y auditoria ================\n");
	seq_printf(m, "allowed_uid            : %d\n", smm_allowed_uid);
	seq_printf(m, "allow_write            : %s\n", smm_allow_write ? "si" : "no");
	seq_printf(m, "verbose                : %s\n", smm_verbose ? "si" : "no");
	seq_printf(m, "paginas en la boveda   : %u (%u KiB)\n", smm_vault_npages(),
		   smm_vault_npages() * (unsigned int)(PAGE_SIZE / 1024));
	seq_printf(m, "kprobe security_mmap_file: %s\n",
		   smm_kprobe_active() ? "activo" : "inactivo");
	seq_puts(m, "------------------------------ contadores ------------------------\n");
	for (i = 0; i < SMM_C_MAX; i++)
		seq_printf(m, "%-30s : %lld\n", counter_names[i],
			   (long long)atomic64_read(&counters[i]));
	seq_puts(m, "------------------- latencia de smm_fault (kernel) ---------------\n");
	seq_printf(m, "promedio (ns)          : %llu\n",
		   faults ? div64_u64(total, faults) : 0ULL);
	seq_printf(m, "maximo (ns)            : %lld\n",
		   (long long)atomic64_read(&fault_ns_max));
	seq_puts(m, "----------------------- ultimos eventos ---------------------------\n");

	/* Copiamos la bitacora bajo el spinlock y la imprimimos fuera de el. */
	mutex_lock(&snap_lock);
	spin_lock_irqsave(&ev_lock, flags);
	count = ev_count;
	start = (ev_head + SMM_EV_N - ev_count) % SMM_EV_N;
	for (i = 0; i < count; i++)
		memcpy(snapshot[i], ev_ring[(start + i) % SMM_EV_N], SMM_EV_LEN);
	spin_unlock_irqrestore(&ev_lock, flags);
	for (i = 0; i < count; i++)
		seq_printf(m, "%s\n", snapshot[i]);
	mutex_unlock(&snap_lock);
	if (!count)
		seq_puts(m, "(sin eventos)\n");
	return 0;
}

static int smm_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, smm_proc_show, NULL);
}

static ssize_t smm_proc_write(struct file *file, const char __user *ubuf,
			      size_t len, loff_t *ppos)
{
	char buf[32];
	unsigned int idx;
	size_t n = min(len, sizeof(buf) - 1);
	int i;

	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	if (copy_from_user(buf, ubuf, n))
		return -EFAULT;
	buf[n] = '\0';
	strim(buf);

	if (sscanf(buf, "corrupt %u", &idx) == 1) {
		int ret = smm_vault_corrupt(idx);

		if (ret)
			return ret;
		smm_event("ADMIN pid=%d altero 1 byte cifrado de la pagina %u (simulacion de manipulacion)",
			  current->pid, idx);
		return len;
	}
	if (!strcmp(buf, "reset")) {
		for (i = 0; i < SMM_C_MAX; i++)
			atomic64_set(&counters[i], 0);
		atomic64_set(&fault_ns_total, 0);
		atomic64_set(&fault_ns_max, 0);
		return len;
	}
	return -EINVAL;
}

static const struct proc_ops smm_proc_ops = {
	.proc_open    = smm_proc_open,
	.proc_read    = seq_read,
	.proc_lseek   = seq_lseek,
	.proc_release = single_release,
	.proc_write   = smm_proc_write,
};

int smm_audit_init(void)
{
	if (!proc_create(SMM_NAME, 0644, NULL, &smm_proc_ops))
		return -ENOMEM;
	return 0;
}

void smm_audit_exit(void)
{
	remove_proc_entry(SMM_NAME, NULL);
}
