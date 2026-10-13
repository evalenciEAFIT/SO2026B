/**
 * @file main.c
 * @brief Programa principal para la suite de cifrado de archivos en Linux.
 *
 * Demostración de conceptos clave de Sistemas Operativos:
 * 1. Descriptores de archivo (File Descriptors) y tabla de descriptores del proceso.
 * 2. Llamadas al sistema (System Calls): open, read, write, close, fstat, mmap, munmap, msync, ftruncate.
 * 3. Metadatos de archivos del Kernel: Inodo, permisos UNIX, tamaño en bytes, tamaño de bloque (st_blksize).
 * 4. Medición precisa de tiempo de CPU y tiempo real (clock_gettime con CLOCK_MONOTONIC).
 * 5. Manejo seguro de memoria dinámica y recursos del kernel.
 */

#include "common.h"
#include "sys_io.h"
#include "crypto_caesar.h"
#include "crypto_xor.h"
#include "crypto_rc4.h"
#include "crypto_transposition.h"
#include "crypto_mmap.h"

static void print_banner(void) {
    printf("====================================================================\n");
    printf("     SUITE DE CIFRADO DE ARCHIVOS - SISTEMAS OPERATIVOS (LINUX)     \n");
    printf("                 Orientado a System Calls y Kernel I/O              \n");
    printf("====================================================================\n\n");
}

static void print_help(const char *prog_name) {
    print_banner();
    printf("USO:\n");
    printf("  %s -a <algoritmo> -m <modo> -i <origen> -o <destino> -k <clave> [opciones]\n\n", prog_name);

    printf("ALGORITMOS SOPORTADOS (-a):\n");
    printf("  caesar      : Cifrado César Extendido (Byte-Shift modular 256 por bloques)\n");
    printf("  xor         : Cifrado XOR Polialfabético (Flujo simétrico con clave cíclica)\n");
    printf("  rc4         : Cifrado de Flujo RC4 (Rivest Cipher 4 con KSA y PRGA)\n");
    printf("  transp      : Cifrado por Transposición Columnar por Bloques (Matricial con cabecera TRSP)\n");
    printf("  mmap        : Cifrado en Memoria Virtual vía llamada al sistema mmap(2) (Zero-Copy)\n\n");

    printf("MODOS (-m o banderas directas):\n");
    printf("  -m enc | -e : Cifrar (Encrypt)\n");
    printf("  -m dec | -d : Descifrar (Decrypt)\n\n");

    printf("PARÁMETROS OBLIGATORIOS:\n");
    printf("  -i <ruta>   : Archivo de entrada\n");
    printf("  -o <ruta>   : Archivo de salida (Opcional en mmap si se cifra in-place)\n");
    printf("  -k <clave>  : Clave de cifrado (Número entero para 'caesar', texto/clave para los demás)\n\n");

    printf("OPCIONES ADICIONALES:\n");
    printf("  -b          : Modo Benchmark (Mide tiempo de ejecución y tasa de transferencia en MB/s)\n");
    printf("  -s          : Mostrar metadatos del archivo consultados mediante la syscall fstat(2)\n");
    printf("  -h          : Mostrar este mensaje de ayuda\n\n");

    printf("EJEMPLOS:\n");
    printf("  %s -a caesar -e -i doc.txt -o doc.enc -k 15\n", prog_name);
    printf("  %s -a caesar -d -i doc.enc -o doc.dec.txt -k 15\n", prog_name);
    printf("  %s -a xor -e -i video.mp4 -o video.enc -k MiClaveSecreta -b\n", prog_name);
    printf("  %s -a rc4 -e -i dato.bin -o dato.rc4 -k SuperKey2026\n", prog_name);
    printf("  %s -a transp -e -i mensaje.txt -o mensaje.enc -k MATRIZ\n", prog_name);
    printf("  %s -a mmap -e -i archivo.dat -o archivo.mmap.enc -k LlaveVirtual\n", prog_name);
    printf("  %s -a mmap -e -i archivo.dat -k LlaveVirtual (in-situ en memoria virtual)\n\n", prog_name);
}

static void print_file_metadata(int fd, const char *label) {
    struct stat st;
    if (sys_get_stat(fd, &st) == STATUS_OK) {
        printf("--- Metadatos del Sistema de Archivos (%s) ---\n", label);
        printf("  * Descriptor de archivo (fd) : %d\n", fd);
        printf("  * Inodo en disco (st_ino)    : %lu\n", (unsigned long)st.st_ino);
        printf("  * Dispositivo (st_dev)       : %lu\n", (unsigned long)st.st_dev);
        printf("  * Tamaño total (st_size)     : %ld bytes (%.2f KB)\n", (long)st.st_size, (double)st.st_size / 1024.0);
        printf("  * Bloque de I/O (st_blksize) : %ld bytes\n", (long)st.st_blksize);
        printf("  * Bloques de 512B (st_blocks): %ld\n", (long)st.st_blocks);
        printf("  * Permisos UNIX (st_mode)    : %04o\n", (unsigned int)(st.st_mode & 0777));
        printf("--------------------------------------------------\n\n");
    }
}

static double get_time_diff_ms(struct timespec start, struct timespec end) {
    double sec = (double)(end.tv_sec - start.tv_sec);
    double nsec = (double)(end.tv_nsec - start.tv_nsec);
    return (sec * 1000.0) + (nsec / 1000000.0);
}

int main(int argc, char *argv[]) {
    char *algo = NULL;
    char *input_path = NULL;
    char *output_path = NULL;
    char *key_str = NULL;
    CryptoMode mode = 0;
    bool benchmark = false;
    bool show_stats = false;

    /* Parseo manual de argumentos de línea de comandos */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_help(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-a") == 0 && i + 1 < argc) {
            algo = argv[++i];
        } else if (strcmp(argv[i], "-i") == 0 && i + 1 < argc) {
            input_path = argv[++i];
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output_path = argv[++i];
        } else if (strcmp(argv[i], "-k") == 0 && i + 1 < argc) {
            key_str = argv[++i];
        } else if (strcmp(argv[i], "-e") == 0) {
            mode = MODE_ENCRYPT;
        } else if (strcmp(argv[i], "-d") == 0) {
            mode = MODE_DECRYPT;
        } else if (strcmp(argv[i], "-m") == 0 && i + 1 < argc) {
            i++;
            if (strcasecmp(argv[i], "enc") == 0 || strcasecmp(argv[i], "encrypt") == 0) {
                mode = MODE_ENCRYPT;
            } else if (strcasecmp(argv[i], "dec") == 0 || strcasecmp(argv[i], "decrypt") == 0) {
                mode = MODE_DECRYPT;
            } else {
                fprintf(stderr, "[ERROR] Modo inválido: %s. Use 'enc' o 'dec'.\n", argv[i]);
                return 1;
            }
        } else if (strcmp(argv[i], "-b") == 0) {
            benchmark = true;
        } else if (strcmp(argv[i], "-s") == 0) {
            show_stats = true;
        } else {
            fprintf(stderr, "[ERROR] Argumento no reconocido: %s\n", argv[i]);
            fprintf(stderr, "Ejecute '%s -h' para consultar la ayuda.\n", argv[0]);
            return 1;
        }
    }

    /* Validación de parámetros requeridos */
    if (!algo || !input_path || !key_str || mode == 0) {
        fprintf(stderr, "[ERROR] Parámetros insuficientes.\n");
        fprintf(stderr, "Debe especificar al menos: -a <algoritmo> -m <enc|dec> -i <origen> -k <clave>\n");
        fprintf(stderr, "Ejecute '%s -h' para ver las opciones disponibles.\n", argv[0]);
        return 1;
    }

    /* mmap permite operar in-place si output_path es nulo */
    bool is_mmap = (strcasecmp(algo, "mmap") == 0);
    if (!is_mmap && !output_path) {
        fprintf(stderr, "[ERROR] El algoritmo '%s' requiere especificar el archivo de salida (-o <destino>).\n", algo);
        return 1;
    }

    printf("[INFO] Algoritmo seleccionado : %s\n", algo);
    printf("[INFO] Modo de operación      : %s\n", (mode == MODE_ENCRYPT) ? "Cifrado (Encrypt)" : "Descifrado (Decrypt)");
    printf("[INFO] Archivo de entrada     : %s\n", input_path);
    if (output_path) {
        printf("[INFO] Archivo de salida      : %s\n", output_path);
    } else {
        printf("[INFO] Archivo de salida      : <In-Situ / In-Place sobre el mismo archivo>\n");
    }

    struct timespec ts_start, ts_end;
    if (clock_gettime(CLOCK_MONOTONIC, &ts_start) < 0) {
        log_sys_error("clock_gettime falló");
    }

    StatusCode status = STATUS_OK;

    if (is_mmap) {
        /*
         * Módulo especial mmap: opera con mapeo directo de memoria virtual.
         */
        status = crypto_mmap_process(input_path, output_path, (const uint8_t *)key_str, strlen(key_str), mode);

    } else {
        /*
         * Módulos basados en Descriptores de Archivos y E/S tradicional por bloques.
         */
        int fd_in = sys_open_read(input_path);
        if (fd_in < 0) {
            fprintf(stderr, "[FATAL] No se pudo abrir el archivo de entrada.\n");
            return 1;
        }

        struct stat st_in;
        if (sys_get_stat(fd_in, &st_in) != STATUS_OK) {
            sys_close_fd(&fd_in);
            return 1;
        }

        if (show_stats) {
            print_file_metadata(fd_in, "Entrada");
        }

        /* Creamos el archivo de salida con los mismos permisos del archivo original */
        int fd_out = sys_open_write(output_path, st_in.st_mode);
        if (fd_out < 0) {
            sys_close_fd(&fd_in);
            fprintf(stderr, "[FATAL] No se pudo abrir/crear el archivo de salida.\n");
            return 1;
        }

        if (strcasecmp(algo, "caesar") == 0) {
            int shift = atoi(key_str);
            status = crypto_caesar_process(fd_in, fd_out, shift, mode);
        } else if (strcasecmp(algo, "xor") == 0) {
            status = crypto_xor_process(fd_in, fd_out, (const uint8_t *)key_str, strlen(key_str), mode);
        } else if (strcasecmp(algo, "rc4") == 0) {
            status = crypto_rc4_process(fd_in, fd_out, (const uint8_t *)key_str, strlen(key_str), mode);
        } else if (strcasecmp(algo, "transp") == 0 || strcasecmp(algo, "transposition") == 0) {
            status = crypto_transposition_process(fd_in, fd_out, key_str, mode);
        } else {
            fprintf(stderr, "[ERROR] Algoritmo desconocido: '%s'. Opciones: caesar, xor, rc4, transp, mmap.\n", algo);
            status = STATUS_ERR_INVALID_ARGS;
        }

        /* Cerrar descriptores de archivo en la tabla del proceso */
        sys_close_fd(&fd_in);
        sys_close_fd(&fd_out);
    }

    clock_gettime(CLOCK_MONOTONIC, &ts_end);
    double elapsed_ms = get_time_diff_ms(ts_start, ts_end);

    if (status != STATUS_OK) {
        fprintf(stderr, "\n[FALLO] La operación no se completó: %s (código: %d)\n",
                status_to_string(status), status);
        return (int)status;
    }

    printf("\n[ÉXITO] %s finalizado satisfactoriamente.\n",
           (mode == MODE_ENCRYPT) ? "Cifrado" : "Descifrado");

    if (benchmark) {
        /* Calcular tamaño del archivo procesado para la métrica */
        struct stat st_final;
        const char *ref_file = output_path ? output_path : input_path;
        double mb_processed = 0.0;
        if (stat(ref_file, &st_final) == 0) {
            mb_processed = (double)st_final.st_size / (1024.0 * 1024.0);
        }

        double sec = elapsed_ms / 1000.0;
        double throughput = (sec > 0.000001) ? (mb_processed / sec) : 0.0;

        printf("------------------ METRICAS DE BENCHMARK (SO) ------------------\n");
        printf("  * Tiempo transcurrido (CLOCK_MONOTONIC): %.3f ms (%.4f s)\n", elapsed_ms, sec);
        printf("  * Datos procesados                     : %.4f MB\n", mb_processed);
        printf("  * Rendimiento (Throughput)             : %.2f MB/s\n", throughput);
        printf("----------------------------------------------------------------\n");
    }

    return 0;
}
