/**
 * @file main.c
 * @brief PASO 2: Demostración de Memoria Mapeada (mmap) en Espacio de Usuario
 * @course Sistemas Operativos - DOCENCIA 2026-B
 * 
 * CONCEPTO TEÓRICO EN LA CAPA KERNEL:
 * -----------------------------------
 * ¿Cómo funciona mmap() y por qué es más eficiente que read()?
 * 
 * 1. LLAMADA INICIAL sys_mmap():
 *    - El proceso solicita mapear un archivo o dispositivo a su espacio de memoria.
 *    - El kernel reserva un rango de direcciones virtuales contiguas en el proceso
 *      creando una estructura en el kernel llamada:
 *          struct vm_area_struct (VMA)
 *    - IMPORTANTE: En este momento el kernel NO carga el archivo completo en RAM.
 *      Solo registra que ese rango virtual pertenece a ese archivo (Paginación por Demanda).
 *    - mmap() retorna un puntero virtual (void *ptr). Esto toma solo unos microsegundos.
 * 
 * 2. ACCESO A DATOS Y PAGE FAULT (Fallo de Página):
 *    - Cuando el programa ejecuta: char c = ptr[i];
 *    - La MMU (Unidad de Manejo de Memoria de la CPU) intenta traducir la dirección virtual.
 *    - Como la entrada en la Tabla de Páginas (PTE - Page Table Entry) aún no está activa,
 *      la CPU dispara una excepción de hardware: Page Fault (Fallo de Página, Vector #14).
 *    - El manejador de fallos de página del kernel (do_page_fault) intercepta la interrupción:
 *        a) Verifica que la dirección caiga dentro de un VMA válido.
 *        b) Busca la página en la Page Cache de Linux (si no está, la trae de disco).
 *        c) Mapea directamente el marco físico (PFN) de la Page Cache a la PTE del proceso.
 *    - Se reanuda la instrucción de la CPU de manera transparente para el proceso.
 * 
 * 3. POR QUÉ ES ZERO-COPY Y ULTRA EFICIENTE:
 *    - CERO COPIAS: El proceso lee DIRECTAMENTE desde la misma memoria física de la
 *      Page Cache del kernel. NO se ejecuta copy_to_user().
 *    - CERO SYSCALLS POSTERIORES: Una vez que la página está mapeada en la PTE,
 *      los siguientes millones de accesos a esa página son operaciones puras de CPU/RAM,
 *      sin cambios de contexto a Ring 0 ni llamadas al sistema adicionales.
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

#define TEST_FILENAME       "archivo_prueba_mmap.bin"
#define DEFAULT_FILE_SIZE   (32 * 1024 * 1024) /* 32 MB */

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

static int create_test_file(const char *filename, size_t size) {
    printf("%s[*] Generando archivo de prueba: %s%s%s (%.2f MB)...%s\n", 
           CLR_CYAN, CLR_BOLD, filename, CLR_RESET, (double)size / (1024 * 1024), CLR_RESET);
    
    int fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror(CLR_RED "Error en open() al crear archivo" CLR_RESET);
        return -1;
    }

    if (ftruncate(fd, (off_t)size) != 0) {
        perror(CLR_RED "ftruncate" CLR_RESET);
        close(fd);
        return -1;
    }

    char *map = (char *)mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) {
        perror(CLR_RED "mmap al crear" CLR_RESET);
        close(fd);
        return -1;
    }

    memset(map, 'A', size);
    msync(map, size, MS_SYNC);
    munmap(map, size);
    close(fd);

    printf("%s[+] Archivo creado e inicializado exitosamente.%s\n\n", CLR_GREEN, CLR_RESET);
    return 0;
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    const char *filename = TEST_FILENAME;
    size_t file_size = DEFAULT_FILE_SIZE;

    printf("%s================================================================================%s\n", CLR_CYAN, CLR_RESET);
    printf("%s%s PASO 2: MEMORIA MAPEADA CON mmap() (ESPACIO DE USUARIO)                     %s\n", CLR_BOLD, CLR_BLUE, CLR_RESET);
    printf("%s================================================================================%s\n", CLR_CYAN, CLR_RESET);
    printf("%s%s[PROPÓSITO]%s\n", CLR_GREEN, CLR_BOLD, CLR_RESET);
    printf("  Eliminar las copias intermedias de memoria y las syscalls de lectura repetitivas\n");
    printf("  accediendo a los datos del archivo directamente mediante punteros C en RAM.\n\n");
    printf("%s%s[ESTRATEGIA TÉCNICA]%s\n", CLR_YELLOW, CLR_BOLD, CLR_RESET);
    printf("  1. Se invoca sys_mmap una sola vez para reservar un rango de direcciones virtuales (VMA).\n");
    printf("  2. En el primer acceso a cada página, la MMU dispara una excepción Page Fault (#PF).\n");
    printf("  3. El kernel enlaza directamente el marco físico de la Page Cache a la PTE del proceso.\n");
    printf("  4. Cero llamadas a sys_read y Cero copias copy_to_user() (Zero-Copy).\n");
    printf("%s================================================================================%s\n\n", CLR_CYAN, CLR_RESET);

    /* 1. Asegurar existencia del archivo */
    struct stat st;
    if (stat(filename, &st) != 0 || st.st_size < (off_t)file_size) {
        if (create_test_file(filename, file_size) != 0)
            return EXIT_FAILURE;
    } else {
        file_size = (size_t)st.st_size;
        printf("%s[i] Usando archivo existente: %s%s%s (%.2f MB)%s\n\n", 
               CLR_CYAN, CLR_BOLD, filename, CLR_RESET, (double)file_size / (1024 * 1024), CLR_RESET);
    }

    /* 2. Obtener estadísticas de fallos de página iniciales con getrusage() */
    struct rusage ru_before, ru_after;
    getrusage(RUSAGE_SELF, &ru_before);

    /* 3. Abrir descriptor de archivo */
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        perror(CLR_RED "open" CLR_RESET);
        return EXIT_FAILURE;
    }

    printf("%s[*] Mapeando archivo completo a memoria virtual con mmap()...%s\n", CLR_YELLOW, CLR_RESET);
    double t_start = get_time_sec();

    char *mapped_data = (char *)mmap(NULL, file_size, PROT_READ, MAP_SHARED, fd, 0);
    if (mapped_data == MAP_FAILED) {
        perror(CLR_RED "mmap" CLR_RESET);
        close(fd);
        return EXIT_FAILURE;
    }

    madvise(mapped_data, file_size, MADV_SEQUENTIAL);

    printf("%s[+] Archivo mapeado en dirección virtual: %s%p%s\n", 
           CLR_GREEN, CLR_BOLD, (void *)mapped_data, CLR_RESET);
    printf("%s[*] Procesando memoria directamente mediante puntero C (Zero-Copy)...%s\n", 
           CLR_CYAN, CLR_RESET);

    /* 4. Acceso directo como si fuera un arreglo de RAM común */
    unsigned long long checksum = 0;
    for (size_t i = 0; i < file_size; i++) {
        checksum += (unsigned char)mapped_data[i];
    }

    double t_end = get_time_sec();
    double elapsed = t_end - t_start;
    double throughput_mb = ((double)file_size / (1024.0 * 1024.0)) / elapsed;

    /* 5. Obtener estadísticas de fallos de página finales */
    getrusage(RUSAGE_SELF, &ru_after);
    long minor_faults = ru_after.ru_minflt - ru_before.ru_minflt;
    long major_faults = ru_after.ru_majflt - ru_before.ru_majflt;

    /* 6. Liberar el mapeo y cerrar */
    if (munmap(mapped_data, file_size) != 0) {
        perror(CLR_RED "munmap" CLR_RESET);
    }
    close(fd);

    /* 7. Mostrar resultados y análisis */
    printf("\n%s--- RESULTADOS DEL PASO 2 (Memoria Mapeada con mmap) ---%s\n", CLR_CYAN, CLR_RESET);
    printf(" Total bytes procesados  : %s%zu bytes (%.2f MB)%s\n", 
           CLR_BOLD, file_size, (double)file_size / (1024.0 * 1024.0), CLR_RESET);
    printf(" Llamadas a sys_read     : %s%s0 (¡Ninguna!)%s\n", CLR_GREEN, CLR_BOLD, CLR_RESET);
    printf(" Llamadas a sys_mmap     : %s1%s\n", CLR_BOLD, CLR_RESET);
    printf(" Minor Page Faults (MMU) : %s%ld%s (resoluciones en Page Cache)\n", 
           CLR_YELLOW, minor_faults, CLR_RESET);
    printf(" Major Page Faults (I/O) : %s%ld%s (lecturas de disco físicas)\n", 
           CLR_YELLOW, major_faults, CLR_RESET);
    printf(" Tiempo transcurrido     : %s%.4f segundos%s\n", CLR_YELLOW, elapsed, CLR_RESET);
    printf(" Throughput (Rendimiento): %s%s%.2f MB/s%s\n", CLR_GREEN, CLR_BOLD, throughput_mb, CLR_RESET);
    printf(" Checksum de verificación: %s0x%llx%s\n", CLR_CYAN, checksum, CLR_RESET);
    printf("%s----------------------------------------------------%s\n", CLR_CYAN, CLR_RESET);
    printf("%s[!] NOTA EDUCATIVA:%s\n", CLR_YELLOW, CLR_RESET);
    printf("    A diferencia de read(), %sNO hubo copias intermedias de memoria (Zero-Copy)%s.\n", 
           CLR_BOLD, CLR_RESET);
    printf("    El proceso leyó directamente desde los marcos de página física\n");
    printf("    donde el kernel almacena los datos, ahorrando miles de syscalls.\n");
    printf("%s==============================================================%s\n", CLR_CYAN, CLR_RESET);

    return EXIT_SUCCESS;
}
