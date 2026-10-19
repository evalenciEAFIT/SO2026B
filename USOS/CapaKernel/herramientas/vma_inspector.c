/**
 * @file vma_inspector.c
 * @brief Herramienta docente para inspeccionar VMAs, Tablas de Páginas y PFNs con Color
 * @course Sistemas Operativos - DOCENCIA 2026-B
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#define PAGE_SIZE_VAL 4096

/* Códigos de Color ANSI */
#define CLR_RESET   "\033[0m"
#define CLR_BOLD    "\033[1m"
#define CLR_RED     "\033[31m"
#define CLR_GREEN   "\033[32m"
#define CLR_YELLOW  "\033[33m"
#define CLR_BLUE    "\033[34m"
#define CLR_MAGENTA "\033[35m"
#define CLR_CYAN    "\033[36m"

static int get_pfn_info(void *vaddr, uint64_t *pfn, int *present) {
    uintptr_t addr = (uintptr_t)vaddr;
    uintptr_t vpn = addr / PAGE_SIZE_VAL;

    int fd = open("/proc/self/pagemap", O_RDONLY);
    if (fd < 0) {
        return -1;
    }

    off_t offset = vpn * sizeof(uint64_t);
    if (lseek(fd, offset, SEEK_SET) == (off_t)-1) {
        close(fd);
        return -1;
    }

    uint64_t entry = 0;
    if (read(fd, &entry, sizeof(entry)) != sizeof(entry)) {
        close(fd);
        return -1;
    }
    close(fd);

    *present = (entry >> 63) & 1;           /* Bit 63: Página presente en RAM */
    *pfn = entry & ((1ULL << 55) - 1);       /* Bits 0-54: Page Frame Number */
    return 0;
}

int main(void) {
    printf("%s======================================================================%s\n", CLR_CYAN, CLR_RESET);
    printf("%s%s HERRAMIENTA DE INSPECCIÓN: MMU, TABLA DE PÁGINAS Y PAGEMAP LINUX    %s\n", CLR_BOLD, CLR_BLUE, CLR_RESET);
    printf("%s======================================================================%s\n", CLR_CYAN, CLR_RESET);
    printf("%s%s[PROPÓSITO]%s\n", CLR_GREEN, CLR_BOLD, CLR_RESET);
    printf("  Visualizar directamente cómo la MMU y el subsistema de memoria virtual de Linux\n");
    printf("  traducen direcciones virtuales a marcos físicos (PFN) y resuelven Page Faults.\n\n");
    printf("%s%s[ESTRATEGIA TÉCNICA]%s\n", CLR_YELLOW, CLR_BOLD, CLR_RESET);
    printf("  1. Se reservan 4 páginas con mmap(MAP_ANONYMOUS). Ninguna tiene PFN asignado (bit 63 = 0).\n");
    printf("  2. Se inspecciona /proc/self/pagemap antes de tocar la memoria.\n");
    printf("  3. Se escribe un dato únicamente en la Página 2, provocando un Page Fault de hardware.\n");
    printf("  4. Se re-inspecciona /proc/self/pagemap: el bit Presente cambia a 1 solo para esa página.\n");
    printf("%s======================================================================%s\n\n", CLR_CYAN, CLR_RESET);

    size_t map_size = 4 * PAGE_SIZE_VAL;
    char *ptr = (char *)mmap(
        NULL,
        map_size,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS,
        -1,
        0
    );

    if (ptr == MAP_FAILED) {
        perror(CLR_RED "mmap" CLR_RESET);
        return EXIT_FAILURE;
    }

    printf("%s[1] Memoria virtual reservada con mmap() en dirección: %s%p%s (4 páginas)%s\n\n", 
           CLR_CYAN, CLR_BOLD, (void *)ptr, CLR_RESET, CLR_RESET);

    /* 2. Inspeccionar estado ANTES de tocar la memoria */
    printf("%s[2] ESTADO DE LA TABLA DE PÁGINAS (ANTES de acceder a los datos):%s\n", CLR_YELLOW, CLR_RESET);
    for (int i = 0; i < 4; i++) {
        uint64_t pfn = 0;
        int present = 0;
        void *page_addr = ptr + (i * PAGE_SIZE_VAL);

        if (get_pfn_info(page_addr, &pfn, &present) == 0) {
            printf("    Página %d (Addr %p) -> Presente en RAM: %s%s | PFN: 0x%lx\n",
                   i, page_addr, 
                   present ? CLR_GREEN "SI [1]" : CLR_RED "NO [0] (Paginación Perezosa)",
                   CLR_RESET, pfn);
        }
    }
    printf("    %s-> Observación: Ninguna página tiene marco físico asignado todavía.%s\n\n", 
           CLR_CYAN, CLR_RESET);

    /* 3. Tocar únicamente la Página 2 */
    printf("%s[3] Provocando Page Fault intencional accediendo solo a la PAGINA 2 (ptr[%d])...%s\n", 
           CLR_YELLOW, 2 * PAGE_SIZE_VAL, CLR_RESET);
    ptr[2 * PAGE_SIZE_VAL] = 'X';
    printf("    %s-> Dato escrito en RAM.%s\n\n", CLR_GREEN, CLR_RESET);

    /* 4. Inspeccionar estado DESPUÉS de tocar la memoria */
    printf("%s[4] ESTADO DE LA TABLA DE PÁGINAS (DESPUÉS del acceso):%s\n", CLR_YELLOW, CLR_RESET);
    for (int i = 0; i < 4; i++) {
        uint64_t pfn = 0;
        int present = 0;
        void *page_addr = ptr + (i * PAGE_SIZE_VAL);

        if (get_pfn_info(page_addr, &pfn, &present) == 0) {
            printf("    Página %d (Addr %p) -> Presente en RAM: %s%s | PFN: 0x%lx\n",
                   i, page_addr, 
                   present ? CLR_GREEN CLR_BOLD "SI [1] <-- ¡PAGE FAULT RESUELTO!" : CLR_RED "NO [0]",
                   CLR_RESET, pfn);
        }
    }

    printf("\n%s======================================================================%s\n", CLR_CYAN, CLR_RESET);
    printf("%s%s CONCLUSIÓN PEDAGÓGICA:%s\n", CLR_BOLD, CLR_YELLOW, CLR_RESET);
    printf(" La MMU y el subsistema de memoria virtual de Linux solo asignan marcos\n");
    printf(" físicos (PFN) cuando la CPU ejecuta una instrucción que realmente toca\n");
    printf(" la página, ahorrando memoria y tiempo de inicialización.\n");
    printf("%s======================================================================%s\n", CLR_CYAN, CLR_RESET);

    munmap(ptr, map_size);
    return EXIT_SUCCESS;
}
