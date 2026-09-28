/**
 * ============================================================================
 * Proyecto: Compresor y Descompresor de Archivos con Algoritmo de Huffman
 * Asignatura: Sistemas Operativos (4to Semestre)
 * 
 * Propósito del archivo de cabecera:
 *   Define los tipos de datos, constantes y prototipos de funciones
 *   en español para implementar el flujo completo de compresión
 *   y descompresión secuencial.
 * ============================================================================
 */

#ifndef HUFFMAN_H
#define HUFFMAN_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>     // Llamadas al sistema POSIX: read(), write(), close(), lseek()
#include <fcntl.h>      // Control de archivos y banderas: open(), O_RDONLY, O_WRONLY, O_CREAT
#include <sys/stat.h>   // Metadatos y llamadas de estado: stat(), mkdir(), permisos 0644/0755
#include <sys/types.h>  // Tipos primitivos del sistema: mode_t, off_t, ssize_t

// Cantidad máxima de símbolos posibles en un byte (de 0x00 a 0xFF)
#define MAX_SIMBOLOS 256

// Longitud máxima teórica del código binario asignado a un símbolo
#define MAX_LONGITUD_CODIGO 256

// Número identificador único de formato (Magic Number: "HUFF" en hexadecimal)
#define NUMERO_MAGICO_HUFFMAN 0x48554646

/*
 * ============================================================================
 * Estructuras de Datos
 * ============================================================================
 */

/**
 * Estructura para representar un nodo en el árbol binario de Huffman.
 * 
 * Concepto de SO / Estructuras:
 *   - Nodos hoja: representan un símbolo real (`simbolo`) y su frecuencia.
 *   - Nodos internos: no representan símbolos, almacenan la suma de
 *     frecuencias de sus dos hijos (`izq` y `der`).
 */
typedef struct NodoHuffman {
    unsigned char simbolo;          // Byte que representa (válido en hojas)
    uint32_t frecuencia;            // Cantidad de apariciones del símbolo
    struct NodoHuffman *izq;        // Puntero al hijo izquierdo (representa bit 0)
    struct NodoHuffman *der;        // Puntero al hijo derecho (representa bit 1)
} NodoHuffman;

/**
 * Estructura para almacenar la secuencia binaria correspondiente a un símbolo.
 * Se guarda como cadena de caracteres ('0' y '1') para facilitar la comprensión
 * y depuración visual por parte de los estudiantes.
 */
typedef struct {
    char codigo[MAX_LONGITUD_CODIGO]; // Cadena de caracteres '0' y '1'
    int longitud;                     // Número de bits efectivos en el código
} CodigoHuffman;

/**
 * Estructura para el diccionario completo de traducción (tabla de códigos).
 * Permite buscar en tiempo constante O(1) el código de cualquier byte (0 a 255).
 */
typedef struct {
    CodigoHuffman tabla[MAX_SIMBOLOS];
    bool presente[MAX_SIMBOLOS];       // Indica si el símbolo existe en el archivo
} Diccionario;

/*
 * ============================================================================
 * Prototipos de las Funciones Principales
 * ============================================================================
 */

/**
 * Función: leer_archivo
 * Propósito:
 *   Abre un archivo en modo binario, mide su tamaño con llamadas al sistema/libc
 *   (fseek/ftell), reserva memoria en el Heap (malloc) y carga el contenido completo.
 * 
 * Parámetros:
 *   - ruta_archivo: Cadena con la ruta del archivo que se desea leer.
 *   - tamano_salida: Puntero donde se almacenará el tamaño en bytes del archivo.
 * 
 * Retorno:
 *   - Puntero al búfer en memoria con los datos del archivo, o NULL si ocurrió un error.
 */
unsigned char* leer_archivo(const char *ruta_archivo, size_t *tamano_salida);

/**
 * Función: contar_frecuencias
 * Propósito:
 *   Recorre el búfer de datos byte por byte e incrementa el contador de apariciones
 *   de cada valor (0 a 255), generando la tabla de frecuencias (histograma).
 * 
 * Parámetros:
 *   - datos: Búfer en memoria con los bytes leídos.
 *   - tamano_datos: Cantidad total de bytes en el búfer.
 *   - frecuencias: Arreglo de 256 enteros donde se almacenan los conteos.
 */
void contar_frecuencias(const unsigned char *datos, 
                        size_t tamano_datos, 
                        uint32_t frecuencias[MAX_SIMBOLOS]);

/**
 * Función: crear_arbol_huffman
 * Propósito:
 *   Construye el árbol binario de codificación óptima mediante un algoritmo voraz (Greedy),
 *   combinando reiteradamente los dos nodos de menor frecuencia.
 * 
 * Parámetros:
 *   - frecuencias: Arreglo de conteos de apariciones para cada símbolo.
 * 
 * Retorno:
 *   - Puntero a la raíz del árbol de Huffman creado, o NULL si el archivo estaba vacío.
 */
NodoHuffman* crear_arbol_huffman(const uint32_t frecuencias[MAX_SIMBOLOS]);

/**
 * Función: generar_codigos
 * Propósito:
 *   Recorre el árbol de Huffman de forma recursiva (recorrido en profundidad)
 *   para construir la cadena de bits ('0' a la izquierda, '1' a la derecha)
 *   y registrarla en el diccionario para cada símbolo presente.
 * 
 * Parámetros:
 *   - raiz: Puntero a la raíz del árbol de Huffman.
 *   - diccionario: Estructura donde se guardan los códigos generados.
 */
void generar_codigos(NodoHuffman *raiz, Diccionario *diccionario);

/**
 * Función: guardar_comprimido
 * Propósito:
 *   Genera el archivo final comprimido. Escribe la cabecera (número mágico,
 *   tamaño original y frecuencias de los tokens) y luego escribe el flujo
 *   de bits comprimidos empaquetados en bytes.
 * 
 * Parámetros:
 *   - ruta_salida: Ruta del archivo comprimido destino (ej: archivo.huf).
 *   - datos_originales: Búfer con los datos originales sin comprimir.
 *   - tamano_original: Cantidad exacta de bytes del archivo original.
 *   - frecuencias: Tabla de frecuencias requerida para que el descompresor
 *                  pueda reconstruir exactamente el mismo árbol.
 *   - diccionario: Tabla de códigos generada.
 * 
 * Retorno:
 *   - 0 si tuvo éxito, o -1 si ocurrió algún error de E/S.
 */
int guardar_comprimido(const char *ruta_salida, 
                       const unsigned char *datos_originales, 
                       size_t tamano_original, 
                       const uint32_t frecuencias[MAX_SIMBOLOS], 
                       const Diccionario *diccionario);

/**
 * Función: descomprimir_archivo
 * Propósito:
 *   Lee un archivo comprimido, verifica su número mágico, lee el tamaño original,
 *   recupera la tabla de frecuencias, reconstruye el árbol de Huffman y decodifica
 *   el flujo de bits hasta recuperar el archivo idéntico original.
 * 
 * Parámetros:
 *   - ruta_comprimido: Ruta del archivo comprimido de entrada.
 *   - ruta_descomprimido: Ruta del archivo donde se restaurará la información original.
 * 
 * Retorno:
 *   - 0 si tuvo éxito, o -1 si ocurrió algún error de validación o E/S.
 */
int descomprimir_archivo(const char *ruta_comprimido, const char *ruta_descomprimido);

/**
 * Función: liberar_arbol
 * Propósito:
 *   Libera de la memoria dinámica (Heap) todos los nodos creados para el árbol
 *   utilizando un recorrido post-orden (hijo izquierdo, hijo derecho, nodo actual).
 * 
 * Parámetros:
 *   - raiz: Puntero al nodo raíz del árbol a liberar.
 */
void liberar_arbol(NodoHuffman *raiz);

/**
 * Función: asegurar_directorio_padre
 * Propósito:
 *   Inspecciona la ruta de un archivo de salida. Si incluye subdirectorios
 *   (por ejemplo: "salidas/resultado.huf"), invoca llamadas al sistema (mkdir)
 *   para garantizar que la carpeta de destino exista antes de intentar escribir.
 * 
 * Parámetros:
 *   - ruta_archivo: Ruta relativa o absoluta del archivo de salida.
 */
void asegurar_directorio_padre(const char *ruta_archivo);

/**
 * Función: registrar_archivo_salida
 * Propósito:
 *   Registra en un archivo de bitácora/manifiesto (por defecto "salidas/registro_salidas.txt")
 *   la información detallada de cada archivo generado: fecha/hora, operación,
 *   rutas de origen y destino, tamaños en bytes y porcentaje de reducción.
 * 
 * Parámetros:
 *   - ruta_registro: Ruta del archivo donde se guarda el historial (log).
 *   - operacion: Tipo de operación realizada ("COMPRESIÓN" o "DESCOMPRESIÓN").
 *   - ruta_origen: Archivo de entrada procesado.
 *   - ruta_destino: Archivo generado en el sistema de archivos.
 *   - tamano_origen: Tamaño en bytes del archivo de entrada.
 *   - tamano_destino: Tamaño en bytes del archivo de salida.
 *   - porcentaje_ahorro: Porcentaje de espacio ahorrado (solo compresión).
 */
void registrar_archivo_salida(const char *ruta_registro,
                              const char *operacion,
                              const char *ruta_origen,
                              const char *ruta_destino,
                              size_t tamano_origen,
                              long tamano_destino,
                              double porcentaje_ahorro);

#endif // HUFFMAN_H
