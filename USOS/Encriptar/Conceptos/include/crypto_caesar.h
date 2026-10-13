/**
 * @file crypto_caesar.h
 * @brief Algoritmo de Cifrado César Extendido a nivel de Byte.
 *
 * Conceptos de Sistemas Operativos:
 * - Procesamiento en Streaming sobre buffers de bloque.
 * - Desplazamiento modular sobre el espacio completo de un byte (0-255).
 * - E/S por bloques usando descriptores de archivo de Linux.
 */

#ifndef CRYPTO_CAESAR_H
#define CRYPTO_CAESAR_H

#include "common.h"

/**
 * @brief Ejecuta el cifrado o descifrado César byte a byte desde fd_in hacia fd_out.
 *
 * @param fd_in Descriptor de archivo origen (abierto para lectura).
 * @param fd_out Descriptor de archivo destino (abierto para escritura).
 * @param shift Cantidad de desplazamiento (clave numérica).
 * @param mode MODE_ENCRYPT para cifrar, MODE_DECRYPT para descifrar.
 * @return StatusCode Estado del proceso.
 */
StatusCode crypto_caesar_process(int fd_in, int fd_out, int shift, CryptoMode mode);

#endif /* CRYPTO_CAESAR_H */
