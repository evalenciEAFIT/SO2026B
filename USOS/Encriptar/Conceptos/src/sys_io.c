/**
 * @file sys_io.c
 * @brief Implementación de utilidades de E/S orientadas a llamadas al sistema (System Calls).
 *
 * En este archivo se implementa el principio de interacción directa con el Núcleo (Kernel)
 * de Linux a través de descriptores de archivos enteros (file descriptors).
 */

#include "sys_io.h"

void log_sys_error(const char *context) {
    if (context != NULL) {
        fprintf(stderr, "[ERROR SO] %s: %s (errno=%d)\n", context, strerror(errno), errno);
    } else {
        fprintf(stderr, "[ERROR SO] Error de sistema: %s (errno=%d)\n", strerror(errno), errno);
    }
}

const char* status_to_string(StatusCode code) {
    switch (code) {
        case STATUS_OK:
            return "Operación completada con éxito";
        case STATUS_ERR_INVALID_ARGS:
            return "Argumentos inválidos o insuficientes";
        case STATUS_ERR_OPEN_INPUT:
            return "Fallo en llamada al sistema open(2) para lectura";
        case STATUS_ERR_OPEN_OUTPUT:
            return "Fallo en llamada al sistema open(2) para escritura";
        case STATUS_ERR_READ:
            return "Error en llamada al sistema read(2)";
        case STATUS_ERR_WRITE:
            return "Error en llamada al sistema write(2)";
        case STATUS_ERR_STAT:
            return "Error al consultar metadatos con fstat(2)";
        case STATUS_ERR_MMAP:
            return "Fallo al mapear memoria virtual con mmap(2)";
        case STATUS_ERR_MEMORY:
            return "Memoria insuficiente en espacio de usuario (malloc)";
        case STATUS_ERR_ALGO_FAILED:
            return "Error interno en el algoritmo de cifrado";
        default:
            return "Error desconocido";
    }
}

int sys_open_read(const char *path) {
    if (!path) return -1;
    /*
     * Llamada al sistema open(2):
     * O_RDONLY: Solicita al kernel acceso de solo lectura al archivo.
     * Retorna un descriptor de archivo asignado en la File Descriptor Table del proceso.
     */
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        log_sys_error("Fallo al abrir archivo de entrada con open(O_RDONLY)");
    }
    return fd;
}

int sys_open_write(const char *path, mode_t mode) {
    if (!path) return -1;
    /*
     * Llamada al sistema open(2):
     * O_WRONLY: Escritura únicamente.
     * O_CREAT: Si el archivo no existe, el kernel lo crea en el sistema de archivos.
     * O_TRUNC: Si el archivo existe, su longitud es truncada a cero bytes.
     * mode: Permisos UNIX asignados al inodo si el archivo es creado (respetando umask).
     */
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, mode);
    if (fd < 0) {
        log_sys_error("Fallo al crear/abrir archivo de salida con open(O_WRONLY|O_CREAT|O_TRUNC)");
    }
    return fd;
}

StatusCode sys_get_stat(int fd, struct stat *st) {
    if (fd < 0 || !st) return STATUS_ERR_INVALID_ARGS;
    /*
     * Llamada al sistema fstat(2):
     * Obtiene los metadatos del inodo asociado al descriptor abierto en el VFS del kernel:
     * tamaño total (st_size), permisos (st_mode), tamaño óptimo de bloque (st_blksize).
     */
    if (fstat(fd, st) < 0) {
        log_sys_error("Fallo al invocar fstat(2)");
        return STATUS_ERR_STAT;
    }
    return STATUS_OK;
}

StatusCode sys_read_chunk(int fd, void *buf, size_t count, size_t *bytes_read) {
    if (fd < 0 || !buf || !bytes_read) return STATUS_ERR_INVALID_ARGS;

    size_t total = 0;
    uint8_t *ptr = (uint8_t *)buf;

    while (total < count) {
        /*
         * Llamada al sistema read(2):
         * Transfiere bytes desde el buffer de la Page Cache del kernel al buffer del usuario.
         * Puede retornar menos bytes de los solicitados (Short Read) o ser interrumpida por señal.
         */
        ssize_t n = read(fd, ptr + total, count - total);
        if (n < 0) {
            if (errno == EINTR) {
                /* Interrupción por señal POSIX: reintentar la llamada */
                continue;
            }
            log_sys_error("Fallo durante read(2)");
            *bytes_read = total;
            return STATUS_ERR_READ;
        }
        if (n == 0) {
            /* Fin de archivo (EOF - End Of File) alcanzado */
            break;
        }
        total += (size_t)n;
    }

    *bytes_read = total;
    return STATUS_OK;
}

StatusCode sys_write_chunk(int fd, const void *buf, size_t count) {
    if (fd < 0 || !buf) return STATUS_ERR_INVALID_ARGS;

    size_t total = 0;
    const uint8_t *ptr = (const uint8_t *)buf;

    while (total < count) {
        /*
         * Llamada al sistema write(2):
         * Transfiere datos desde el espacio de usuario a la caché de páginas del kernel.
         * Manejamos escrituras parciales (Short Writes) hasta escribir todos los bytes.
         */
        ssize_t n = write(fd, ptr + total, count - total);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            log_sys_error("Fallo durante write(2)");
            return STATUS_ERR_WRITE;
        }
        total += (size_t)n;
    }

    return STATUS_OK;
}

void sys_close_fd(int *fd) {
    if (fd && *fd >= 0) {
        /*
         * Llamada al sistema close(2):
         * Libera el descriptor de archivo de la tabla de descriptores del proceso.
         */
        if (close(*fd) < 0) {
            log_sys_error("Aviso al cerrar descriptor con close(2)");
        }
        *fd = -1;
    }
}

StatusCode sys_copy_stream(int fd_src, int fd_dst) {
    if (fd_src < 0 || fd_dst < 0) return STATUS_ERR_INVALID_ARGS;

    struct stat st;
    size_t buf_size = DEFAULT_BUFFER_SIZE;
    if (sys_get_stat(fd_src, &st) == STATUS_OK && st.st_blksize > 0) {
        buf_size = (size_t)st.st_blksize;
    }

    uint8_t *buffer = (uint8_t *)malloc(buf_size);
    if (!buffer) {
        log_sys_error("malloc falló al asignar buffer de copia");
        return STATUS_ERR_MEMORY;
    }

    size_t n_read = 0;
    StatusCode status = STATUS_OK;

    while (1) {
        status = sys_read_chunk(fd_src, buffer, buf_size, &n_read);
        if (status != STATUS_OK) break;
        if (n_read == 0) break; /* EOF */

        status = sys_write_chunk(fd_dst, buffer, n_read);
        if (status != STATUS_OK) break;
    }

    free(buffer);
    return status;
}
