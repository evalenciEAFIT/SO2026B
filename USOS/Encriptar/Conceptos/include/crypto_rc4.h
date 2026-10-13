/**
 * @file crypto_rc4.h
 * @brief Algoritmo de Cifrado de Flujo RC4 (Rivest Cipher 4 / ARC4).
 *
 * Conceptos de Sistemas Operativos:
 * - Algoritmo de generación de flujo pseudoaleatorio (PRGA) independiente del bloque de disco.
 * - Sincronización del estado interno del cifrador (S-Box de 256 bytes) en espacio de usuario.
 * - Integración con flujos de llamadas al sistema read(2) y write(2).
 */

#ifndef CRYPTO_RC4_H
#define CRYPTO_RC4_H

#include "common.h"

/**
 * @brief Estructura que almacena el estado interno del cifrador RC4.
 */
typedef struct {
    uint8_t S[256];
    uint8_t i;
    uint8_t j;
} RC4Context;

/**
 * @brief Inicializa el contexto RC4 mediante el algoritmo KSA (Key-Scheduling Algorithm).
 *
 * @param ctx Puntero al contexto RC4.
 * @param key Clave secreta.
 * @param key_len Longitud de la clave en bytes (típicamente 1 a 256 bytes).
 */
void rc4_init(RC4Context *ctx, const uint8_t *key, size_t key_len);

/**
 * @brief Cifra/Descifra un bloque de memoria generando el flujo PRGA.
 *
 * @param ctx Puntero al contexto RC4 activo.
 * @param data Buffer de datos de entrada/salida (modificado in-situ).
 * @param len Número de bytes a procesar.
 */
void rc4_crypt(RC4Context *ctx, uint8_t *data, size_t len);

/**
 * @brief Procesa un archivo completo de entrada a salida usando RC4 y System Calls.
 *
 * @param fd_in Descriptor de archivo origen.
 * @param fd_out Descriptor de archivo destino.
 * @param key Clave de cifrado.
 * @param key_len Longitud de la clave.
 * @param mode Modo de operación (MODE_ENCRYPT o MODE_DECRYPT).
 * @return StatusCode Estado del proceso.
 */
StatusCode crypto_rc4_process(int fd_in, int fd_out, const uint8_t *key, size_t key_len, CryptoMode mode);

#endif /* CRYPTO_RC4_H */
