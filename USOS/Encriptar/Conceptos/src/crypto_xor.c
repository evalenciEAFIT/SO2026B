/**
 * @file crypto_xor.c
 * @brief Implementación del Cifrado XOR Polialfabético.
 *
 * Conceptos de Sistemas Operativos:
 * - Continuidad del flujo de datos (Stream) a través de fronteras de bloques.
 *   El índice de la clave debe preservarse correctamente independientemente
 *   de cuántos bytes devuelva cada llamada read(2).
 * - Eficiencia en el procesamiento a nivel de bit en la CPU.
 */

#include "crypto_xor.h"
#include "sys_io.h"

StatusCode crypto_xor_process(int fd_in, int fd_out, const uint8_t *key, size_t key_len, CryptoMode mode) {
    (void)mode; /* En XOR simétrico, la misma operación cifra y descifra: (A ^ B) ^ B = A */

    if (fd_in < 0 || fd_out < 0 || !key || key_len == 0) {
        return STATUS_ERR_INVALID_ARGS;
    }

    struct stat st;
    size_t buf_size = DEFAULT_BUFFER_SIZE;
    if (sys_get_stat(fd_in, &st) == STATUS_OK && st.st_blksize > 0) {
        buf_size = (size_t)st.st_blksize;
    }

    uint8_t *buffer = (uint8_t *)malloc(buf_size);
    if (!buffer) {
        log_sys_error("Fallo al reservar memoria para buffer de XOR");
        return STATUS_ERR_MEMORY;
    }

    size_t key_idx = 0;
    size_t bytes_read = 0;
    StatusCode status = STATUS_OK;

    while (1) {
        status = sys_read_chunk(fd_in, buffer, buf_size, &bytes_read);
        if (status != STATUS_OK) break;
        if (bytes_read == 0) break; /* EOF */

        /*
         * Operación XOR byte por byte con rotación de la clave.
         * El índice 'key_idx' se mantiene entre lecturas sucesivas para asegurar
         * que la secuencia de la clave no se desincronice en los límites del buffer.
         */
        for (size_t i = 0; i < bytes_read; i++) {
            buffer[i] ^= key[key_idx];
            key_idx = (key_idx + 1) % key_len;
        }

        status = sys_write_chunk(fd_out, buffer, bytes_read);
        if (status != STATUS_OK) break;
    }

    free(buffer);
    return status;
}
