/**
 * @file crypto_mmap.c
 * @brief Implementación de Cifrado basado en Mapeo de Memoria (mmap System Call).
 *
 * Conceptos de Sistemas Operativos demostrados:
 * - Llamada al sistema mmap(2):
 *   El núcleo mapea las páginas del archivo directamente en el espacio de direcciones
 *   virtuales del proceso. No hay necesidad de alojar buffers intermedios en malloc
 *   ni ejecutar lecturas o escrituras read(2)/write(2) repetidas.
 * - Copia cero (Zero-Copy):
 *   El procesador accede directamente a la Page Cache del kernel. Los fallos de página
 *   (Page Faults) son resueltos por el subsistema de memoria virtual de Linux.
 * - Llamada al sistema ftruncate(2):
 *   Modifica el tamaño en inodo del archivo en el sistema de archivos antes del mapeo.
 * - Llamada al sistema msync(2) con MS_SYNC:
 *   Fuerza la sincronización inmediata de las páginas sucias (dirty pages) al disco físico.
 * - Llamada al sistema munmap(2):
 *   Desvincula el rango de direcciones virtuales del proceso.
 */

#include "crypto_mmap.h"
#include "sys_io.h"

static void apply_mmap_cipher(const uint8_t *src, uint8_t *dst, size_t size,
                              const uint8_t *key, size_t key_len) {
    for (size_t i = 0; i < size; i++) {
        uint8_t k = key[i % key_len];
        /*
         * Cifrador simétrico de byte:
         * Inversión de bits y XOR con la clave rotada.
         * Involutivo: (byte ^ ~k) ^ ~k = byte
         */
        dst[i] = src[i] ^ (uint8_t)(~k);
    }
}

StatusCode crypto_mmap_process(const char *path_in, const char *path_out,
                               const uint8_t *key, size_t key_len,
                               CryptoMode mode) {
    (void)mode; /* El algoritmo aplicado es involutivo (simétrico) */

    if (!path_in || !key || key_len == 0) {
        return STATUS_ERR_INVALID_ARGS;
    }

    bool in_place = (path_out == NULL || strcmp(path_in, path_out) == 0);

    if (in_place) {
        /*
         * MODO 1: Cifrado In-Situ (In-Place) usando mmap(MAP_SHARED).
         * Modifica las páginas del archivo directamente en almacenamiento secundario.
         */
        int fd = open(path_in, O_RDWR);
        if (fd < 0) {
            log_sys_error("mmap in-place: open(O_RDWR) falló");
            return STATUS_ERR_OPEN_INPUT;
        }

        struct stat st;
        if (sys_get_stat(fd, &st) != STATUS_OK) {
            sys_close_fd(&fd);
            return STATUS_ERR_STAT;
        }

        if (st.st_size == 0) {
            /* Archivo vacío: nada que mapear ni cifrar */
            sys_close_fd(&fd);
            return STATUS_OK;
        }

        /* Mapear archivo en memoria virtual con permisos de Lectura y Escritura */
        uint8_t *mapped_addr = (uint8_t *)mmap(NULL, (size_t)st.st_size,
                                               PROT_READ | PROT_WRITE,
                                               MAP_SHARED, fd, 0);
        if (mapped_addr == MAP_FAILED) {
            log_sys_error("mmap(MAP_SHARED, PROT_READ|PROT_WRITE) falló");
            sys_close_fd(&fd);
            return STATUS_ERR_MMAP;
        }

        /* Procesamiento directo en la memoria virtual del proceso */
        apply_mmap_cipher(mapped_addr, mapped_addr, (size_t)st.st_size, key, key_len);

        /* Sincronización explícita de páginas sucias (dirty pages) a disco */
        if (msync(mapped_addr, (size_t)st.st_size, MS_SYNC) < 0) {
            log_sys_error("msync(MS_SYNC) falló");
        }

        /* Desmapear y liberar rango de memoria virtual */
        if (munmap(mapped_addr, (size_t)st.st_size) < 0) {
            log_sys_error("munmap falló");
        }

        sys_close_fd(&fd);
        return STATUS_OK;

    } else {
        /*
         * MODO 2: Cifrado desde archivo origen a archivo destino usando dos mapeos mmap.
         */
        int fd_in = open(path_in, O_RDONLY);
        if (fd_in < 0) {
            log_sys_error("mmap copy: open(fd_in, O_RDONLY) falló");
            return STATUS_ERR_OPEN_INPUT;
        }

        struct stat st_in;
        if (sys_get_stat(fd_in, &st_in) != STATUS_OK) {
            sys_close_fd(&fd_in);
            return STATUS_ERR_STAT;
        }

        /* Crear o truncar archivo destino con los mismos permisos del archivo origen */
        int fd_out = open(path_out, O_RDWR | O_CREAT | O_TRUNC, st_in.st_mode);
        if (fd_out < 0) {
            log_sys_error("mmap copy: open(fd_out, O_RDWR|O_CREAT|O_TRUNC) falló");
            sys_close_fd(&fd_in);
            return STATUS_ERR_OPEN_OUTPUT;
        }

        if (st_in.st_size == 0) {
            /* Archivo vacío */
            sys_close_fd(&fd_in);
            sys_close_fd(&fd_out);
            return STATUS_OK;
        }

        /*
         * Syscall ftruncate(2):
         * Dimensiona el archivo destino antes de mapearlo. Si se mapeara un archivo de 0 bytes,
         * cualquier intento de acceso generaría la señal SIGBUS (Bus error).
         */
        if (ftruncate(fd_out, st_in.st_size) < 0) {
            log_sys_error("ftruncate(fd_out) falló");
            sys_close_fd(&fd_in);
            sys_close_fd(&fd_out);
            return STATUS_ERR_WRITE;
        }

        /* Mapear archivo de entrada para solo lectura */
        const uint8_t *in_map = (const uint8_t *)mmap(NULL, (size_t)st_in.st_size,
                                                      PROT_READ, MAP_SHARED, fd_in, 0);
        if (in_map == MAP_FAILED) {
            log_sys_error("mmap(fd_in) falló");
            sys_close_fd(&fd_in);
            sys_close_fd(&fd_out);
            return STATUS_ERR_MMAP;
        }

        /* Mapear archivo de salida para lectura y escritura compartida */
        uint8_t *out_map = (uint8_t *)mmap(NULL, (size_t)st_in.st_size,
                                           PROT_READ | PROT_WRITE, MAP_SHARED, fd_out, 0);
        if (out_map == MAP_FAILED) {
            log_sys_error("mmap(fd_out) falló");
            munmap((void *)in_map, (size_t)st_in.st_size);
            sys_close_fd(&fd_in);
            sys_close_fd(&fd_out);
            return STATUS_ERR_MMAP;
        }

        /* Transformación directa entre mapas de memoria */
        apply_mmap_cipher(in_map, out_map, (size_t)st_in.st_size, key, key_len);

        /* Sincronizar cambios a disco con msync */
        msync(out_map, (size_t)st_in.st_size, MS_SYNC);

        /* Desmapear ambas regiones */
        munmap((void *)in_map, (size_t)st_in.st_size);
        munmap(out_map, (size_t)st_in.st_size);

        sys_close_fd(&fd_in);
        sys_close_fd(&fd_out);
        return STATUS_OK;
    }
}
