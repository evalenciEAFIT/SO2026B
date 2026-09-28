/**
 * ============================================================================
 * Proyecto: Compresor y Descompresor de Archivos con Algoritmo de Huffman
 * Asignatura: Sistemas Operativos (4to Semestre)
 * 
 * GUÍA TEÓRICA SOBRE LLAMADAS AL SISTEMA (SYSTEM CALLS):
 * ----------------------------------------------------------------------------
 * 1. ¿Qué es una llamada al sistema (syscall)?
 *    Es la interfaz formal y segura mediante la cual un programa que se ejecuta
 *    en espacio de usuario (User Space / Modo No Privilegiado) solicita servicios
 *    directamente al núcleo del sistema operativo (Kernel Space / Modo Privilegiado).
 * 
 * 2. Transición de Modo (Context Switch de Modo):
 *    Cuando invocamos open(), read(), write() o close():
 *    a) Los argumentos se colocan en registros específicos de la CPU.
 *    b) Se ejecuta una instrucción de trampa/interrupción por software (syscall / int 0x80).
 *    c) La CPU cambia al nivel de privilegio 0 (Modo Núcleo).
 *    d) El despachador de syscalls del Kernel ejecuta la rutina correspondiente
 *       interactuando con los controladores de disco (drivers) y el sistema de archivos.
 *    e) El resultado se devuelve y la CPU retorna al Modo Usuario.
 * 
 * 3. ¿Qué es un Descriptor de Archivo (File Descriptor - fd)?
 *    Es un número entero no negativo (int fd) que sirve como índice dentro de
 *    la Tabla de Descriptores de Archivos del Proceso, administrada por el SO.
 *    - 0: Entrada estándar (STDIN_FILENO)
 *    - 1: Salida estándar (STDOUT_FILENO)
 *    - 2: Error estándar (STDERR_FILENO)
 *    - 3+: Asignados a archivos abiertos por el usuario.
 * 
 * 4. Búferes de Entrada/Salida a nivel de Bloque:
 *    Cada llamada al sistema tiene un costo de tiempo por cambio de contexto.
 *    Para optimizar el rendimiento, agrupamos las lecturas y escrituras en
 *    búferes de 4096 bytes (4 KB), coincidiendo con el tamaño de página y bloque
 *    estándar del sistema de archivos en los sistemas operativos modernos.
 * ============================================================================
 */

#include "huffman.h"
#include <string.h>
#include <time.h>

// Tamaño de bloque de 4096 bytes (4 KB), coincidente con la página de memoria del SO
#define TAMANO_BLOQUE_IO 4096

/*
 * ============================================================================
 * MÓDULO AUXILIAR: Escritor de Bits con Búfer de Bloque y Syscall write()
 * ============================================================================
 */
typedef struct {
    int descriptor_archivo;                 // File Descriptor (fd) asignado por open()
    unsigned char bufer_bits;               // Byte donde se acumulan los bits (0 a 8)
    int bits_acumulados;                    // Cantidad de bits acumulados en bufer_bits
    unsigned char bufer_bloque[TAMANO_BLOQUE_IO]; // Búfer de 4 KB para emitir escrituras en bloque
    size_t bytes_en_bloque;                 // Cantidad de bytes en bufer_bloque esperando write()
} EscritorBits;

/**
 * Inicializa el escritor de bits vinculándolo al descriptor de archivo (fd).
 */
static void inicializar_escritor_bits(EscritorBits *escritor, int descriptor_archivo) {
    escritor->descriptor_archivo = descriptor_archivo;
    escritor->bufer_bits = 0;
    escritor->bits_acumulados = 0;
    escritor->bytes_en_bloque = 0;
}

/**
 * Envía el bloque acumulado de bytes a disco invocando la llamada al sistema write().
 */
static void vaciar_bloque_escritura(EscritorBits *escritor) {
    if (escritor->bytes_en_bloque > 0) {
        size_t escritos_totales = 0;
        while (escritos_totales < escritor->bytes_en_bloque) {
            // Syscall write(fd, buffer, cantidad)
            ssize_t res = write(escritor->descriptor_archivo, 
                                escritor->bufer_bloque + escritos_totales, 
                                escritor->bytes_en_bloque - escritos_totales);
            if (res < 0) {
                perror("Error en llamada al sistema write() en EscritorBits");
                return;
            }
            escritos_totales += (size_t)res;
        }
        escritor->bytes_en_bloque = 0;
    }
}

/**
 * Agrega un bit (0 o 1) al búfer de bits. Al completar 8 bits, transfiere el byte
 * al búfer de bloque. Si el bloque de 4 KB se llena, invoca write() al sistema operativo.
 */
static void escribir_bit(EscritorBits *escritor, int valor_bit) {
    // Desplazar a la izquierda e insertar el bit
    escritor->bufer_bits = (unsigned char)((escritor->bufer_bits << 1) | (valor_bit & 1));
    escritor->bits_acumulados++;

    // Al completar 8 bits (1 byte)
    if (escritor->bits_acumulados == 8) {
        escritor->bufer_bloque[escritor->bytes_en_bloque++] = escritor->bufer_bits;
        escritor->bufer_bits = 0;
        escritor->bits_acumulados = 0;

        // Si se completó una página de 4096 bytes, emitimos la llamada al sistema write()
        if (escritor->bytes_en_bloque == TAMANO_BLOQUE_IO) {
            vaciar_bloque_escritura(escritor);
        }
    }
}

/**
 * Rellena con ceros a la derecha los bits pendientes y vacía el búfer hacia el disco.
 */
static void vaciar_escritor_bits(EscritorBits *escritor) {
    if (escritor->bits_acumulados > 0) {
        escritor->bufer_bits <<= (8 - escritor->bits_acumulados);
        escritor->bufer_bloque[escritor->bytes_en_bloque++] = escritor->bufer_bits;
        escritor->bufer_bits = 0;
        escritor->bits_acumulados = 0;
    }
    vaciar_bloque_escritura(escritor);
}

/*
 * ============================================================================
 * MÓDULO AUXILIAR: Lector de Bits con Búfer de Bloque y Syscall read()
 * ============================================================================
 */
typedef struct {
    int descriptor_archivo;                 // File Descriptor (fd) para lectura
    unsigned char bufer_bloque[TAMANO_BLOQUE_IO]; // Bloque de 4 KB leído con read()
    size_t bytes_disponibles;               // Bytes devueltos en la última llamada a read()
    size_t indice_lectura;                  // Posición del byte actual dentro de bufer_bloque
    unsigned char bufer_bits;               // Byte actual del que se están extrayendo bits
    int bits_restantes;                     // Bits aún no leídos en bufer_bits (0 a 8)
} LectorBits;

static void inicializar_lector_bits(LectorBits *lector, int descriptor_archivo) {
    lector->descriptor_archivo = descriptor_archivo;
    lector->bytes_disponibles = 0;
    lector->indice_lectura = 0;
    lector->bufer_bits = 0;
    lector->bits_restantes = 0;
}

/**
 * Lee el siguiente bit de la secuencia. Cuando se agota el búfer interno,
 * invoca la llamada al sistema read() para cargar una página de 4 KB desde el disco.
 */
static int leer_bit(LectorBits *lector) {
    if (lector->bits_restantes == 0) {
        // Si ya procesamos todos los bytes del bloque de 4 KB, leemos un nuevo bloque
        if (lector->indice_lectura >= lector->bytes_disponibles) {
            // Syscall read(fd, buffer, cantidad)
            ssize_t leidos = read(lector->descriptor_archivo, lector->bufer_bloque, TAMANO_BLOQUE_IO);
            if (leidos <= 0) {
                return -1; // Fin de archivo o error de lectura
            }
            lector->bytes_disponibles = (size_t)leidos;
            lector->indice_lectura = 0;
        }

        // Extraer el siguiente byte del bloque
        lector->bufer_bits = lector->bufer_bloque[lector->indice_lectura++];
        lector->bits_restantes = 8;
    }

    // Extraer el bit más significativo (MSB a LSB)
    lector->bits_restantes--;
    int valor_bit = (lector->bufer_bits >> lector->bits_restantes) & 1;
    return valor_bit;
}

/*
 * ============================================================================
 * 1. FUNCIÓN: leer_archivo (Implementada con Llamadas al Sistema)
 * ============================================================================
 * Llamadas al Sistema Utilizadas:
 *   - open(): Abre el archivo solicitando al SO un nuevo descriptor en la tabla.
 *   - lseek(): Modifica y consulta el puntero de posición (offset) en el Kernel.
 *   - read(): Solicita al Kernel la transferencia de datos físicos a la RAM.
 *   - close(): Libera el descriptor de archivo del proceso.
 */
unsigned char* leer_archivo(const char *ruta_archivo, size_t *tamano_salida) {
    if (!ruta_archivo || !tamano_salida) {
        return NULL;
    }

    // 1. LLAMADA AL SISTEMA: open()
    // Bandera O_RDONLY: Abre el archivo exclusivamente en modo de solo lectura.
    int descriptor_archivo = open(ruta_archivo, O_RDONLY);
    if (descriptor_archivo == -1) {
        perror("Error en llamada al sistema open() al intentar leer");
        return NULL;
    }

    // 2. LLAMADA AL SISTEMA: lseek()
    // lseek(fd, 0, SEEK_END): Sitúa el offset en el último byte y retorna la posición,
    // permitiendo averiguar el tamaño exacto del archivo en bytes.
    off_t longitud_archivo = lseek(descriptor_archivo, 0, SEEK_END);
    if (longitud_archivo < 0) {
        perror("Error en llamada al sistema lseek()");
        close(descriptor_archivo);
        return NULL;
    }

    // Reposicionar el offset al inicio del archivo (SEEK_SET, 0)
    if (lseek(descriptor_archivo, 0, SEEK_SET) < 0) {
        perror("Error al reposicionar lseek()");
        close(descriptor_archivo);
        return NULL;
    }

    *tamano_salida = (size_t)longitud_archivo;

    // Caso especial: archivo vacío
    if (longitud_archivo == 0) {
        close(descriptor_archivo);
        unsigned char *bufer_vacio = (unsigned char*)malloc(1);
        return bufer_vacio;
    }

    // Reservar memoria en el Heap para almacenar el contenido completo
    unsigned char *bufer_datos = (unsigned char*)malloc((size_t)longitud_archivo);
    if (!bufer_datos) {
        fprintf(stderr, "Error: Memoria insuficiente en el Heap para cargar el archivo.\n");
        close(descriptor_archivo);
        return NULL;
    }

    // 3. LLAMADA AL SISTEMA: read()
    // En sistemas operativos POSIX, read() puede leer menos bytes de los solicitados
    // si ocurre una interrupción por señal o límites del sistema de archivos.
    // Por ello, es una buena práctica utilizar un bucle hasta completar el tamaño total.
    size_t bytes_totales_leidos = 0;
    while (bytes_totales_leidos < (size_t)longitud_archivo) {
        ssize_t bytes_leidos_iteracion = read(descriptor_archivo, 
                                             bufer_datos + bytes_totales_leidos, 
                                             (size_t)longitud_archivo - bytes_totales_leidos);
        if (bytes_leidos_iteracion < 0) {
            perror("Error en llamada al sistema read()");
            free(bufer_datos);
            close(descriptor_archivo);
            return NULL;
        }
        if (bytes_leidos_iteracion == 0) {
            break; // Fin inesperado del archivo
        }
        bytes_totales_leidos += (size_t)bytes_leidos_iteracion;
    }

    // 4. LLAMADA AL SISTEMA: close()
    // Cierra el descriptor para evitar agotamiento de descriptores de archivos en el SO.
    close(descriptor_archivo);
    return bufer_datos;
}

/*
 * ============================================================================
 * 2. FUNCIÓN: contar_frecuencias
 * ============================================================================
 */
void contar_frecuencias(const unsigned char *datos, 
                        size_t tamano_datos, 
                        uint32_t frecuencias[MAX_SIMBOLOS]) {
    for (int i = 0; i < MAX_SIMBOLOS; i++) {
        frecuencias[i] = 0;
    }

    for (size_t i = 0; i < tamano_datos; i++) {
        unsigned char token = datos[i];
        frecuencias[token]++;
    }
}

/*
 * ============================================================================
 * 3. FUNCIÓN: crear_arbol_huffman
 * ============================================================================
 */
static NodoHuffman* crear_nodo(unsigned char simbolo, 
                               uint32_t frecuencia, 
                               NodoHuffman *hijo_izquierdo, 
                               NodoHuffman *hijo_derecho) {
    NodoHuffman *nuevo_nodo = (NodoHuffman*)malloc(sizeof(NodoHuffman));
    if (!nuevo_nodo) {
        fprintf(stderr, "Error: No se pudo asignar memoria para un NodoHuffman.\n");
        exit(EXIT_FAILURE);
    }
    nuevo_nodo->simbolo = simbolo;
    nuevo_nodo->frecuencia = frecuencia;
    nuevo_nodo->izq = hijo_izquierdo;
    nuevo_nodo->der = hijo_derecho;
    return nuevo_nodo;
}

NodoHuffman* crear_arbol_huffman(const uint32_t frecuencias[MAX_SIMBOLOS]) {
    NodoHuffman *lista_nodos[MAX_SIMBOLOS];
    int cantidad_nodos = 0;

    for (int i = 0; i < MAX_SIMBOLOS; i++) {
        if (frecuencias[i] > 0) {
            lista_nodos[cantidad_nodos++] = crear_nodo((unsigned char)i, frecuencias[i], NULL, NULL);
        }
    }

    if (cantidad_nodos == 0) {
        return NULL;
    }

    if (cantidad_nodos == 1) {
        return crear_nodo(0, lista_nodos[0]->frecuencia, lista_nodos[0], NULL);
    }

    while (cantidad_nodos > 1) {
        int indice_min1 = 0;
        for (int i = 1; i < cantidad_nodos; i++) {
            if (lista_nodos[i]->frecuencia < lista_nodos[indice_min1]->frecuencia) {
                indice_min1 = i;
            }
        }
        NodoHuffman *primer_minimo = lista_nodos[indice_min1];

        lista_nodos[indice_min1] = lista_nodos[cantidad_nodos - 1];
        cantidad_nodos--;

        int indice_min2 = 0;
        for (int i = 1; i < cantidad_nodos; i++) {
            if (lista_nodos[i]->frecuencia < lista_nodos[indice_min2]->frecuencia) {
                indice_min2 = i;
            }
        }
        NodoHuffman *segundo_minimo = lista_nodos[indice_min2];

        NodoHuffman *nodo_padre = crear_nodo(0, 
                                             primer_minimo->frecuencia + segundo_minimo->frecuencia, 
                                             primer_minimo, 
                                             segundo_minimo);

        lista_nodos[indice_min2] = nodo_padre;
    }

    return lista_nodos[0];
}

/*
 * ============================================================================
 * 4. FUNCIÓN: generar_codigos
 * ============================================================================
 */
static void recorrer_arbol_recursivo(NodoHuffman *nodo_actual, 
                                     char *prefijo_temporal, 
                                     int profundidad, 
                                     Diccionario *diccionario) {
    if (!nodo_actual) {
        return;
    }

    if (!nodo_actual->izq && !nodo_actual->der) {
        prefijo_temporal[profundidad] = '\0';

        if (profundidad == 0) {
            strcpy(diccionario->tabla[nodo_actual->simbolo].codigo, "0");
            diccionario->tabla[nodo_actual->simbolo].longitud = 1;
        } else {
            strcpy(diccionario->tabla[nodo_actual->simbolo].codigo, prefijo_temporal);
            diccionario->tabla[nodo_actual->simbolo].longitud = profundidad;
        }

        diccionario->presente[nodo_actual->simbolo] = true;
        return;
    }

    if (nodo_actual->izq) {
        prefijo_temporal[profundidad] = '0';
        recorrer_arbol_recursivo(nodo_actual->izq, prefijo_temporal, profundidad + 1, diccionario);
    }

    if (nodo_actual->der) {
        prefijo_temporal[profundidad] = '1';
        recorrer_arbol_recursivo(nodo_actual->der, prefijo_temporal, profundidad + 1, diccionario);
    }
}

void generar_codigos(NodoHuffman *raiz, Diccionario *diccionario) {
    for (int i = 0; i < MAX_SIMBOLOS; i++) {
        diccionario->tabla[i].codigo[0] = '\0';
        diccionario->tabla[i].longitud = 0;
        diccionario->presente[i] = false;
    }

    if (!raiz) {
        return;
    }

    char prefijo_temporal[MAX_LONGITUD_CODIGO];
    recorrer_arbol_recursivo(raiz, prefijo_temporal, 0, diccionario);
}

/*
 * ============================================================================
 * 5. FUNCIÓN: guardar_comprimido (Implementada con Llamadas al Sistema)
 * ============================================================================
 * Llamadas al Sistema Utilizadas:
 *   - open(): con banderas O_WRONLY | O_CREAT | O_TRUNC y permisos 0644.
 *   - write(): escribe directamente los encabezados binarios y el flujo de bits.
 *   - close(): confirma el vaciado de buffers del Kernel y cierra el descriptor.
 */
int guardar_comprimido(const char *ruta_salida, 
                       const unsigned char *datos_originales, 
                       size_t tamano_original, 
                       const uint32_t frecuencias[MAX_SIMBOLOS], 
                       const Diccionario *diccionario) {
    // Garantizar que la carpeta de destino exista (llamada a mkdir)
    asegurar_directorio_padre(ruta_salida);

    // 1. LLAMADA AL SISTEMA: open()
    // Banderas explicadas para estudiantes de SO:
    //   - O_WRONLY: Abrir únicamente para escritura (Write Only).
    //   - O_CREAT: Si el archivo no existe en el inodo, el Kernel lo crea.
    //   - O_TRUNC: Si el archivo ya existía, limpia su contenido truncándolo a 0 bytes.
    // Modo de Permisos (0644 en notación octal POSIX):
    //   - 6 (rw-): El usuario propietario puede leer y escribir.
    //   - 4 (r--): Los miembros del grupo solo pueden leer.
    //   - 4 (r--): Los demás usuarios solo pueden leer.
    int fd_salida = open(ruta_salida, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_salida == -1) {
        perror("Error en llamada al sistema open() al crear archivo comprimido");
        return -1;
    }

    // 2. LLAMADA AL SISTEMA: write() para metadatos de la cabecera
    // a) Escribir el número mágico (4 bytes)
    uint32_t numero_magico = NUMERO_MAGICO_HUFFMAN;
    if (write(fd_salida, &numero_magico, sizeof(uint32_t)) != sizeof(uint32_t)) {
        perror("Error en llamada al sistema write() escribiendo numero magico");
        close(fd_salida);
        return -1;
    }

    // b) Escribir el tamaño original en bytes (uint64_t = 8 bytes)
    uint64_t tamano_en_bytes = (uint64_t)tamano_original;
    if (write(fd_salida, &tamano_en_bytes, sizeof(uint64_t)) != sizeof(uint64_t)) {
        perror("Error en llamada al sistema write() escribiendo tamano original");
        close(fd_salida);
        return -1;
    }

    // c) Contar y escribir la cantidad de símbolos distintos
    uint16_t simbolos_distintos = 0;
    for (int i = 0; i < MAX_SIMBOLOS; i++) {
        if (frecuencias[i] > 0) {
            simbolos_distintos++;
        }
    }
    if (write(fd_salida, &simbolos_distintos, sizeof(uint16_t)) != sizeof(uint16_t)) {
        perror("Error en llamada al sistema write() escribiendo cantidad de simbolos");
        close(fd_salida);
        return -1;
    }

    // d) Escribir los pares de frecuencias del diccionario
    for (int i = 0; i < MAX_SIMBOLOS; i++) {
        if (frecuencias[i] > 0) {
            unsigned char simbolo = (unsigned char)i;
            write(fd_salida, &simbolo, sizeof(unsigned char));
            write(fd_salida, &frecuencias[i], sizeof(uint32_t));
        }
    }

    if (tamano_original == 0) {
        close(fd_salida);
        return 0;
    }

    // 3. Escribir bits empaquetados usando EscritorBits (que invoca write() en bloques)
    EscritorBits escritor;
    inicializar_escritor_bits(&escritor, fd_salida);

    for (size_t i = 0; i < tamano_original; i++) {
        unsigned char caracter = datos_originales[i];
        const char *cadena_codigo = diccionario->tabla[caracter].codigo;
        for (int j = 0; cadena_codigo[j] != '\0'; j++) {
            escribir_bit(&escritor, cadena_codigo[j] - '0');
        }
    }

    // Vaciar los bits residuales hacia el descriptor mediante write()
    vaciar_escritor_bits(&escritor);

    // 4. LLAMADA AL SISTEMA: close()
    close(fd_salida);
    return 0;
}

/*
 * ============================================================================
 * 6. FUNCIÓN: descomprimir_archivo (Implementada con Llamadas al Sistema)
 * ============================================================================
 * Llamadas al Sistema Utilizadas:
 *   - open(): para abrir el archivo comprimido (O_RDONLY) y crear el destino (O_WRONLY | O_CREAT | O_TRUNC).
 *   - read(): lee la cabecera binaria y el flujo comprimido.
 *   - write(): escribe bloques de bytes decodificados directamente en el archivo destino.
 *   - close(): cierra ambos descriptores.
 */
int descomprimir_archivo(const char *ruta_comprimido, const char *ruta_descomprimido) {
    // 1. LLAMADA AL SISTEMA: open() para lectura del comprimido
    int fd_entrada = open(ruta_comprimido, O_RDONLY);
    if (fd_entrada == -1) {
        perror("Error en llamada al sistema open() al abrir archivo comprimido");
        return -1;
    }

    // 2. LLAMADA AL SISTEMA: read() para validar el número mágico
    uint32_t numero_magico_leido = 0;
    if (read(fd_entrada, &numero_magico_leido, sizeof(uint32_t)) != sizeof(uint32_t) || 
        numero_magico_leido != NUMERO_MAGICO_HUFFMAN) {
        fprintf(stderr, "Error: El archivo no es un archivo Huffman valido (Magic Number incorrecto).\n");
        close(fd_entrada);
        return -1;
    }

    // 3. LLAMADA AL SISTEMA: read() para leer el tamaño original esperado
    uint64_t tamano_original = 0;
    if (read(fd_entrada, &tamano_original, sizeof(uint64_t)) != sizeof(uint64_t)) {
        fprintf(stderr, "Error al leer tamano original con llamada read().\n");
        close(fd_entrada);
        return -1;
    }

    // 4. LLAMADA AL SISTEMA: read() para leer cantidad de símbolos
    uint16_t simbolos_distintos = 0;
    if (read(fd_entrada, &simbolos_distintos, sizeof(uint16_t)) != sizeof(uint16_t)) {
        fprintf(stderr, "Error al leer cantidad de simbolos con llamada read().\n");
        close(fd_entrada);
        return -1;
    }

    // 5. LLAMADA AL SISTEMA: read() para reconstruir la tabla de frecuencias
    uint32_t frecuencias[MAX_SIMBOLOS] = {0};
    for (int i = 0; i < simbolos_distintos; i++) {
        unsigned char simbolo = 0;
        uint32_t frecuencia = 0;
        if (read(fd_entrada, &simbolo, sizeof(unsigned char)) != sizeof(unsigned char) ||
            read(fd_entrada, &frecuencia, sizeof(uint32_t)) != sizeof(uint32_t)) {
            fprintf(stderr, "Error al leer diccionario de frecuencias con llamada read().\n");
            close(fd_entrada);
            return -1;
        }
        frecuencias[simbolo] = frecuencia;
    }

    // Asegurar directorio destino
    asegurar_directorio_padre(ruta_descomprimido);

    // 6. LLAMADA AL SISTEMA: open() para crear el archivo restaurado
    int fd_salida = open(ruta_descomprimido, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_salida == -1) {
        perror("Error en llamada al sistema open() al crear archivo restaurado");
        close(fd_entrada);
        return -1;
    }

    // Si el archivo original estaba vacío
    if (tamano_original == 0) {
        close(fd_salida);
        close(fd_entrada);
        return 0;
    }

    // 7. Reconstruir el árbol de Huffman con las frecuencias obtenidas
    NodoHuffman *raiz = crear_arbol_huffman(frecuencias);
    if (!raiz) {
        fprintf(stderr, "Error al reconstruir el arbol de Huffman.\n");
        close(fd_salida);
        close(fd_entrada);
        return -1;
    }

    // 8. Decodificación de bits y escritura mediante búfer de bloque y llamada write()
    LectorBits lector;
    inicializar_lector_bits(&lector, fd_entrada);

    uint64_t bytes_recuperados = 0;
    NodoHuffman *nodo_actual = raiz;

    // Búfer intermedio para agrupar salidas y emitir llamadas al sistema write() óptimas
    unsigned char bufer_escritura[TAMANO_BLOQUE_IO];
    size_t bytes_en_bufer_escritura = 0;

    // Caso de un solo símbolo único repetido
    if (!raiz->izq && !raiz->der) {
        for (uint64_t i = 0; i < tamano_original; i++) {
            bufer_escritura[bytes_en_bufer_escritura++] = raiz->simbolo;
            if (bytes_en_bufer_escritura == TAMANO_BLOQUE_IO) {
                write(fd_salida, bufer_escritura, bytes_en_bufer_escritura);
                bytes_en_bufer_escritura = 0;
            }
        }
        bytes_recuperados = tamano_original;
    } else {
        while (bytes_recuperados < tamano_original) {
            int valor_bit = leer_bit(&lector);
            if (valor_bit == -1) {
                fprintf(stderr, "Advertencia: Fin prematuro de la secuencia de bits.\n");
                break;
            }

            nodo_actual = (valor_bit == 0) ? nodo_actual->izq : nodo_actual->der;

            // Al llegar a una hoja, encontramos el byte original
            if (nodo_actual && !nodo_actual->izq && !nodo_actual->der) {
                bufer_escritura[bytes_en_bufer_escritura++] = nodo_actual->simbolo;
                bytes_recuperados++;
                nodo_actual = raiz; // Reiniciar recorrido

                // Cuando el búfer alcanza 4 KB, invocar la syscall write()
                if (bytes_en_bufer_escritura == TAMANO_BLOQUE_IO) {
                    write(fd_salida, bufer_escritura, bytes_en_bufer_escritura);
                    bytes_en_bufer_escritura = 0;
                }
            }
        }
    }

    // Vaciar bytes pendientes restantes en el búfer de salida
    if (bytes_en_bufer_escritura > 0) {
        write(fd_salida, bufer_escritura, bytes_en_bufer_escritura);
    }

    // 9. LLAMADAS AL SISTEMA: close()
    close(fd_salida);
    close(fd_entrada);
    liberar_arbol(raiz);

    if (bytes_recuperados != tamano_original) {
        fprintf(stderr, "Error: Se recuperaron %lu bytes de los %lu esperados.\n",
                (unsigned long)bytes_recuperados, (unsigned long)tamano_original);
        return -1;
    }

    return 0;
}

/*
 * ============================================================================
 * 7. FUNCIÓN: liberar_arbol
 * ============================================================================
 */
void liberar_arbol(NodoHuffman *raiz) {
    if (!raiz) {
        return;
    }
    liberar_arbol(raiz->izq);
    liberar_arbol(raiz->der);
    free(raiz);
}

/*
 * ============================================================================
 * 8. FUNCIÓN: asegurar_directorio_padre (Implementada con Llamadas al Sistema)
 * ============================================================================
 * Llamadas al Sistema Utilizadas:
 *   - stat(): Consulta los inodos y metadatos del directorio sin abrirlo.
 *   - mkdir(): Crea una entrada de directorio en el inodo correspondiente con
 *              permisos 0755 (rwxr-xr-x).
 */
void asegurar_directorio_padre(const char *ruta_archivo) {
    if (!ruta_archivo) return;

    const char *ultimo_separador = strrchr(ruta_archivo, '/');
    if (!ultimo_separador) {
        return; // Ubicado en el directorio actual (.)
    }

    size_t longitud_dir = (size_t)(ultimo_separador - ruta_archivo);
    if (longitud_dir == 0) {
        return; // Ubicado en la raíz (/)
    }

    char ruta_directorio[512];
    if (longitud_dir >= sizeof(ruta_directorio)) {
        longitud_dir = sizeof(ruta_directorio) - 1;
    }
    strncpy(ruta_directorio, ruta_archivo, longitud_dir);
    ruta_directorio[longitud_dir] = '\0';

    // LLAMADA AL SISTEMA: stat() para interrogar inodos
    struct stat estado;
    if (stat(ruta_directorio, &estado) == -1) {
        // LLAMADA AL SISTEMA: mkdir(ruta, permisos)
        // Permisos 0755: rwxr-xr-x (lectura/escritura/ejecución dueño, lectura/ejecución otros)
        mkdir(ruta_directorio, 0755);
    }
}

/*
 * ============================================================================
 * 9. FUNCIÓN: registrar_archivo_salida (Implementada con Llamadas al Sistema)
 * ============================================================================
 * Llamadas al Sistema Utilizadas:
 *   - open(): con bandera O_APPEND para agregar al final sin sobreescribir.
 *   - write(): escribe directamente el bloque de registro.
 *   - close(): cierra el descriptor.
 */
void registrar_archivo_salida(const char *ruta_registro,
                              const char *operacion,
                              const char *ruta_origen,
                              const char *ruta_destino,
                              size_t tamano_origen,
                              long tamano_destino,
                              double porcentaje_ahorro) {
    if (!ruta_registro) return;

    asegurar_directorio_padre(ruta_registro);

    struct stat estado;
    bool es_archivo_nuevo = (stat(ruta_registro, &estado) == -1);

    // LLAMADA AL SISTEMA: open()
    // Bandera O_APPEND: Cada llamada a write() se posiciona atómicamente al final del archivo.
    int fd_bitacora = open(ruta_registro, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd_bitacora == -1) {
        perror("Advertencia: No se pudo abrir la bitacora con llamada open()");
        return;
    }

    char mensaje[1024];
    int longitud_mensaje = 0;

    if (es_archivo_nuevo) {
        const char *encabezado = 
            "================================================================================\n"
            "BITÁCORA Y REGISTRO DE ARCHIVOS DE SALIDA - ALGORITMO DE HUFFMAN\n"
            "Implementado con Llamadas al Sistema (open, read, write, close, lseek)\n"
            "Asignatura: Sistemas Operativos (4to Semestre)\n"
            "================================================================================\n\n";
        write(fd_bitacora, encabezado, strlen(encabezado));
    }

    // Consulta de reloj del sistema
    time_t tiempo_actual = time(NULL);
    struct tm *info_tiempo = localtime(&tiempo_actual);
    char texto_fecha[64];
    strftime(texto_fecha, sizeof(texto_fecha), "%Y-%m-%d %H:%M:%S", info_tiempo);

    if (strcmp(operacion, "COMPRESIÓN") == 0) {
        longitud_mensaje = snprintf(mensaje, sizeof(mensaje),
            "[Registro: %s]\n"
            "  • Operación:          %s\n"
            "  • Archivo de Entrada: %s (%zu bytes)\n"
            "  • Archivo de Salida:  %s (%ld bytes)\n"
            "  • Ahorro en Disco:    %.2f%%\n"
            "  • Syscalls de E/S:    open, read, write, close, lseek\n"
            "  • Estado:             EXITOSO\n"
            "--------------------------------------------------------------------------------\n",
            texto_fecha, operacion, ruta_origen, tamano_origen, ruta_destino, tamano_destino, porcentaje_ahorro);
    } else {
        longitud_mensaje = snprintf(mensaje, sizeof(mensaje),
            "[Registro: %s]\n"
            "  • Operación:          %s\n"
            "  • Archivo de Entrada: %s (%zu bytes)\n"
            "  • Archivo de Salida:  %s (%ld bytes)\n"
            "  • Syscalls de E/S:    open, read, write, close\n"
            "  • Estado:             EXITOSO\n"
            "--------------------------------------------------------------------------------\n",
            texto_fecha, operacion, ruta_origen, tamano_origen, ruta_destino, tamano_destino);
    }

    // LLAMADA AL SISTEMA: write()
    if (longitud_mensaje > 0) {
        write(fd_bitacora, mensaje, (size_t)longitud_mensaje);
    }

    // LLAMADA AL SISTEMA: close()
    close(fd_bitacora);
}
