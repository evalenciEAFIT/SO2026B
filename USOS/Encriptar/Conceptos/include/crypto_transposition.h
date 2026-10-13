/**
 * @file crypto_transposition.h
 * @brief Algoritmo de Cifrado por Transposición de Columnas por Bloques.
 *
 * Conceptos de Sistemas Operativos:
 * - Procesamiento por bloques estructurados en memoria (Matrices en Heap).
 * - Manejo de relleno (Padding) y longitud exacta del archivo original.
 * - Cabecera binaria básica para preservar el tamaño real del archivo (Metadatos a nivel de usuario).
 * - Comparación entre I/O por bloques vs I/O en flujo (Stream).
 */

#ifndef CRYPTO_TRANSPOSITION_H
#define CRYPTO_TRANSPOSITION_H

#include "common.h"

/**
 * @brief Procesa el cifrado o descifrado por transposición columnar.
 *
 * En cifrado, añade un encabezado con el tamaño original del archivo para
 * permitir restaurar la longitud exacta sin residuos del padding al descifrar.
 *
 * @param fd_in Descriptor de archivo origen.
 * @param fd_out Descriptor de archivo destino.
 * @param key Clave textual para derivar la permutación de columnas.
 * @param mode Modo (MODE_ENCRYPT o MODE_DECRYPT).
 * @return StatusCode Código de estado de la operación.
 */
StatusCode crypto_transposition_process(int fd_in, int fd_out, const char *key, CryptoMode mode);

#endif /* CRYPTO_TRANSPOSITION_H */
