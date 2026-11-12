// SPDX-License-Identifier: GPL-2.0
/*
 * guard_preload.c - "Defensa" ingenua en ESPACIO DE USUARIO (contraejemplo)
 *
 * Biblioteca para LD_PRELOAD que intercepta mmap()/mmap64() de libc y niega
 * el mapeo de archivos cuya ruta contenga GUARD_PATTERN (por defecto "secreto"
 * o "secure_mmap").
 *
 * Existe para DEMOSTRAR que este tipo de defensa es trivialmente evadible:
 * basta con invocar la syscall directamente (syscall(SYS_mmap, ...)), con un
 * binario estatico, o simplemente no exportar LD_PRELOAD. Ver
 * smm_attacks raw_bypass y scripts/run_attacks.sh (prueba T04/T05).
 *
 *   LD_PRELOAD=build/guard_preload.so ./build/smm_attacks raw_bypass /tmp/secreto.txt
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

typedef void *(*mmap_fn)(void *, size_t, int, int, int, off_t);

static int guarded(int fd, char *path, size_t len)
{
	char link[64];
	const char *pat = getenv("GUARD_PATTERN");
	ssize_t n;

	if (fd < 0)
		return 0;
	snprintf(link, sizeof(link), "/proc/self/fd/%d", fd);
	n = readlink(link, path, len - 1);
	if (n < 0)
		return 0;
	path[n] = '\0';
	if (pat)
		return strstr(path, pat) != NULL;
	return strstr(path, "secreto") || strstr(path, "secure_mmap");
}

static void *guard(void *addr, size_t len, int prot, int flags, int fd, off_t off,
		   const char *sym)
{
	static mmap_fn real;
	char path[PATH_MAX];

	if (!real)
		real = (mmap_fn)dlsym(RTLD_NEXT, sym);
	if (guarded(fd, path, sizeof(path))) {
		fprintf(stderr, "[guard LD_PRELOAD] %s() de %s BLOQUEADO en espacio de usuario\n",
			sym, path);
		errno = EACCES;
		return MAP_FAILED;
	}
	return real(addr, len, prot, flags, fd, off);
}

void *mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off)
{
	return guard(addr, len, prot, flags, fd, off, "mmap");
}

void *mmap64(void *addr, size_t len, int prot, int flags, int fd, off64_t off)
{
	return guard(addr, len, prot, flags, fd, (off_t)off, "mmap64");
}
