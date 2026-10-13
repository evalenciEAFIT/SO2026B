/**
 * @file sys_io.h
 * @brief Capa de abstracción y utilidades de E/S orientada estrictamente a System Calls.
 *
 * Conceptos de Sistemas Operativos ilustrados:
 * - Llamadas al sistema POSIX: open(2), close(2), read(2), write(2), fstat(2), lseek(2).
 * - Manejo de lecturas/escrituras parciales (Short reads & short writes).
 * - Manejo de interrupciones de señal (EINTR).
 * - Preservación de metadatos de archivos (inodo, tamaño, permisos con struct stat).
 * - Detección del tamaño óptimo de bloque de E/S del sistema de archivos (st_blksize).
 */

#ifndef SYS_IO_H
#define SYS_IO_H

#include "common.h"

/**
 * @brief Abre un archivo de entrada para lectura utilizando la syscall open(2).
 *
 * @param path Ruta del archivo a abrir.
 * @return int Descriptor de archivo (fd >= 0) o -1 si hubo error.
 */
int sys_open_read(const char *path);

/**
 * @brief Crea o trunca un archivo de salida para escritura utilizando open(2).
 *
 * Utiliza las banderas O_WRONLY | O_CREAT | O_TRUNC y aplica los permisos especificados.
 *
 * @param path Ruta del archivo a crear.
 * @param mode Permisos en formato mode_t (por ejemplo 0644).
 * @return int Descriptor de archivo (fd >= 0) o -1 si hubo error.
 */
int sys_open_write(const char *path, mode_t mode);

/**
 * @brief Obtiene la información y metadatos de un archivo usando fstat(2).
 *
 * @param fd Descriptor de archivo.
 * @param st Puntero a estructura struct stat donde se depositan los metadatos.
 * @return StatusCode STATUS_OK o STATUS_ERR_STAT.
 */
StatusCode sys_get_stat(int fd, struct stat *st);

/**
 * @brief Lee de forma segura exactamente 'count' bytes o hasta alcanzar EOF.
 *
 * Garantiza que ante lecturas parciales o interrupciones por señales (EINTR),
 * la función continúe leyendo hasta completar el buffer o detectar fin de archivo.
 *
 * @param fd Descriptor de archivo.
 * @param buf Buffer de destino.
 * @param count Número máximo de bytes a leer.
 * @param bytes_read Puntero donde se almacenará el número real de bytes leídos.
 * @return StatusCode STATUS_OK o STATUS_ERR_READ.
 */
StatusCode sys_read_chunk(int fd, void *buf, size_t count, size_t *bytes_read);

/**
 * @brief Escribe de forma garantizada exactamente 'count' bytes en el descriptor.
 *
 * Maneja escrituras parciales y reintentos ante la señal EINTR.
 *
 * @param fd Descriptor de archivo.
 * @param buf Buffer de datos de origen.
 * @param count Número total de bytes a escribir.
 * @return StatusCode STATUS_OK o STATUS_ERR_WRITE.
 */
StatusCode sys_write_chunk(int fd, const void *buf, size_t count);

/**
 * @brief Cierra de forma segura un descriptor de archivo y lo invalida a -1.
 *
 * @param fd Puntero a la variable que contiene el descriptor de archivo.
 */
void sys_close_fd(int *fd);

/**
 * @brief Duplica el contenido de un archivo a otro usando llamadas al sistema
 * preservando tamaño de bloque óptimo (st_blksize).
 *
 * @param fd_src Descriptor origen.
 * @param fd_dst Descriptor destino.
 * @return StatusCode STATUS_OK en caso de éxito.
 */
StatusCode sys_copy_stream(int fd_src, int fd_dst);

#endif /* SYS_IO_H */
