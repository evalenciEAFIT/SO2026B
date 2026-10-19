/**
 * @file test_demand_paging.c
 * @brief Cliente de prueba para evidenciar Fallos de Página bajo Demanda con Salida a Color
 * @course Sistemas Operativos - DOCENCIA 2026-B
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <errno.h>

#define DEVICE_PATH   "/dev/demo_demand_fault"
#define PAGE_SIZE_VAL 4096
#define TOTAL_PAGES   8
#define TOTAL_BYTES   (TOTAL_PAGES * PAGE_SIZE_VAL)

/* Códigos de Color ANSI */
#define CLR_RESET   "\033[0m"
#define CLR_BOLD    "\033[1m"
#define CLR_RED     "\033[31m"
#define CLR_GREEN   "\033[32m"
#define CLR_YELLOW  "\033[33m"
#define CLR_BLUE    "\033[34m"
#define CLR_MAGENTA "\033[35m"
#define CLR_CYAN    "\033[36m"

static long get_resident_pages(void) {
    long size = 0, resident = 0;
    FILE *f = fopen("/proc/self/statm", "r");
    if (f) {
        if (fscanf(f, "%ld %ld", &size, &resident) == 2) {
            fclose(f);
            return resident;
        }
        fclose(f);
    }
    return -1;
}

int main(void) {
    printf("%s======================================================================%s\n", CLR_CYAN, CLR_RESET);
    printf("%s%s PASO 6: PAGINACIÓN POR DEMANDA REAL EN EL KERNEL (vm_ops->fault)      %s\n", CLR_BOLD, CLR_BLUE, CLR_RESET);
    printf(" Dispositivo objetivo: %s%s%s%s\n", CLR_BOLD, CLR_YELLOW, DEVICE_PATH, CLR_RESET);
    printf("%s======================================================================%s\n", CLR_CYAN, CLR_RESET);
    printf("%s%s[PROPÓSITO]%s\n", CLR_GREEN, CLR_BOLD, CLR_RESET);
    printf("  Demostrar cómo opera la paginación perezosa (Lazy Allocation) en la Capa Kernel,\n");
    printf("  asignando páginas físicas solo cuando la CPU intenta acceder a una dirección.\n\n");
    printf("%s%s[ESTRATEGIA TÉCNICA]%s\n", CLR_YELLOW, CLR_BOLD, CLR_RESET);
    printf("  1. El driver instala struct vm_operations_struct con el callback .fault.\n");
    printf("  2. El proceso reserva 8 páginas con mmap() (RSS inicial = 0 KB).\n");
    printf("  3. Se accede únicamente a la Página 0 y Página 5. La MMU dispara Page Faults (#PF).\n");
    printf("  4. El driver vincula el marco físico solo para esas 2 páginas. Las otras 6 no consumen RAM.\n");
    printf("%s======================================================================%s\n\n", CLR_CYAN, CLR_RESET);

    int fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "%s[-] No se pudo abrir %s: %s%s\n", CLR_RED, DEVICE_PATH, strerror(errno), CLR_RESET);
        fprintf(stderr, "    %sAsegúrese de haber cargado el módulo: sudo ./scripts/load_modules.sh%s\n", CLR_YELLOW, CLR_RESET);
        return EXIT_FAILURE;
    }

    struct rusage ru_before, ru_step;
    getrusage(RUSAGE_SELF, &ru_before);

    printf("%s[1] Ejecutando mmap() para reservar 8 páginas virtuales (%d bytes)...%s\n", 
           CLR_CYAN, TOTAL_BYTES, CLR_RESET);
    char *ptr = (char *)mmap(NULL, TOTAL_BYTES, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) {
        perror(CLR_RED "mmap" CLR_RESET);
        close(fd);
        return EXIT_FAILURE;
    }
    printf("%s[+] Mapeo establecido en dirección virtual: %s%p%s\n", 
           CLR_GREEN, CLR_BOLD, (void *)ptr, CLR_RESET);
    printf("    %sEn este momento: NINGUNA página física ha sido asignada a la PTE.%s\n\n", 
           CLR_YELLOW, CLR_RESET);

    /* Paso A: Acceder únicamente a la Página 0 */
    printf("%s[2] Leyendo el primer byte de la PAGINA 0 (ptr[0])...%s\n", CLR_YELLOW, CLR_RESET);
    getrusage(RUSAGE_SELF, &ru_before);
    char c0 = ptr[0]; /* <-- ¡AQUÍ LA CPU DISPARA EL PAGE FAULT #1! */
    getrusage(RUSAGE_SELF, &ru_step);

    printf("    Carácter leído: '%s%c%s'\n", CLR_BOLD, c0, CLR_RESET);
    printf("    Minor Page Faults detectados: %s+%ld%s\n", 
           CLR_GREEN, ru_step.ru_minflt - ru_before.ru_minflt, CLR_RESET);
    printf("    Páginas residentes en proceso (RSS): %s%ld páginas%s\n", 
           CLR_BOLD, get_resident_pages(), CLR_RESET);
    printf("    Contenido inicial de Página 0:\n    %s---\n    %.100s...\n    ---%s\n\n", 
           CLR_CYAN, ptr, CLR_RESET);

    /* Paso B: Acceder selectivamente a la Página 5 */
    size_t offset_pag5 = 5 * PAGE_SIZE_VAL;
    printf("%s[3] Saltando directamente a la PAGINA 5 (ptr[%zu])...%s\n", 
           CLR_YELLOW, offset_pag5, CLR_RESET);
    getrusage(RUSAGE_SELF, &ru_before);
    char c5 = ptr[offset_pag5]; /* <-- ¡AQUÍ LA CPU DISPARA EL PAGE FAULT #2! */
    getrusage(RUSAGE_SELF, &ru_step);

    printf("    Carácter leído: '%s%c%s'\n", CLR_BOLD, c5, CLR_RESET);
    printf("    Minor Page Faults detectados: %s+%ld%s\n", 
           CLR_GREEN, ru_step.ru_minflt - ru_before.ru_minflt, CLR_RESET);
    printf("    Páginas residentes en proceso (RSS): %s%ld páginas%s\n", 
           CLR_BOLD, get_resident_pages(), CLR_RESET);
    printf("    Contenido de Página 5:\n    %s---\n    %.100s...\n    ---%s\n\n", 
           CLR_CYAN, ptr + offset_pag5, CLR_RESET);

    /* Conclusión */
    printf("%s======================================================================%s\n", CLR_CYAN, CLR_RESET);
    printf("%s%s CONCLUSIÓN PEDAGÓGICA:%s\n", CLR_BOLD, CLR_YELLOW, CLR_RESET);
    printf(" - Las páginas 1, 2, 3, 4, 6 y 7 %sNUNCA fueron cargadas en memoria%s.\n", CLR_BOLD, CLR_RESET);
    printf(" - El manejador del kernel %sdemo_vma_fault()%s solo se activó %s2 veces%s.\n", 
           CLR_CYAN, CLR_RESET, CLR_GREEN, CLR_RESET);
    printf(" - Verifique los mensajes capturados en el kernel ejecutando:\n");
    printf("   %ssudo dmesg | tail -n 12%s\n", CLR_BOLD, CLR_RESET);
    printf("%s======================================================================%s\n", CLR_CYAN, CLR_RESET);

    munmap(ptr, TOTAL_BYTES);
    close(fd);
    return EXIT_SUCCESS;
}
