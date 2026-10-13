/**
 * @file crypto_caesar.c
 * @brief Implementación del Cifrado César Extendido (Byte-Shift Substitution).
 *
 * Conceptos de Sistemas Operativos:
 * - E/S por bloques: En lugar de procesar byte por byte mediante llamadas individuales,
 *   se lee en bloques optimizados correspondientes al bloque de disco (st_blksize)
 *   o tamaño de página de memoria virtual (4096 bytes).
 * - Minimización de transiciones de contexto Usuario <-> Núcleo (Context Switches).
 */

#include "crypto_caesar.h"
#include "sys_io.h"

StatusCode crypto_caesar_process(int fd_in, int fd_out, int shift, CryptoMode mode) {
    if (fd_in < 0 || fd_out < 0) return STATUS_ERR_INVALID_ARGS;

    /*
     * En criptografía clásica César, el desplazamiento en módulo 256
     * permite cifrar cualquier archivo binario (PDF, imágenes, ejecutables, texto).
     */
    int effective_shift = shift % 256;
    if (effective_shift < 0) effective_shift += 256;

    if (mode == MODE_DECRYPT) {
        effective_shift = (256 - effective_shift) % 256;
    }

    /* Consultar metadatos para optimizar el tamaño de buffer al tamaño de bloque del SO */
    struct stat st;
    size_t buf_size = DEFAULT_BUFFER_SIZE;
    if (sys_get_stat(fd_in, &st) == STATUS_OK && st.st_blksize > 0) {
        buf_size = (size_t)st.st_blksize;
    }

    uint8_t *buffer = (uint8_t *)malloc(buf_size);
    if (!buffer) {
        log_sys_error("Fallo al reservar memoria para buffer de César");
        return STATUS_ERR_MEMORY;
    }

    size_t bytes_read = 0;
    StatusCode status = STATUS_OK;

    while (1) {
        status = sys_read_chunk(fd_in, buffer, buf_size, &bytes_read);
        if (status != STATUS_OK) break;
        if (bytes_read == 0) break; /* EOF */

        /* Transformación en espacio de usuario */
        for (size_t i = 0; i < bytes_read; i++) {
            buffer[i] = (uint8_t)((buffer[i] + effective_shift) & 0xFF);
        }

        /* Escritura al descriptor de salida mediante syscall write(2) */
        status = sys_write_chunk(fd_out, buffer, bytes_read);
        if (status != STATUS_OK) break;
    }

    free(buffer);
    return status;
}
