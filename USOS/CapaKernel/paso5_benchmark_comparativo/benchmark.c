/**
 * @file benchmark.c
 * @brief PASO 5: Suite de Benchmarks Comparativos (read vs mmap) con Salida a Color
 * @course Sistemas Operativos - DOCENCIA 2026-B
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <time.h>
#include <errno.h>

#define BENCHMARK_FILE  "bench_data.bin"
#define FILE_SIZE_MB    64
#define FILE_SIZE_BYTES ((size_t)FILE_SIZE_MB * 1024 * 1024)

/* Códigos de Color ANSI */
#define CLR_RESET   "\033[0m"
#define CLR_BOLD    "\033[1m"
#define CLR_RED     "\033[31m"
#define CLR_GREEN   "\033[32m"
#define CLR_YELLOW  "\033[33m"
#define CLR_BLUE    "\033[34m"
#define CLR_MAGENTA "\033[35m"
#define CLR_CYAN    "\033[36m"

typedef struct {
    double real_time;
    double user_cpu;
    double sys_cpu;
    long minflt;
    long majflt;
    double throughput;
    unsigned long long checksum;
} BenchResult;

static double timeval_to_sec(struct timeval tv) {
    return (double)tv.tv_sec + (double)tv.tv_usec / 1e6;
}

static double get_real_time(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static void prepare_test_file(const char *path, size_t size) {
    struct stat st;
    if (stat(path, &st) == 0 && (size_t)st.st_size == size) {
        return;
    }

    printf("%s[*] Generando archivo de benchmark (%d MB)...%s\n", CLR_CYAN, FILE_SIZE_MB, CLR_RESET);
    int fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror(CLR_RED "open" CLR_RESET);
        exit(EXIT_FAILURE);
    }

    if (ftruncate(fd, size) != 0) {
        perror(CLR_RED "ftruncate" CLR_RESET);
        close(fd);
        exit(EXIT_FAILURE);
    }

    char *map = (char *)mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (map != MAP_FAILED) {
        for (size_t i = 0; i < size; i += 4096) {
            map[i] = (char)(i & 0x7F);
        }
        msync(map, size, MS_SYNC);
        munmap(map, size);
    }
    close(fd);
    printf("%s[+] Archivo preparado.%s\n\n", CLR_GREEN, CLR_RESET);
}

static BenchResult bench_read(const char *path, size_t block_size) {
    BenchResult res;
    memset(&res, 0, sizeof(res));

    char *buf = (char *)malloc(block_size);
    if (!buf) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        perror("open");
        free(buf);
        exit(EXIT_FAILURE);
    }

    posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL);

    struct rusage ru_start, ru_end;
    getrusage(RUSAGE_SELF, &ru_start);
    double t_start = get_real_time();

    unsigned long long checksum = 0;
    size_t total_bytes = 0;

    while (1) {
        ssize_t bytes = read(fd, buf, block_size);
        if (bytes <= 0) break;
        total_bytes += bytes;
        for (ssize_t i = 0; i < bytes; i += 64) {
            checksum += (unsigned char)buf[i];
        }
    }

    double t_end = get_real_time();
    getrusage(RUSAGE_SELF, &ru_end);

    close(fd);
    free(buf);

    res.real_time = t_end - t_start;
    res.user_cpu = timeval_to_sec(ru_end.ru_utime) - timeval_to_sec(ru_start.ru_utime);
    res.sys_cpu = timeval_to_sec(ru_end.ru_stime) - timeval_to_sec(ru_start.ru_stime);
    res.minflt = ru_end.ru_minflt - ru_start.ru_minflt;
    res.majflt = ru_end.ru_majflt - ru_start.ru_majflt;
    res.throughput = ((double)total_bytes / (1024.0 * 1024.0)) / res.real_time;
    res.checksum = checksum;

    return res;
}

static BenchResult bench_mmap(const char *path, size_t file_size) {
    BenchResult res;
    memset(&res, 0, sizeof(res));

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    struct rusage ru_start, ru_end;
    getrusage(RUSAGE_SELF, &ru_start);
    double t_start = get_real_time();

    char *map = (char *)mmap(NULL, file_size, PROT_READ, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) {
        perror("mmap");
        close(fd);
        exit(EXIT_FAILURE);
    }

    madvise(map, file_size, MADV_SEQUENTIAL);

    unsigned long long checksum = 0;
    for (size_t i = 0; i < file_size; i += 64) {
        checksum += (unsigned char)map[i];
    }

    munmap(map, file_size);
    double t_end = get_real_time();
    getrusage(RUSAGE_SELF, &ru_end);

    close(fd);

    res.real_time = t_end - t_start;
    res.user_cpu = timeval_to_sec(ru_end.ru_utime) - timeval_to_sec(ru_start.ru_utime);
    res.sys_cpu = timeval_to_sec(ru_end.ru_stime) - timeval_to_sec(ru_start.ru_stime);
    res.minflt = ru_end.ru_minflt - ru_start.ru_minflt;
    res.majflt = ru_end.ru_majflt - ru_start.ru_majflt;
    res.throughput = ((double)file_size / (1024.0 * 1024.0)) / res.real_time;
    res.checksum = checksum;

    return res;
}

int main(void) {
    prepare_test_file(BENCHMARK_FILE, FILE_SIZE_BYTES);

    printf("%s========================================================================================%s\n", 
           CLR_CYAN, CLR_RESET);
    printf("%s%s   PASO 5: COMPARATIVA RIGUROSA DE RENDIMIENTO (read 4K vs read 64K vs mmap Zero-Copy)     %s\n",
           CLR_BOLD, CLR_BLUE, CLR_RESET);
    printf("   Tamaño del archivo de prueba: %s%d MB%s (%zu bytes)\n",
           CLR_BOLD, FILE_SIZE_MB, CLR_RESET, FILE_SIZE_BYTES);
    printf("%s========================================================================================%s\n", 
           CLR_CYAN, CLR_RESET);
    printf("%s%s[PROPÓSITO]%s\n", CLR_GREEN, CLR_BOLD, CLR_RESET);
    printf("  Comparar empíricamente el costo computacional de las llamadas al sistema y copias en kernel\n");
    printf("  frente al acceso directo mediante memoria virtual mapeada (Zero-Copy).\n\n");
    printf("%s%s[ESTRATEGIA TÉCNICA]%s\n", CLR_YELLOW, CLR_BOLD, CLR_RESET);
    printf("  1. Se ejecutan 3 métodos sobre el mismo conjunto de datos de 64 MB:\n");
    printf("     - read() con bloques de 4 KB (16,384 syscalls, 16,384 copias de memoria).\n");
    printf("     - read() con bloques de 64 KB (1,024 syscalls, menos cambios pero persiste la copia).\n");
    printf("     - mmap() Zero-Copy (1 sola syscall, resolución por hardware MMU mediante Page Fault).\n");
    printf("  2. Métricas del subsistema de recursos del kernel (getrusage): tiempo de CPU en usuario (Ring 3),\n");
    printf("     tiempo de CPU en kernel (Ring 0, sys_cpu) y fallos de página menores/mayores.\n");
    printf("%s========================================================================================%s\n\n", 
           CLR_CYAN, CLR_RESET);

    printf("%s[*] Realizando calentamiento de la Page Cache...%s\n", CLR_YELLOW, CLR_RESET);
    bench_read(BENCHMARK_FILE, 64 * 1024);

    printf("%s[*] Ejecutando Prueba 1: read() con bloques de 4 KB...%s\n", CLR_CYAN, CLR_RESET);
    BenchResult r_4k = bench_read(BENCHMARK_FILE, 4 * 1024);

    printf("%s[*] Ejecutando Prueba 2: read() con bloques de 64 KB...%s\n", CLR_CYAN, CLR_RESET);
    BenchResult r_64k = bench_read(BENCHMARK_FILE, 64 * 1024);

    printf("%s[*] Ejecutando Prueba 3: mmap() (Zero-Copy)...%s\n", CLR_GREEN, CLR_RESET);
    BenchResult r_mmap = bench_mmap(BENCHMARK_FILE, FILE_SIZE_BYTES);

    /* Mostrar tabla comparativa con formato enriquecido */
    printf("\n");
    printf("%s+---------------------------------+-------------+-------------+-------------+%s\n", CLR_CYAN, CLR_RESET);
    printf("%s| %-31s | %-11s | %-11s | %-11s |%s\n", 
           CLR_CYAN, "Métrica del Kernel", "read (4KB)", "read (64KB)", "mmap (Zero)", CLR_RESET);
    printf("%s+---------------------------------+-------------+-------------+-------------+%s\n", CLR_CYAN, CLR_RESET);
    printf("| %-31s | %11.5f | %11.5f | %s%11.5f%s |\n",
           "Tiempo Real (segundos)", r_4k.real_time, r_64k.real_time, CLR_GREEN, r_mmap.real_time, CLR_RESET);
    printf("| %-31s | %11.5f | %11.5f | %11.5f |\n",
           "CPU Usuario (Ring 3) seg", r_4k.user_cpu, r_64k.user_cpu, r_mmap.user_cpu);
    printf("| %-31s | %s%11.5f%s | %11.5f | %s%s%11.5f%s |\n",
           "CPU Kernel (Ring 0) seg", CLR_RED, r_4k.sys_cpu, CLR_RESET, r_64k.sys_cpu, CLR_GREEN, CLR_BOLD, r_mmap.sys_cpu, CLR_RESET);
    printf("| %-31s | %11ld | %11ld | %11ld |\n",
           "Minor Page Faults (MMU)", r_4k.minflt, r_64k.minflt, r_mmap.minflt);
    printf("| %-31s | %11ld | %11ld | %11ld |\n",
           "Major Page Faults (I/O disco)", r_4k.majflt, r_64k.majflt, r_mmap.majflt);
    printf("| %-31s | %11.2f | %11.2f | %s%s%11.2f%s |\n",
           "Throughput (MB/s)", r_4k.throughput, r_64k.throughput, CLR_GREEN, CLR_BOLD, r_mmap.throughput, CLR_RESET);
    printf("%s+---------------------------------+-------------+-------------+-------------+%s\n", CLR_CYAN, CLR_RESET);

    double factor = r_mmap.throughput / r_4k.throughput;
    printf("\n%s>>> CONCLUSIÓN EMPÍRICA:%s\n", CLR_YELLOW, CLR_RESET);
    printf("    %smmap()%s fue %s%s%.2fx veces más rápido%s que %sread()%s con bloques de 4KB.\n", 
           CLR_GREEN, CLR_RESET, CLR_BOLD, CLR_GREEN, factor, CLR_RESET, CLR_RED, CLR_RESET);
    printf("    El tiempo de CPU en Kernel (%ssys_cpu%s) con mmap es significativamente menor\n", CLR_BOLD, CLR_RESET);
    printf("    porque se elimina %scopy_to_user()%s y los continuos cambios de contexto.\n", CLR_RED, CLR_RESET);
    printf("%s========================================================================================%s\n", 
           CLR_CYAN, CLR_RESET);

    return EXIT_SUCCESS;
}
