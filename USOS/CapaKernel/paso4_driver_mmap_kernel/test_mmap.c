/**
 * @file test_mmap.c
 * @brief Programa de usuario para probar el driver Zero-Copy con mmap()
 * @course Sistemas Operativos - DOCENCIA 2026-B
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <time.h>
#include <errno.h>

#define DEVICE_PATH   "/dev/demo_mmap"
#define MAPPING_SIZE  (64 * 1024)   /* 64 KB (16 páginas de 4 KB) */

/* Códigos de Color ANSI */
#define CLR_RESET   "\033[0m"
#define CLR_BOLD    "\033[1m"
#define CLR_RED     "\033[31m"
#define CLR_GREEN   "\033[32m"
#define CLR_YELLOW  "\033[33m"
#define CLR_BLUE    "\033[34m"
#define CLR_MAGENTA "\033[35m"
#define CLR_CYAN    "\033[36m"

static double get_time_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

int main(void) {
    printf("%s================================================================================%s\n", CLR_CYAN, CLR_RESET);
    printf("%s%s PASO 4: CAPA KERNEL OPTIMIZADA CON dev_mmap() Y remap_pfn_range()            %s\n", CLR_BOLD, CLR_BLUE, CLR_RESET);
    printf(" Dispositivo objetivo: %s%s%s%s\n", CLR_BOLD, CLR_YELLOW, DEVICE_PATH, CLR_RESET);
    printf("%s================================================================================%s\n", CLR_CYAN, CLR_RESET);
    printf("%s%s[PROPÓSITO]%s\n", CLR_GREEN, CLR_BOLD, CLR_RESET);
    printf("  Programar en la Capa Kernel la función .mmap para habilitar comunicación\n");
    printf("  bidireccional de ultra-alto rendimiento directamente en memoria física (Zero-Copy).\n\n");
    printf("%s%s[ESTRATEGIA TÉCNICA]%s\n", CLR_YELLOW, CLR_BOLD, CLR_RESET);
    printf("  1. El módulo reserva páginas contiguas con alloc_pages() y las marca con SetPageReserved().\n");
    printf("  2. En dev_mmap(), remap_pfn_range() reprograma la tabla de páginas del usuario.\n");
    printf("  3. El usuario lee y escribe directamente en la memoria física del kernel sin syscalls.\n");
    printf("%s================================================================================%s\n\n", CLR_CYAN, CLR_RESET);

    /* 1. Abrir descriptor de archivo del dispositivo */
    int fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "%s[-] No se pudo abrir %s: %s%s\n", CLR_RED, DEVICE_PATH, strerror(errno), CLR_RESET);
        fprintf(stderr, "    %s¿Cargó el módulo del kernel? (sudo ./scripts/load_modules.sh)%s\n", CLR_YELLOW, CLR_RESET);
        return EXIT_FAILURE;
    }

    /* 2. Invocar sys_mmap para mapear la memoria física del kernel */
    printf("%s[*] Invocando mmap() sobre el dispositivo...%s\n", CLR_CYAN, CLR_RESET);
    char *shared_mem = (char *)mmap(
        NULL,
        MAPPING_SIZE,
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        fd,
        0
    );

    if (shared_mem == MAP_FAILED) {
        perror(CLR_RED "[-] Error en mmap" CLR_RESET);
        close(fd);
        return EXIT_FAILURE;
    }

    printf("%s[+] ¡Mapeo exitoso! Puntero virtual de usuario: %s%p%s\n\n", 
           CLR_GREEN, CLR_BOLD, (void *)shared_mem, CLR_RESET);

    /* 3. LECTURA ZERO-COPY: Leer directamente desde la memoria del kernel */
    printf("%s[1] LECTURA ZERO-COPY DESDE LA MEMORIA DEL KERNEL:%s\n", CLR_YELLOW, CLR_RESET);
    printf("%s--------------------------------------------------------------%s\n", CLR_CYAN, CLR_RESET);
    printf("%s%s%s", CLR_BOLD, shared_mem, CLR_RESET);
    printf("%s--------------------------------------------------------------%s\n\n", CLR_CYAN, CLR_RESET);

    /* 4. ESCRITURA ZERO-COPY: Modificar la memoria del kernel desde usuario */
    printf("%s[2] ESCRITURA ZERO-COPY DIRECTA:%s\n", CLR_YELLOW, CLR_RESET);
    printf("    Escribiendo nuevo mensaje en la memoria física compartida...\n");

    const char *nuevo_mensaje = "MODIFICADO_DESDE_USUARIO: Memoria alterada directamente en RAM sin syscalls!\n";
    snprintf(shared_mem, MAPPING_SIZE, "%s", nuevo_mensaje);
    printf("    %s[+] Memoria actualizada instantáneamente mediante asignación directa.%s\n\n", 
           CLR_GREEN, CLR_RESET);

    /* 5. Pedirle al driver del kernel que inspeccione la memoria y la reporte en dmesg */
    printf("%s[3] VERIFICACIÓN EN LA CAPA KERNEL:%s\n", CLR_YELLOW, CLR_RESET);
    printf("    Notificando al driver para que verifique el buffer en memoria física...\n");
    char dummy_buf[1];
    read(fd, dummy_buf, 1);
    printf("    %s[+] Revise el log del kernel con: %ssudo dmesg | tail -n 5%s\n\n", 
           CLR_GREEN, CLR_BOLD, CLR_RESET);

    /* 6. BENCHMARK DE VELOCIDAD DE ACCESO: 10,000,000 de accesos a memoria */
    printf("%s[4] BENCHMARK DE VELOCIDAD:%s\n", CLR_YELLOW, CLR_RESET);
    long iteraciones = 10000000;
    printf("    Ejecutando %s%ld%s operaciones directas en la memoria compartida...\n", 
           CLR_BOLD, iteraciones, CLR_RESET);

    double t_start = get_time_sec();
    volatile char *vptr = shared_mem;
    for (long i = 0; i < iteraciones; i++) {
        vptr[i % MAPPING_SIZE] = (char)(i & 0xFF);
    }
    double t_end = get_time_sec();
    double elapsed = t_end - t_start;
    double ops_per_sec = (double)iteraciones / elapsed;

    printf("    Tiempo total     : %s%.6f segundos%s\n", CLR_YELLOW, elapsed, CLR_RESET);
    printf("    Operaciones / seg: %s%s%.2f millones de ops/seg%s\n", 
           CLR_GREEN, CLR_BOLD, ops_per_sec / 1e6, CLR_RESET);
    printf("    %s¡Velocidad pura de bus de memoria sin cambios de contexto a Ring 0!%s\n\n", 
           CLR_CYAN, CLR_RESET);

    /* 7. Limpiar mapeo y cerrar descriptor */
    if (munmap(shared_mem, MAPPING_SIZE) != 0) {
        perror(CLR_RED "munmap" CLR_RESET);
    }
    close(fd);

    printf("%s==============================================================%s\n", CLR_CYAN, CLR_RESET);
    printf("%s[+] Prueba completada con éxito.%s\n", CLR_GREEN, CLR_RESET);
    printf("%s==============================================================%s\n", CLR_CYAN, CLR_RESET);

    return EXIT_SUCCESS;
}
