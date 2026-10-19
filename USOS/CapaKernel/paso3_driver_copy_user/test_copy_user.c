/**
 * @file test_copy_user.c
 * @brief Programa de usuario para probar el driver con copy_to_user()
 * @course Sistemas Operativos - DOCENCIA 2026-B
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

#define DEVICE_PATH "/dev/demo_copy_user"
#define READ_CHUNK  4096

/* Códigos de Color ANSI */
#define CLR_RESET   "\033[0m"
#define CLR_BOLD    "\033[1m"
#define CLR_RED     "\033[31m"
#define CLR_GREEN   "\033[32m"
#define CLR_YELLOW  "\033[33m"
#define CLR_BLUE    "\033[34m"
#define CLR_CYAN    "\033[36m"

static double get_time_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

int main(void) {
    printf("%s================================================================================%s\n", CLR_CYAN, CLR_RESET);
    printf("%s%s PASO 3: DRIVER EN CAPA KERNEL CON dev_read() Y copy_to_user()                %s\n", CLR_BOLD, CLR_BLUE, CLR_RESET);
    printf(" Dispositivo objetivo: %s%s%s%s\n", CLR_BOLD, CLR_YELLOW, DEVICE_PATH, CLR_RESET);
    printf("%s================================================================================%s\n", CLR_CYAN, CLR_RESET);
    printf("%s%s[PROPÓSITO]%s\n", CLR_GREEN, CLR_BOLD, CLR_RESET);
    printf("  Comprender la arquitectura interna de un controlador de caracteres en el Kernel\n");
    printf("  y por qué es mandatorio usar copy_to_user() para transferir datos a Ring 3.\n\n");
    printf("%s%s[ESTRATEGIA TÉCNICA]%s\n", CLR_YELLOW, CLR_BOLD, CLR_RESET);
    printf("  1. El driver reserva búfer en memoria física de kernel con kzalloc(GFP_KERNEL).\n");
    printf("  2. La función dev_read() es invocada cuando el usuario llama a read().\n");
    printf("  3. El kernel ejecuta copy_to_user() para validar punteros y transferir datos.\n");
    printf("  4. Se evidencia el costo de duplicación de memoria byte a byte por CPU.\n");
    printf("%s================================================================================%s\n\n", CLR_CYAN, CLR_RESET);

    int fd = open(DEVICE_PATH, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "%s[-] No se pudo abrir %s: %s%s\n", CLR_RED, DEVICE_PATH, strerror(errno), CLR_RESET);
        fprintf(stderr, "    %s¿Cargó el módulo del kernel? (sudo ./scripts/load_modules.sh)%s\n", CLR_YELLOW, CLR_RESET);
        return EXIT_FAILURE;
    }

    char buffer[READ_CHUNK + 1];
    size_t total_bytes = 0;
    int syscalls = 0;

    double t_start = get_time_sec();

    while (1) {
        ssize_t bytes = read(fd, buffer, READ_CHUNK);
        if (bytes < 0) {
            perror(CLR_RED "Error en read()" CLR_RESET);
            close(fd);
            return EXIT_FAILURE;
        }
        if (bytes == 0)
            break; /* EOF */

        syscalls++;
        total_bytes += bytes;

        if (syscalls == 1) {
            buffer[bytes < 250 ? bytes : 250] = '\0';
            printf("%s[+] Fragmento recibido desde el kernel:%s\n", CLR_GREEN, CLR_RESET);
            printf("%s--------------------------------------------------------------%s\n", CLR_CYAN, CLR_RESET);
            printf("%s%s%s", CLR_BOLD, buffer, CLR_RESET);
            printf("%s--------------------------------------------------------------%s\n\n", CLR_CYAN, CLR_RESET);
        }
    }

    double t_end = get_time_sec();
    double elapsed = t_end - t_start;

    close(fd);

    printf("%s--- MÉTRICAS ---\n%s", CLR_CYAN, CLR_RESET);
    printf(" Total bytes leídos     : %s%zu bytes%s\n", CLR_BOLD, total_bytes, CLR_RESET);
    printf(" Total syscalls read()  : %s%s%d%s\n", CLR_RED, CLR_BOLD, syscalls, CLR_RESET);
    printf(" Tiempo empleado        : %s%.6f segundos%s\n", CLR_YELLOW, elapsed, CLR_RESET);
    printf("%s--------------------------------------------------------------%s\n", CLR_CYAN, CLR_RESET);
    printf("%s[!] En cada llamada a read(), el driver en el kernel ejecutó copy_to_user().%s\n", 
           CLR_YELLOW, CLR_RESET);
    printf("%s==============================================================%s\n", CLR_CYAN, CLR_RESET);

    return EXIT_SUCCESS;
}
