/**
 * @file crypto_transposition.c
 * @brief Implementación del Cifrado por Transposición Columnar por Bloques.
 *
 * Conceptos de Sistemas Operativos:
 * - Procesamiento por bloques: A diferencia de los cifradores de flujo (Stream),
 *   los cifradores de bloque requieren agrupar los datos en matrices fijas en memoria.
 * - Cabeceras de archivo personalizadas: Inclusión de metadatos (Magic Number y
 *   tamaño original en bytes) para permitir el manejo exacto de Padding sin alterar
 *   el tamaño original del archivo al descifrar.
 * - Manejo de memoria dinámica (Heap) y buffers dimensionales.
 */

#include "crypto_transposition.h"
#include "sys_io.h"

#define TRANSP_MAGIC 0x54525350 /* "TRSP" en ASCII */
#define ROWS_PER_BLOCK 64

#pragma pack(push, 1)
typedef struct {
    uint32_t magic;         /* Número mágico 'TRSP' */
    uint64_t original_size; /* Tamaño exacto original del archivo */
    uint32_t col_count;     /* Número de columnas derivado de la clave */
} TranspositionHeader;
#pragma pack(pop)

/**
 * @brief Estructura auxiliar para ordenar las columnas alfabéticamente.
 */
typedef struct {
    char ch;
    size_t original_idx;
} KeyColumn;

static int compare_key_cols(const void *a, const void *b) {
    const KeyColumn *ka = (const KeyColumn *)a;
    const KeyColumn *kb = (const KeyColumn *)b;
    if (ka->ch != kb->ch) {
        return (int)(unsigned char)ka->ch - (int)(unsigned char)kb->ch;
    }
    return (ka->original_idx < kb->original_idx) ? -1 : 1;
}

/**
 * @brief Genera la permutación de columnas a partir de una clave textual.
 */
static int* create_permutation(const char *key, size_t num_cols) {
    KeyColumn *cols = (KeyColumn *)malloc(num_cols * sizeof(KeyColumn));
    int *perm = (int *)malloc(num_cols * sizeof(int));
    if (!cols || !perm) {
        free(cols);
        free(perm);
        return NULL;
    }

    for (size_t i = 0; i < num_cols; i++) {
        cols[i].ch = key[i];
        cols[i].original_idx = i;
    }

    qsort(cols, num_cols, sizeof(KeyColumn), compare_key_cols);

    for (size_t i = 0; i < num_cols; i++) {
        perm[i] = (int)cols[i].original_idx;
    }

    free(cols);
    return perm;
}

static StatusCode encrypt_transposition(int fd_in, int fd_out, const char *key, size_t num_cols) {
    struct stat st;
    if (sys_get_stat(fd_in, &st) != STATUS_OK) {
        return STATUS_ERR_STAT;
    }

    int *perm = create_permutation(key, num_cols);
    if (!perm) return STATUS_ERR_MEMORY;

    /* Escribir encabezado con metadatos del archivo */
    TranspositionHeader hdr;
    hdr.magic = TRANSP_MAGIC;
    hdr.original_size = (uint64_t)st.st_size;
    hdr.col_count = (uint32_t)num_cols;

    StatusCode status = sys_write_chunk(fd_out, &hdr, sizeof(hdr));
    if (status != STATUS_OK) {
        free(perm);
        return status;
    }

    size_t block_bytes = ROWS_PER_BLOCK * num_cols;
    uint8_t *in_block = (uint8_t *)malloc(block_bytes);
    uint8_t *out_block = (uint8_t *)malloc(block_bytes);

    if (!in_block || !out_block) {
        free(perm);
        free(in_block);
        free(out_block);
        return STATUS_ERR_MEMORY;
    }

    size_t bytes_read = 0;
    while (1) {
        status = sys_read_chunk(fd_in, in_block, block_bytes, &bytes_read);
        if (status != STATUS_OK) break;
        if (bytes_read == 0) break; /* EOF */

        /* Relleno (Padding) con ceros en el último bloque */
        if (bytes_read < block_bytes) {
            memset(in_block + bytes_read, 0, block_bytes - bytes_read);
        }

        /*
         * Transposición columnar:
         * La matriz tiene ROWS_PER_BLOCK filas y num_cols columnas.
         * Se leen las filas normalmente, pero se transponen por columnas permutadas.
         */
        size_t out_idx = 0;
        for (size_t c = 0; c < num_cols; c++) {
            int src_col = perm[c];
            for (size_t r = 0; r < ROWS_PER_BLOCK; r++) {
                out_block[out_idx++] = in_block[r * num_cols + src_col];
            }
        }

        status = sys_write_chunk(fd_out, out_block, block_bytes);
        if (status != STATUS_OK) break;
    }

    free(perm);
    free(in_block);
    free(out_block);
    return status;
}

static StatusCode decrypt_transposition(int fd_in, int fd_out, const char *key, size_t num_cols) {
    TranspositionHeader hdr;
    size_t bytes_read = 0;

    StatusCode status = sys_read_chunk(fd_in, &hdr, sizeof(hdr), &bytes_read);
    if (status != STATUS_OK) return status;
    if (bytes_read != sizeof(hdr) || hdr.magic != TRANSP_MAGIC) {
        fprintf(stderr, "[ERROR] El archivo no tiene la cabecera válida de transposición (TRSP).\n");
        return STATUS_ERR_ALGO_FAILED;
    }

    if (hdr.col_count != (uint32_t)num_cols) {
        fprintf(stderr, "[AVISO] La clave provista genera %zu columnas pero el archivo fue cifrado con %u columnas.\n",
                num_cols, hdr.col_count);
    }

    int *perm = create_permutation(key, num_cols);
    if (!perm) return STATUS_ERR_MEMORY;

    size_t block_bytes = ROWS_PER_BLOCK * num_cols;
    uint8_t *in_block = (uint8_t *)malloc(block_bytes);
    uint8_t *out_block = (uint8_t *)malloc(block_bytes);

    if (!in_block || !out_block) {
        free(perm);
        free(in_block);
        free(out_block);
        return STATUS_ERR_MEMORY;
    }

    uint64_t remaining = hdr.original_size;

    while (remaining > 0) {
        status = sys_read_chunk(fd_in, in_block, block_bytes, &bytes_read);
        if (status != STATUS_OK) break;
        if (bytes_read == 0) break; /* EOF inesperado */

        /* Reconstrucción inversa de la matriz */
        for (size_t c = 0; c < num_cols; c++) {
            int target_col = perm[c];
            for (size_t r = 0; r < ROWS_PER_BLOCK; r++) {
                uint8_t byte = in_block[c * ROWS_PER_BLOCK + r];
                out_block[r * num_cols + target_col] = byte;
            }
        }

        size_t to_write = (remaining < (uint64_t)block_bytes) ? (size_t)remaining : block_bytes;
        status = sys_write_chunk(fd_out, out_block, to_write);
        if (status != STATUS_OK) break;

        remaining -= to_write;
    }

    free(perm);
    free(in_block);
    free(out_block);
    return status;
}

StatusCode crypto_transposition_process(int fd_in, int fd_out, const char *key, CryptoMode mode) {
    if (fd_in < 0 || fd_out < 0 || !key) return STATUS_ERR_INVALID_ARGS;

    size_t num_cols = strlen(key);
    if (num_cols < 2) {
        fprintf(stderr, "[ERROR] La clave de transposición debe tener al menos 2 caracteres.\n");
        return STATUS_ERR_INVALID_ARGS;
    }

    if (mode == MODE_ENCRYPT) {
        return encrypt_transposition(fd_in, fd_out, key, num_cols);
    } else {
        return decrypt_transposition(fd_in, fd_out, key, num_cols);
    }
}
