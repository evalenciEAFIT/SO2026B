/**
 * @file main.c
 * @brief PASO 1: Demostración de Entrada/Salida Clásica con read() y write()
 * @course Sistemas Operativos - DOCENCIA 2026-B
 * 
 * CONCEPTO TEÓRICO EN LA CAPA KERNEL:
 * -----------------------------------
 * Cuando un proceso en Espacio de Usuario ejecuta la llamada al sistema read():
 * 1. TRAP/INTERRUPCIÓN DE SOFTWARE: La CPU conmuta de modo usuario (Ring 3) 
 *    a modo kernel (Ring 0). Esto produce un "Context Switch" de privilegio.
 * 2. VFS (Virtual File System): El kernel consulta el descriptor 'fd' en la
 *    tabla de descriptores del proceso (task_struct->files).
 * 3. PAGE CACHE: El kernel localiza la página de disco en la memoria física del
 *    kernel (Page Cache) o la lee de disco si no está en RAM.
 * 4. COPIA DE MEMORIA (DOBLE BUFFER): El kernel NO puede darle acceso directo
 *    al búfer del kernel al usuario por seguridad y aislamiento de memoria.
 *    Por tanto, ejecuta copy_to_user(): la CPU copia byte a byte los datos
 *    desde la memoria del kernel hacia el búfer en el heap/stack del usuario.
 * 5. RETORNO DE SYSCALL: La CPU conmuta nuevamente de Ring 0 a Ring 3.
 * 
 * Desventaja principal:
 * - Si leemos un archivo de 100 MB con bloques de 4 KB, se ejecutan 25,600
 *   llamadas al sistema (25,600 cambios de contexto de ida y vuelta) y se
 *   copian 100 MB de memoria de kernel a usuario innecesariamente.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#define DEFAULT_BLOCK_SIZE  4096       /* 4 KB (tamaño típico de página) */
#define DEFAULT_FILE_SIZE   (32 * 1024 * 1024) /* 32 MB para demostración */
#define TEST_FILENAME       "archivo_prueba_io.bin"

/* Códigos de Color ANSI para salidas CLI legibles y atractivas */
#define CLR_RESET   "\033[0m"
#define CLR_BOLD    "\033[1m"
#define CLR_RED     "\033[31m"
#define CLR_GREEN   "\033[32m"
#define CLR_YELLOW  "\033[33m"
#define CLR_BLUE    "\033[34m"
#define CLR_MAGENTA "\033[35m"
#define CLR_CYAN    "\033[36m"

/* Obtener marca de tiempo en segundos con precisión de nanosegundos */
static double get_time_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

/* Crear un archivo de prueba con datos aleatorios/patrón */
static int create_test_file(const char *filename, size_t size) {
    printf("%s[*] Generando archivo de prueba: %s%s%s (%.2f MB)...%s\n", 
           CLR_CYAN, CLR_BOLD, filename, CLR_RESET, (double)size / (1024 * 1024), CLR_RESET);
    
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror(CLR_RED "Error en open() al crear archivo" CLR_RESET);
        return -1;
    }

    char buffer[DEFAULT_BLOCK_SIZE];
    memset(buffer, 'A', sizeof(buffer));

    size_t written = 0;
    while (written < size) {
        size_t to_write = sizeof(buffer);
        if (written + to_write > size)
            to_write = size - written;

        ssize_t res = write(fd, buffer, to_write);
        if (res < 0) {
            perror(CLR_RED "Error en write()" CLR_RESET);
            close(fd);
            return -1;
        }
        written += (size_t)res;
    }

    close(fd);
    printf("%s[+] Archivo creado exitosamente.%s\n\n", CLR_GREEN, CLR_RESET);
    return 0;
}

int main(int argc, char *argv[]) {
    size_t block_size = DEFAULT_BLOCK_SIZE;
    size_t file_size = DEFAULT_FILE_SIZE;
    const char *filename = TEST_FILENAME;

    printf("%s================================================================================%s\n", CLR_CYAN, CLR_RESET);
    printf("%s%s PASO 1: ENTRADA/SALIDA CLÁSICA CON read() (ESPACIO DE USUARIO)              %s\n", CLR_BOLD, CLR_BLUE, CLR_RESET);
    printf("%s================================================================================%s\n", CLR_CYAN, CLR_RESET);
    printf("%s%s[PROPÓSITO]%s\n", CLR_GREEN, CLR_BOLD, CLR_RESET);
    printf("  Demostrar el flujo tradicional de lectura de archivos y medir la sobrecarga\n");
    printf("  generada por continuos cambios de contexto (Ring 3 <-> Ring 0) y doble búfer.\n\n");
    printf("%s%s[ESTRATEGIA TÉCNICA]%s\n", CLR_YELLOW, CLR_BOLD, CLR_RESET);
    printf("  1. Se solicita al kernel la lectura de bloques contiguos de 4 KB con read().\n");
    printf("  2. La CPU conmuta a Ring 0, el kernel busca los datos en la Page Cache y\n");
    printf("     ejecuta copy_to_user() para duplicar cada byte hacia el búfer de usuario.\n");
    printf("  3. La CPU conmuta de vuelta a Ring 3 tras completar cada bloque.\n");
    printf("%s================================================================================%s\n\n", CLR_CYAN, CLR_RESET);

    /* Permitir parametrizar tamaño de bloque por línea de comandos */
    if (argc >= 2) {
        long bs = atol(argv[1]);
        if (bs > 0) block_size = (size_t)bs;
    }

    /* 1. Crear archivo si no existe */
    struct stat st;
    if (stat(filename, &st) != 0 || st.st_size < (off_t)file_size) {
        if (create_test_file(filename, file_size) != 0)
            return EXIT_FAILURE;
    } else {
        file_size = (size_t)st.st_size;
        printf("%s[i] Usando archivo existente: %s%s%s (%.2f MB)%s\n\n", 
               CLR_CYAN, CLR_BOLD, filename, CLR_RESET, (double)file_size / (1024 * 1024), CLR_RESET);
    }

    /* 2. Reservar búfer en Espacio de Usuario */
    char *user_buffer = (char *)malloc(block_size);
    if (!user_buffer) {
        perror(CLR_RED "malloc" CLR_RESET);
        return EXIT_FAILURE;
    }

    /* 3. Abrir el archivo */
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        perror(CLR_RED "open" CLR_RESET);
        free(user_buffer);
        return EXIT_FAILURE;
    }

    /* 4. Ejecutar lectura clásica y cronometrar */
    printf("%s[*] Leyendo archivo con read() con bloques de %zu bytes (%zu KB)...%s\n",
           CLR_YELLOW, block_size, block_size / 1024, CLR_RESET);

    unsigned long long total_syscalls = 0;
    unsigned long long total_bytes = 0;
    unsigned long long checksum = 0;

    double t_start = get_time_sec();

    while (1) {
        ssize_t bytes_read = read(fd, user_buffer, block_size);
        if (bytes_read < 0) {
            perror(CLR_RED "Error al leer" CLR_RESET);
            break;
        }
        if (bytes_read == 0) {
            break; /* Fin de archivo (EOF) */
        }

        total_syscalls++;
        total_bytes += (unsigned long long)bytes_read;

        /* Procesar datos en espacio de usuario para simular trabajo */
        for (ssize_t i = 0; i < bytes_read; i++) {
            checksum += (unsigned char)user_buffer[i];
        }
    }

    double t_end = get_time_sec();
    double elapsed = t_end - t_start;
    double throughput_mb = ((double)total_bytes / (1024.0 * 1024.0)) / elapsed;

    close(fd);
    free(user_buffer);

    /* 5. Mostrar resultados y análisis */
    printf("\n%s--- RESULTADOS DEL PASO 1 (E/S Clásica con read) ---%s\n", CLR_CYAN, CLR_RESET);
    printf(" Total bytes leídos      : %s%llu bytes (%.2f MB)%s\n", 
           CLR_BOLD, total_bytes, (double)total_bytes / (1024.0 * 1024.0), CLR_RESET);
    printf(" Tamaño de bloque        : %s%zu bytes%s\n", CLR_BOLD, block_size, CLR_RESET);
    printf(" Llamadas a sys_read     : %s%s%llu syscalls%s\n", CLR_RED, CLR_BOLD, total_syscalls, CLR_RESET);
    printf(" Tiempo transcurrido     : %s%.4f segundos%s\n", CLR_YELLOW, elapsed, CLR_RESET);
    printf(" Throughput (Rendimiento): %s%s%.2f MB/s%s\n", CLR_GREEN, CLR_BOLD, throughput_mb, CLR_RESET);
    printf(" Checksum de verificación: %s0x%llx%s\n", CLR_CYAN, checksum, CLR_RESET);
    printf("%s----------------------------------------------------%s\n", CLR_CYAN, CLR_RESET);
    printf("%s[!] NOTA EDUCATIVA:%s\n", CLR_YELLOW, CLR_RESET);
    printf("    Se realizaron %s%llu%s cambios de contexto de ida y vuelta (Ring 3 <-> Ring 0).\n", 
           CLR_BOLD, total_syscalls, CLR_RESET);
    printf("    El kernel ejecutó %scopy_to_user()%s %s%llu%s veces para duplicar\n", 
           CLR_RED, CLR_RESET, CLR_BOLD, total_syscalls, CLR_RESET);
    printf("    los datos de la Page Cache hacia la memoria del proceso.\n");
    printf("%s==============================================================%s\n", CLR_CYAN, CLR_RESET);

    return EXIT_SUCCESS;
}
