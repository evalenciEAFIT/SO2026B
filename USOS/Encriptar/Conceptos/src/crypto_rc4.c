/**
 * @file crypto_rc4.c
 * @brief Implementación del Cifrado de Flujo RC4 (Rivest Cipher 4).
 *
 * Conceptos de Sistemas Operativos:
 * - Mantenimiento de estado criptográfico en espacio de usuario.
 * - Desacoplamiento entre la generación del flujo de cifrado pseudoaleatorio
 *   y las dimensiones de los bloques físicos de lectura/escritura del disco.
 * - Simetría de cifrado y descifrado mediante operaciones de flujo XOR.
 */

#include "crypto_rc4.h"
#include "sys_io.h"

static inline void swap_bytes(uint8_t *a, uint8_t *b) {
    uint8_t tmp = *a;
    *a = *b;
    *b = tmp;
}

void rc4_init(RC4Context *ctx, const uint8_t *key, size_t key_len) {
    if (!ctx || !key || key_len == 0) return;

    /* 1. Inicialización de la permutación identidad (S-Box) */
    for (int i = 0; i < 256; i++) {
        ctx->S[i] = (uint8_t)i;
    }

    /* 2. Algoritmo de programación de claves (KSA - Key-Scheduling Algorithm) */
    uint8_t j = 0;
    for (int i = 0; i < 256; i++) {
        j = (uint8_t)(j + ctx->S[i] + key[i % key_len]);
        swap_bytes(&ctx->S[i], &ctx->S[j]);
    }

    ctx->i = 0;
    ctx->j = 0;
}

void rc4_crypt(RC4Context *ctx, uint8_t *data, size_t len) {
    if (!ctx || !data) return;

    /* Algoritmo de generación pseudoaleatoria (PRGA) */
    for (size_t n = 0; n < len; n++) {
        ctx->i = (uint8_t)(ctx->i + 1);
        ctx->j = (uint8_t)(ctx->j + ctx->S[ctx->i]);
        swap_bytes(&ctx->S[ctx->i], &ctx->S[ctx->j]);

        uint8_t k = ctx->S[(uint8_t)(ctx->S[ctx->i] + ctx->S[ctx->j])];
        data[n] ^= k;
    }
}

StatusCode crypto_rc4_process(int fd_in, int fd_out, const uint8_t *key, size_t key_len, CryptoMode mode) {
    (void)mode; /* RC4 es simétrico: la misma secuencia de flujo descifra */

    if (fd_in < 0 || fd_out < 0 || !key || key_len == 0) {
        return STATUS_ERR_INVALID_ARGS;
    }

    RC4Context ctx;
    rc4_init(&ctx, key, key_len);

    struct stat st;
    size_t buf_size = DEFAULT_BUFFER_SIZE;
    if (sys_get_stat(fd_in, &st) == STATUS_OK && st.st_blksize > 0) {
        buf_size = (size_t)st.st_blksize;
    }

    uint8_t *buffer = (uint8_t *)malloc(buf_size);
    if (!buffer) {
        log_sys_error("Fallo al reservar memoria para buffer de RC4");
        return STATUS_ERR_MEMORY;
    }

    size_t bytes_read = 0;
    StatusCode status = STATUS_OK;

    while (1) {
        status = sys_read_chunk(fd_in, buffer, buf_size, &bytes_read);
        if (status != STATUS_OK) break;
        if (bytes_read == 0) break; /* EOF */

        /* Cifrado/Descifrado continuo */
        rc4_crypt(&ctx, buffer, bytes_read);

        status = sys_write_chunk(fd_out, buffer, bytes_read);
        if (status != STATUS_OK) break;
    }

    free(buffer);
    return status;
}
