// SPDX-License-Identifier: GPL-2.0
/*
 * smm_bench.c - Medicion de sobrecosto: mmap()+page faults vs read()
 *
 * Uso: smm_bench <archivo|dispositivo> <paginas> <repeticiones> [modo]
 *      modo = mmap_cold | mmap_scan | read_full | all (por defecto all)
 *
 * Modos medidos (cada uno <repeticiones> veces; la 1a se descarta como calentamiento):
 *   mmap_cold : open + mmap + tocar 1 byte por pagina (1 page fault por pagina) + munmap
 *   mmap_scan : igual que mmap_cold pero leyendo TODO el contenido (8 bytes de paso)
 *   read_full : open + read() en bloques de 64 KiB hasta leer todo el archivo
 *
 * Salida: una fila por modo con tiempo promedio, ns/pagina, MB/s y page faults
 * menores (getrusage) por iteracion. Las mismas filas se repiten en formato CSV
 * (prefijo "CSV,") para graficarlas en el informe.
 */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <time.h>
#include <unistd.h>

static long PG;

static uint64_t ns_now(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

static long minflt(void)
{
	struct rusage ru;

	getrusage(RUSAGE_SELF, &ru);
	return ru.ru_minflt;
}

static volatile uint64_t sink;

static int run_mmap(const char *path, long pages, int scan, uint64_t *ns, long *flt)
{
	uint64_t t0, sum = 0;
	long f0, i;
	size_t len = (size_t)pages * PG;
	int fd;
	char *p;

	f0 = minflt();
	t0 = ns_now();
	fd = open(path, O_RDONLY);
	if (fd < 0)
		return -1;
	p = mmap(NULL, len, PROT_READ, MAP_SHARED, fd, 0);
	if (p == MAP_FAILED) {
		close(fd);
		return -1;
	}
	if (scan) {
		for (i = 0; i < (long)(len / sizeof(uint64_t)); i++)
			sum += ((volatile uint64_t *)p)[i];
	} else {
		for (i = 0; i < pages; i++)
			sum += ((volatile char *)p)[i * PG];
	}
	munmap(p, len);
	close(fd);
	*ns = ns_now() - t0;
	*flt = minflt() - f0;
	sink = sum;
	return 0;
}

static int run_read(const char *path, long pages, uint64_t *ns, long *flt)
{
	static char buf[64 * 1024];
	size_t total = (size_t)pages * PG, done = 0;
	uint64_t t0;
	long f0;
	ssize_t n;
	int fd;

	f0 = minflt();
	t0 = ns_now();
	fd = open(path, O_RDONLY);
	if (fd < 0)
		return -1;
	while (done < total) {
		size_t want = total - done < sizeof(buf) ? total - done : sizeof(buf);

		n = read(fd, buf, want);
		if (n <= 0)
			break;
		done += n;
	}
	close(fd);
	*ns = ns_now() - t0;
	*flt = minflt() - f0;
	sink = buf[0];
	return done == total ? 0 : -1;
}

int main(int argc, char **argv)
{
	const char *modes[] = { "mmap_cold", "mmap_scan", "read_full" };
	const char *path, *only = "all";
	long pages, reps, m, r;

	PG = sysconf(_SC_PAGESIZE);
	if (argc < 4) {
		fprintf(stderr, "Uso: %s <archivo|dispositivo> <paginas> <repeticiones> [modo]\n", argv[0]);
		return 2;
	}
	path = argv[1];
	pages = atol(argv[2]);
	reps = atol(argv[3]);
	if (argc > 4)
		only = argv[4];
	if (pages <= 0 || reps < 2) {
		fprintf(stderr, "paginas > 0 y repeticiones >= 2\n");
		return 2;
	}

	printf("Objetivo: %s | %ld paginas (%ld KiB) | %ld repeticiones (1 de calentamiento)\n",
	       path, pages, pages * PG / 1024, reps);
	printf("%-10s %14s %12s %10s %14s\n", "modo", "prom (us)", "ns/pagina", "MB/s",
	       "faults/iter");

	for (m = 0; m < 3; m++) {
		uint64_t ns, tot_ns = 0;
		long flt, tot_flt = 0;

		if (strcmp(only, "all") && strcmp(only, modes[m]))
			continue;
		for (r = 0; r < reps; r++) {
			int ret = m == 2 ? run_read(path, pages, &ns, &flt)
					 : run_mmap(path, pages, m == 1, &ns, &flt);

			if (ret) {
				printf("%-10s ERROR: %s\n", modes[m], strerror(errno));
				break;
			}
			if (r == 0)
				continue;
			tot_ns += ns;
			tot_flt += flt;
		}
		if (r < reps)
			continue;
		{
			double avg_ns = (double)tot_ns / (reps - 1);
			double mbs = (pages * PG / 1e6) / (avg_ns / 1e9);

			printf("%-10s %14.1f %12.1f %10.1f %14.1f\n", modes[m], avg_ns / 1e3,
			       avg_ns / pages, mbs, (double)tot_flt / (reps - 1));
			printf("CSV,%s,%s,%ld,%.1f,%.1f,%.1f,%.1f\n", path, modes[m], pages,
			       avg_ns / 1e3, avg_ns / pages, mbs, (double)tot_flt / (reps - 1));
		}
	}
	return 0;
}
