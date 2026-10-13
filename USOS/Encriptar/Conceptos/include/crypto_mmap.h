/**
 * @file crypto_mmap.h
 * @brief Algoritmo y Mecanismo de Cifrado basado en Mapeo de Memoria (mmap).
 *
 * Conceptos de Sistemas Operativos:
 * - Llamada al sistema mmap(2): Mapeo de archivos en el espacio de direcciones virtuales del proceso.
 * - Tablas de páginas, Fallos de página (Page Faults) bajo demanda y Page Cache del Kernel.
 * - E/S de copia cero (Zero-Copy): Se evitan las transferencias intermedias de buffers de usuario.
 * - Sincronización explícita con almacenamiento mediante msync(2) (MS_SYNC).
 * - Liberación de regiones de memoria virtual con munmap(2).
 */

#ifndef CRYPTO_MMAP_H
#define CRYPTO_MMAP_H

#include "common.h"

/**
 * @brief Cifra un archivo directamente mapeándolo en memoria (in-place o creando copia mapeada).
 *
 * Si path_out es nulo o idéntico a path_in, la operación se realiza in-situ (in-place)
 * sobre las páginas virtuales del archivo original usando MAP_SHARED.
 * Si path_out es provisto, se crea el nuevo archivo, se dimensiona con ftruncate(2)
 * y se procesa mediante dos regiones mmap simultáneas.
 *
 * @param path_in Ruta del archivo de entrada.
 * @param path_out Ruta del archivo de salida (o NULL para in-place).
 * @param key Clave para el cifrado (XOR polialfabético o rotación bit a bit).
 * @param key_len Longitud de la clave.
 * @param mode Modo de operación (MODE_ENCRYPT o MODE_DECRYPT).
 * @return StatusCode Código de estado de la operación.
 */
StatusCode crypto_mmap_process(const char *path_in, const char *path_out,
                               const uint8_t *key, size_t key_len,
                               CryptoMode mode);

#endif /* CRYPTO_MMAP_H */
