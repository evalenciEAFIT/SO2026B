/**
 * ============================================================================
 * Proyecto: Compresor y Descompresor de Archivos con Algoritmo de Huffman
 * Asignatura: Sistemas Operativos (4to Semestre)
 * 
 * Propósito del archivo principal (main.c):
 *   Interfaz por línea de comandos (CLI) con diseño estético, banners,
 *   colores ANSI y barras visuales de métricas para facilitar el seguimiento
 *   del pipeline de compresión y descompresión a los estudiantes.
 * ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "huffman.h"

// Definición de Secuencias de Escape ANSI para Colores y Estilos en Terminal
#define COLOR_REINICIAR "\033[0m"
#define COLOR_NEGRITA   "\033[1m"
#define COLOR_ATENUADO  "\033[2m"

#define COLOR_ROJO      "\033[1;31m"
#define COLOR_VERDE     "\033[1;32m"
#define COLOR_AMARILLO  "\033[1;33m"
#define COLOR_AZUL      "\033[1;34m"
#define COLOR_MAGENTA   "\033[1;35m"
#define COLOR_CIAN      "\033[1;36m"
#define COLOR_BLANCO    "\033[1;37m"

// Archivo donde se registra el historial y ubicación de todas las salidas generadas
#define RUTA_ARCHIVO_REGISTRO "salidas/registro_salidas.txt"

/**
 * Imprime un encabezado institucional / académico estilizado.
 */
static void imprimir_banner_superior(void) {
    printf("\n%s%s╭────────────────────────────────────────────────────────────────────────╮%s\n", COLOR_CIAN, COLOR_NEGRITA, COLOR_REINICIAR);
    printf("%s%s│      SISTEMAS OPERATIVOS - COMPRESIÓN DE ARCHIVOS CON HUFFMAN          │%s\n", COLOR_CIAN, COLOR_NEGRITA, COLOR_REINICIAR);
    printf("%s%s│            Implementación Secuencial Didáctica en Lenguaje C           │%s\n", COLOR_CIAN, COLOR_ATENUADO, COLOR_REINICIAR);
    printf("%s%s╰────────────────────────────────────────────────────────────────────────╯%s\n\n", COLOR_CIAN, COLOR_NEGRITA, COLOR_REINICIAR);
}

/**
 * Función auxiliar: imprimir_simbolo_legible
 * Imprime caracteres no imprimibles con etiquetas claras y color.
 */
static void imprimir_simbolo_legible(unsigned char simbolo) {
    if (simbolo == ' ') {
        printf("%s' ' (espacio)   %s", COLOR_AMARILLO, COLOR_REINICIAR);
    } else if (simbolo == '\n') {
        printf("%s'\\n' (salto)    %s", COLOR_AMARILLO, COLOR_REINICIAR);
    } else if (simbolo == '\t') {
        printf("%s'\\t' (tab)      %s", COLOR_AMARILLO, COLOR_REINICIAR);
    } else if (simbolo == '\r') {
        printf("%s'\\r' (retorno)  %s", COLOR_AMARILLO, COLOR_REINICIAR);
    } else if (isprint(simbolo)) {
        printf("'%c'             ", simbolo);
    } else {
        printf("%s0x%02X (binario) %s", COLOR_ATENUADO, simbolo, COLOR_REINICIAR);
    }
}

/**
 * Dibuja una barra visual de ahorro de espacio porcentual en consola.
 */
static void imprimir_barra_progreso(double porcentaje) {
    int ancho_barra = 20;
    int bloques_llenos = 0;
    
    if (porcentaje > 0) {
        bloques_llenos = (int)((porcentaje / 100.0) * ancho_barra);
        if (bloques_llenos > ancho_barra) bloques_llenos = ancho_barra;
    }

    printf("[");
    for (int i = 0; i < ancho_barra; i++) {
        if (i < bloques_llenos) {
            printf("%s█%s", COLOR_VERDE, COLOR_REINICIAR);
        } else {
            printf("%s░%s", COLOR_ATENUADO, COLOR_REINICIAR);
        }
    }
    printf("]");
}

/**
 * Imprime las instrucciones de uso en caso de error de sintaxis en la llamada.
 */
static void imprimir_uso_programa(const char *nombre_programa) {
    imprimir_banner_superior();
    printf("%s%sModo de uso correcto:%s\n", COLOR_AMARILLO, COLOR_NEGRITA, COLOR_REINICIAR);
    printf("  %s• Comprimir un archivo:%s\n", COLOR_BLANCO, COLOR_REINICIAR);
    printf("      %s%s -c <archivo_origen> <archivo_destino.huf>%s\n\n", COLOR_VERDE, nombre_programa, COLOR_REINICIAR);
    printf("  %s• Descomprimir un archivo:%s\n", COLOR_BLANCO, COLOR_REINICIAR);
    printf("      %s%s -d <archivo_origen.huf> <archivo_recuperado>%s\n\n", COLOR_AZUL, nombre_programa, COLOR_REINICIAR);
    printf("%sEjemplo rápido:%s\n", COLOR_AMARILLO, COLOR_REINICIAR);
    printf("  %s -c documento.txt documento.huf\n", nombre_programa);
    printf("  %s -d documento.huf restaurado.txt\n\n", nombre_programa);
}

int main(int cantidad_argumentos, char *argumentos[]) {
    // Validar cantidad de argumentos suministrados por el sistema operativo
    if (cantidad_argumentos != 4) {
        imprimir_uso_programa(argumentos[0]);
        return EXIT_FAILURE;
    }

    const char *modo_operacion = argumentos[1];
    const char *ruta_origen = argumentos[2];
    const char *ruta_destino = argumentos[3];

    // ========================================================================
    // MODO COMPRESIÓN (-c)
    // ========================================================================
    if (strcmp(modo_operacion, "-c") == 0) {
        imprimir_banner_superior();
        printf("%s%s┌── [MODO COMPRESIÓN] ──────────────────────────────────────────────────┐%s\n", COLOR_AZUL, COLOR_NEGRITA, COLOR_REINICIAR);
        printf("%s│%s  Archivo de origen:  %s%-50s%s%s│%s\n", COLOR_AZUL, COLOR_BLANCO, COLOR_VERDE, ruta_origen, COLOR_REINICIAR, COLOR_AZUL, COLOR_REINICIAR);
        printf("%s│%s  Archivo de destino: %s%-50s%s%s│%s\n", COLOR_AZUL, COLOR_BLANCO, COLOR_CIAN, ruta_destino, COLOR_REINICIAR, COLOR_AZUL, COLOR_REINICIAR);
        printf("%s└────────────────────────────────────────────────────────────────────────┘%s\n\n", COLOR_AZUL, COLOR_REINICIAR);

        // PASO 1: Leer el archivo completo
        printf("%s[Paso 1/5]%s %sLeyendo archivo completo a memoria RAM (Heap)...%s\n", COLOR_AMARILLO, COLOR_REINICIAR, COLOR_NEGRITA, COLOR_REINICIAR);
        size_t tamano_original = 0;
        unsigned char *bufer_datos = leer_archivo(ruta_origen, &tamano_original);
        if (!bufer_datos && tamano_original > 0) {
            printf("  %s[✗] ERROR: No se pudo leer el archivo de entrada '%s'.%s\n", COLOR_ROJO, ruta_origen, COLOR_REINICIAR);
            return EXIT_FAILURE;
        }
        printf("  %s✓%s Tamaño leído: %s%zu bytes%s\n\n", COLOR_VERDE, COLOR_REINICIAR, COLOR_BLANCO, tamano_original, COLOR_REINICIAR);

        // PASO 2: Contar frecuencias por cada byte
        printf("%s[Paso 2/5]%s %sCalculando tabla de frecuencias (Histograma de Tokens)...%s\n", COLOR_AMARILLO, COLOR_REINICIAR, COLOR_NEGRITA, COLOR_REINICIAR);
        uint32_t frecuencias[MAX_SIMBOLOS] = {0};
        contar_frecuencias(bufer_datos, tamano_original, frecuencias);

        int total_simbolos_distintos = 0;
        for (int i = 0; i < MAX_SIMBOLOS; i++) {
            if (frecuencias[i] > 0) {
                total_simbolos_distintos++;
            }
        }
        printf("  %s✓%s Símbolos distintos con frecuencia > 0: %s%d de 256%s\n\n", 
               COLOR_VERDE, COLOR_REINICIAR, COLOR_BLANCO, total_simbolos_distintos, COLOR_REINICIAR);

        // PASO 3: Construcción del Árbol de Huffman
        printf("%s[Paso 3/5]%s %sConstruyendo Árbol de Huffman (Algoritmo Voraz / Greedy)...%s\n", COLOR_AMARILLO, COLOR_REINICIAR, COLOR_NEGRITA, COLOR_REINICIAR);
        NodoHuffman *raiz_huffman = crear_arbol_huffman(frecuencias);
        if (!raiz_huffman && tamano_original > 0) {
            printf("  %s[✗] ERROR: Fallo al estructurar el árbol de Huffman.%s\n", COLOR_ROJO, COLOR_REINICIAR);
            free(bufer_datos);
            return EXIT_FAILURE;
        }
        printf("  %s✓%s Árbol binario óptimo enlazado correctamente en memoria dinámica.\n\n", COLOR_VERDE, COLOR_REINICIAR);

        // PASO 4: Generación de la tabla de codificación
        printf("%s[Paso 4/5]%s %sGenerando códigos binarios de longitud variable...%s\n", COLOR_AMARILLO, COLOR_REINICIAR, COLOR_NEGRITA, COLOR_REINICIAR);
        Diccionario diccionario_codigos;
        generar_codigos(raiz_huffman, &diccionario_codigos);

        // Tabla visual de códigos asignados
        printf("\n  %s┌──────┬──────────────────┬────────────┬────────────────────────┐%s\n", COLOR_CIAN, COLOR_REINICIAR);
        printf("  %s│ Byte │ Símbolo / Token  │ Frecuencia │ Código Binario Huffman │%s\n", COLOR_CIAN, COLOR_REINICIAR);
        printf("  %s├──────┼──────────────────┼────────────┼────────────────────────┤%s\n", COLOR_CIAN, COLOR_REINICIAR);

        for (int i = 0; i < MAX_SIMBOLOS; i++) {
            if (diccionario_codigos.presente[i]) {
                printf("  │ 0x%02X │ ", (unsigned char)i);
                imprimir_simbolo_legible((unsigned char)i);
                printf(" │ %10u │ %s%-22s%s │\n", 
                       frecuencias[i], 
                       COLOR_VERDE, diccionario_codigos.tabla[i].codigo, COLOR_REINICIAR);
            }
        }
        printf("  %s└──────┴──────────────────┴────────────┴────────────────────────┘%s\n\n", COLOR_CIAN, COLOR_REINICIAR);

        // PASO 5: Guardar el archivo comprimido en disco
        printf("%s[Paso 5/5]%s %sSerializando metadatos y empaquetando flujo de bits...%s\n", COLOR_AMARILLO, COLOR_REINICIAR, COLOR_NEGRITA, COLOR_REINICIAR);
        if (guardar_comprimido(ruta_destino, bufer_datos, tamano_original, frecuencias, &diccionario_codigos) != 0) {
            printf("  %s[✗] ERROR: No fue posible escribir el archivo comprimido.%s\n", COLOR_ROJO, COLOR_REINICIAR);
            liberar_arbol(raiz_huffman);
            free(bufer_datos);
            return EXIT_FAILURE;
        }

        // Obtener tamaño final resultante mediante la llamada al sistema stat()
        struct stat st_comprimido;
        long tamano_comprimido = 0;
        if (stat(ruta_destino, &st_comprimido) == 0) {
            tamano_comprimido = (long)st_comprimido.st_size;
        }

        double tasa_compresion = 0.0;
        double ahorro_espacio = 0.0;
        if (tamano_original > 0) {
            tasa_compresion = ((double)tamano_comprimido / (double)tamano_original) * 100.0;
            ahorro_espacio = 100.0 - tasa_compresion;
        }

        // Guardar registro de la salida en el archivo de bitácora
        registrar_archivo_salida(RUTA_ARCHIVO_REGISTRO, "COMPRESIÓN", ruta_origen, ruta_destino, 
                                 tamano_original, tamano_comprimido, ahorro_espacio);

        // Panel de Resumen de Resultados
        printf("\n%s╭──────────────────────── RESUMEN DE COMPRESIÓN ────────────────────────╮%s\n", COLOR_VERDE, COLOR_REINICIAR);
        printf("│  %sTamaño original:%s        %-12zu bytes                           │\n", COLOR_NEGRITA, COLOR_REINICIAR, tamano_original);
        printf("│  %sTamaño comprimido:%s      %-12ld bytes                           │\n", COLOR_NEGRITA, COLOR_REINICIAR, tamano_comprimido);
        printf("│  %sTasa de compresión:%s     %6.2f%%                                    │\n", COLOR_NEGRITA, COLOR_REINICIAR, tasa_compresion);
        printf("│  %sAhorro de espacio:%s      %6.2f%%  ", COLOR_NEGRITA, COLOR_REINICIAR, ahorro_espacio);
        imprimir_barra_progreso(ahorro_espacio);
        printf("         │\n");
        printf("│  %sSyscalls de E/S:%s        open, read, write, lseek, close, stat    │\n", COLOR_NEGRITA, COLOR_REINICIAR);
        printf("│  %sBitácora/Registro:%s      %-49s │\n", COLOR_NEGRITA, COLOR_REINICIAR, RUTA_ARCHIVO_REGISTRO);
        printf("│                                                                        │\n");
        printf("│  %sEstado:%s                 %s[✓] COMPRESIÓN EXITOSA%s                     │\n", 
               COLOR_NEGRITA, COLOR_REINICIAR, COLOR_VERDE, COLOR_REINICIAR);
        printf("%s╰────────────────────────────────────────────────────────────────────────╯%s\n\n", COLOR_VERDE, COLOR_REINICIAR);

        // Liberar recursos
        liberar_arbol(raiz_huffman);
        free(bufer_datos);

    // ========================================================================
    // MODO DESCOMPRESIÓN (-d)
    // ========================================================================
    } else if (strcmp(modo_operacion, "-d") == 0) {
        imprimir_banner_superior();
        printf("%s%s┌── [MODO DESCOMPRESIÓN] ───────────────────────────────────────────────┐%s\n", COLOR_MAGENTA, COLOR_NEGRITA, COLOR_REINICIAR);
        printf("%s│%s  Archivo comprimido: %-51s%s│%s\n", COLOR_MAGENTA, COLOR_BLANCO, ruta_origen, COLOR_MAGENTA, COLOR_REINICIAR);
        printf("%s│%s  Archivo a restaurar: %-50s%s│%s\n", COLOR_MAGENTA, COLOR_BLANCO, ruta_destino, COLOR_MAGENTA, COLOR_REINICIAR);
        printf("%s└────────────────────────────────────────────────────────────────────────┘%s\n\n", COLOR_MAGENTA, COLOR_REINICIAR);

        printf("%s[Paso 1/2]%s %sValidando encabezado y reconstruyendo árbol de Huffman...%s\n", COLOR_AMARILLO, COLOR_REINICIAR, COLOR_NEGRITA, COLOR_REINICIAR);
        printf("%s[Paso 2/2]%s %sDecodificando secuencia de bits hacia archivo restaurado...%s\n\n", COLOR_AMARILLO, COLOR_REINICIAR, COLOR_NEGRITA, COLOR_REINICIAR);

        // Medir tamaño del archivo de entrada comprimido con llamada stat()
        struct stat st_entrada;
        long tamano_comprimido_origen = 0;
        if (stat(ruta_origen, &st_entrada) == 0) {
            tamano_comprimido_origen = (long)st_entrada.st_size;
        }

        if (descomprimir_archivo(ruta_origen, ruta_destino) != 0) {
            printf("\n%s╭────────────────────────────────────────────────────────────────────────╮%s\n", COLOR_ROJO, COLOR_REINICIAR);
            printf("│  %s[✗] ERROR CRÍTICO:%s Falló el proceso de descompresión.              │\n", COLOR_ROJO, COLOR_REINICIAR);
            printf("%s╰────────────────────────────────────────────────────────────────────────╯%s\n\n", COLOR_ROJO, COLOR_REINICIAR);
            return EXIT_FAILURE;
        }

        // Medir tamaño del archivo restaurado mediante stat()
        struct stat st_salida;
        long bytes_recuperados = 0;
        if (stat(ruta_destino, &st_salida) == 0) {
            bytes_recuperados = (long)st_salida.st_size;
        }

        // Guardar registro de la salida en el archivo de bitácora
        registrar_archivo_salida(RUTA_ARCHIVO_REGISTRO, "DESCOMPRESIÓN", ruta_origen, ruta_destino, 
                                 (size_t)tamano_comprimido_origen, bytes_recuperados, 0.0);

        printf("%s╭─────────────────────── RESUMEN DE DESCOMPRESIÓN ───────────────────────╮%s\n", COLOR_VERDE, COLOR_REINICIAR);
        printf("│  %sArchivo recuperado:%s     %-49s │\n", COLOR_NEGRITA, COLOR_REINICIAR, ruta_destino);
        printf("│  %sBytes restaurados:%s      %-12ld bytes                           │\n", COLOR_NEGRITA, COLOR_REINICIAR, bytes_recuperados);
        printf("│  %sSyscalls de E/S:%s        open, read, write, close, stat           │\n", COLOR_NEGRITA, COLOR_REINICIAR);
        printf("│  %sBitácora/Registro:%s      %-49s │\n", COLOR_NEGRITA, COLOR_REINICIAR, RUTA_ARCHIVO_REGISTRO);
        printf("│                                                                        │\n");
        printf("│  %sEstado:%s                 %s[✓] DESCOMPRESIÓN EXITOSA%s                   │\n", 
               COLOR_NEGRITA, COLOR_REINICIAR, COLOR_VERDE, COLOR_REINICIAR);
        printf("%s╰────────────────────────────────────────────────────────────────────────╯%s\n\n", COLOR_VERDE, COLOR_REINICIAR);

    } else {
        printf("%s[✗] Opción no reconocida:%s '%s'\n\n", COLOR_ROJO, COLOR_REINICIAR, modo_operacion);
        imprimir_uso_programa(argumentos[0]);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
