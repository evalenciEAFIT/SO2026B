/**
 * @file crypto_xor.h
 * @brief Algoritmo de Cifrado XOR Polialfabético (Stream con clave repetitiva).
 *
 * Conceptos de Sistemas Operativos:
 * - Continuidad de estado a través de límites de bloques de E/S.
 * - Operaciones a nivel de bit directas sobre memoria física/virtual.
 * - Simetría e involución de la operación XOR: E(D(x)) = x.
 */

#ifndef CRYPTO_XOR_H
#define CRYPTO_XOR_H

#include "common.h"

/**
 * @brief Procesa el archivo aplicando XOR con una clave cíclica.
 *
 * Al ser XOR simétrico e involutivo, la misma función sirve para cifrar y descifrar,
 * pero se respeta la firma con el parámetro mode por consistencia arquitectónica.
 *
 * @param fd_in Descriptor de archivo origen.
 * @param fd_out Descriptor de archivo destino.
 * @param key Clave secreta (cadena de caracteres o secuencia de bytes).
 * @param key_len Longitud de la clave en bytes.
 * @param mode Modo de operación (MODE_ENCRYPT o MODE_DECRYPT).
 * @return StatusCode Estado del proceso.
 */
StatusCode crypto_xor_process(int fd_in, int fd_out, const uint8_t *key, size_t key_len, CryptoMode mode);

#endif /* CRYPTO_XOR_H */
