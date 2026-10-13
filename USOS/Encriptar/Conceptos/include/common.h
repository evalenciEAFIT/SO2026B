/**
 * @file common.h
 * @brief Definiciones globales, tipos de datos y constantes para la suite de cifrado.
 *
 * Conceptos de Sistemas Operativos ilustrados:
 * - Llamadas al Sistema (System Calls vs Standard C Library).
 * - Descriptores de archivo (File Descriptors - enteros manejados por el kernel).
 * - Manejo robusto de errores mediante errno y perror/strerror.
 * - Tamaño de bloque óptimo según el sistema de archivos (st_blksize).
 */

#ifndef COMMON_H
#define COMMON_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <time.h>

/**
 * @brief Tamaño de buffer por defecto (4 KiB = tamaño de página típico en x86_64).
 */
#define DEFAULT_BUFFER_SIZE 4096

/**
 * @brief Permisos por defecto al crear archivos de salida (rw-r--r--).
 * En octal: 0644.
 */
#define DEFAULT_FILE_PERMS (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)

/**
 * @brief Modos de operación criptográfica.
 */
typedef enum {
    MODE_ENCRYPT = 1,
    MODE_DECRYPT = 2
} CryptoMode;

/**
 * @brief Códigos de estado y retorno del sistema.
 */
typedef enum {
    STATUS_OK = 0,
    STATUS_ERR_INVALID_ARGS,
    STATUS_ERR_OPEN_INPUT,
    STATUS_ERR_OPEN_OUTPUT,
    STATUS_ERR_READ,
    STATUS_ERR_WRITE,
    STATUS_ERR_STAT,
    STATUS_ERR_MMAP,
    STATUS_ERR_MEMORY,
    STATUS_ERR_ALGO_FAILED
} StatusCode;

/**
 * @brief Imprime un mensaje de error detallado asociando el contexto con strerror(errno).
 *
 * @param context Mensaje contextual del error.
 */
void log_sys_error(const char *context);

/**
 * @brief Convierte el código de estado a cadena de texto descriptiva.
 *
 * @param code Código de estado.
 * @return const char* Descripción textual.
 */
const char* status_to_string(StatusCode code);

#endif /* COMMON_H */
