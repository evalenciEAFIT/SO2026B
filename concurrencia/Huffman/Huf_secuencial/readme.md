# Compresor y Descompresor de Archivos con Algoritmo de Huffman

### Asignatura: Sistemas Operativos (4to Semestre)

Este proyecto implementa el algoritmo clásico de **Codificación de Huffman** en lenguaje C de forma secuencial, modular y didáctica, utilizando **Llamadas al Sistema POSIX directas (System Calls)** para todas las operaciones de entrada/salida y **nombres 100% en español**.

---

## 🏛️ Conceptos Clave de Sistemas Operativos: Llamadas al Sistema (Syscalls)

En lugar de emplear las funciones de biblioteca de alto nivel de `stdio.h` (`fopen`, `fread`, `fwrite`), este proyecto emplea **llamadas al sistema del Kernel de Linux**:

| Operación                 | Función de Biblioteca (`stdio.h`) | Llamada al Sistema POSIX (`syscall`)            | Descripción y Rol del Kernel                                                                    |
| :------------------------- | :----------------------------------- | :------------------------------------------------ | :----------------------------------------------------------------------------------------------- |
| **Apertura**         | `fopen(ruta, "rb")`                | `open(ruta, O_RDONLY)`                          | Solicita al Kernel una entrada en la**Tabla de Descriptores de Archivos** (`int fd`).    |
| **Creación**        | `fopen(ruta, "wb")`                | `open(..., O_WRONLY \| O_CREAT \| O_TRUNC, 0644)` | Crea el inodo con permisos octales`0644` (`rw-r--r--`).                                      |
| **Lectura**          | `fread(bufer, 1, n, f)`            | `read(fd, bufer, n)`                            | Transfiere bytes físicos desde el controlador de disco hacia la RAM del proceso.                |
| **Escritura**        | `fwrite(bufer, 1, n, f)`           | `write(fd, bufer, n)`                           | Transfiere bytes desde la RAM del proceso hacia los búferes de página del Kernel.              |
| **Posicionamiento**  | `fseek(f, 0, SEEK_END)`            | `lseek(fd, 0, SEEK_END)`                        | Modifica el puntero de*offset* administrado por el Kernel en la estructura de archivo abierto. |
| **Cierre**           | `fclose(f)`                        | `close(fd)`                                     | Libera el descriptor de archivo para evitar fugas de recursos en el sistema.                     |
| **Metadatos**        | `fseek` + `ftell`                | `stat(ruta, &st)`                               | Consulta los inodos directamente sin necesidad de abrir el archivo.                              |
| **Crear Directorio** | *N/A en stdio*                     | `mkdir(ruta, 0755)`                             | Crea una entrada de directorio en el sistema de archivos con permisos`rwxr-xr-x`.              |

### ⚡ Búfer de Bloque de 4 KB (Página de Memoria):

Cada llamada al sistema implica un **cambio de contexto (Context Switch)** entre el *Modo Usuario* y el *Modo Núcleo (Kernel)*. Para maximizar el rendimiento pedagógico y evitar millones de llamadas `write()` de 1 byte, el proyecto agrupa la E/S en bloques de **4096 bytes (4 KB)**, coincidiendo exactamente con el tamaño de página de memoria virtual del SO.

---

## 📁 Estructura del Proyecto

* [**`huffman.h`**](file:///home/edi/DOCENCIA2026/SO2026B/concurrencia/Huffman/Huffman_secuencia/huffman.h): Prototipos de funciones, inclusiones de llamadas al sistema (`<unistd.h>`, `<fcntl.h>`, `<sys/stat.h>`) y tipos de datos.
* [**`huffman.c`**](file:///home/edi/DOCENCIA2026/SO2026B/concurrencia/Huffman/Huffman_secuencia/huffman.c): Implementación con llamadas al sistema (`open`, `read`, `write`, `lseek`, `close`, `stat`, `mkdir`) y explicaciones teóricas detalladas.
* [**`main.c`**](file:///home/edi/DOCENCIA2026/SO2026B/concurrencia/Huffman/Huffman_secuencia/main.c): Interfaz de consola estilizada en colores ANSI que muestra el flujo y las syscalls empleadas.
* [**`Makefile`**](file:///home/edi/DOCENCIA2026/SO2026B/concurrencia/Huffman/Huffman_secuencia/Makefile): Reglas de compilación y prueba automática.
* [**`run.sh`**](file:///home/edi/DOCENCIA2026/SO2026B/concurrencia/Huffman/Huffman_secuencia/run.sh): Script interactivo de ejecución y demostración automatizada.
* [**`README.md`**](file:///home/edi/DOCENCIA2026/SO2026B/concurrencia/Huffman/Huffman_secuencia/README.md): Documentación del proyecto.

---

## 🧩 Funciones Principales

El proyecto sigue un diseño modular donde cada función cumple un objetivo pedagógico concreto:

### 1. `leer_archivo`

* **Firma**: `unsigned char* leer_archivo(const char *ruta_archivo, size_t *tamano_salida)`
* **Llamadas al sistema**: `open(O_RDONLY)` -> `lseek(SEEK_END)` -> `lseek(SEEK_SET)` -> `read()` en bucle -> `close()`.
* **Concepto de SO**: Obtiene el descriptor de archivo (`int fd`), consulta su tamaño en el Kernel, reserva memoria en el Heap con `malloc()` y lee el contenido.

### 2. `contar_frecuencias`

* **Firma**: `void contar_frecuencias(const unsigned char *datos, size_t tamano_datos, uint32_t frecuencias[MAX_SIMBOLOS])`
* **Propósito**: Itera sobre los datos calculando cuántas veces aparece cada byte posible (de 0 a 255).

### 3. `crear_arbol_huffman`

* **Firma**: `NodoHuffman* crear_arbol_huffman(const uint32_t frecuencias[MAX_SIMBOLOS])`
* **Propósito**: Aplica una estrategia codiciosa (*Greedy*) uniendo iterativamente los dos nodos de menor frecuencia bajo un nuevo nodo padre.

### 4. `generar_codigos`

* **Firma**: `void generar_codigos(NodoHuffman *raiz, Diccionario *diccionario)`
* **Propósito**:
  ```

  ```

  Recorrido en profundidad (DFS) por el árbol asignando `'0'` a la izquierda y `'1'` a la derecha.

### 5. `guardar_comprimido`

* **Firma**: `int guardar_comprimido(const char *ruta_salida, ...)`
* **Llamadas al sistema**: `open(..., O_WRONLY | O_CREAT | O_TRUNC, 0644)` -> `write()` de cabecera -> `write()` en bloques de 4 KB con `EscritorBits` -> `close()`.
* **Concepto de SO**: Creación de inodos con máscara de permisos `0644` y empaquetamiento de bits optimizado para páginas de disco.

### 6. `descomprimir_archivo`

* **Firma**: `int descomprimir_archivo(const char *ruta_comprimido, const char *ruta_descomprimido)`
* **Llamadas al sistema**: `open(O_RDONLY)` -> `read()` de cabecera -> `open(O_WRONLY | O_CREAT | O_TRUNC, 0644)` -> decodificación con `LectorBits` -> `write()` de salida -> `close()`.

### 7. `liberar_arbol`

* **Firma**: `void liberar_arbol(NodoHuffman *raiz)`
* **Propósito**: Recorrido post-orden que libera la memoria dinámica con `free()`, enseñando la importancia de devolver la memoria solicitada al sistema operativo.

### 8. `asegurar_directorio_padre`

* **Firma**: `void asegurar_directorio_padre(const char *ruta_archivo)`
* **Propósito**: Extrae el nombre del subdirectorio (por ejemplo, `salidas/`) y, si no existe, invoca la llamada al sistema `mkdir(..., 0755)` para crearlo automáticamente antes de intentar abrir el archivo en disco.

### 9. `registrar_archivo_salida`

* **Firma**: `void registrar_archivo_salida(const char *ruta_registro, ...)`
* **Propósito**: Añade una entrada con marca de tiempo al archivo de bitácora y registro de salidas (`salidas/registro_salidas.txt`), documentando todas las operaciones (compresión, descompresión, tamaños y ahorros).

---

## 📂 Organización de Archivos de Salida

Para mantener ordenado el espacio de trabajo, el programa y los scripts dirigen los archivos generados a la carpeta `salidas/`:

* `salidas/<nombre>.huf`: Archivo comprimido con encabezado y bits empaquetados.
* `salidas/<nombre>_recuperado.txt`: Archivo restaurado tras descompresión.
* `salidas/registro_salidas.txt`: **Archivo de bitácora** donde se registran permanentemente todos los archivos de salida generados, su fecha, ruta y métricas.

## 🛠️ Compilación y Ejecución

### Ejecución Rápida y Demostración Automatizada:

El proyecto cuenta con un script interactivo con colores y validaciones automáticas:

```bash
./run.sh
```

Este script:

1. Recompila el proyecto con GCC.
2. Genera el archivo de prueba con texto explicativo de Sistemas Operativos.
3. Lo comprime hacia `salidas/` mostrando la tabla de códigos Huffman en color y métricas en vivo.
4. Lo descomprime recuperando el archivo original.
5. Comprueba byte a byte con `diff -s` que la descompresión sea idéntica.
6. Muestra un análisis comparativo de tamaños y muestra las últimas entradas de `salidas/registro_salidas.txt`.

---

### Compilar manualmente:

```bash
make
```

### Ejecutar pruebas del Makefile:

```bash
make test
```

### Comprimir manualmente:

```bash
./huffman -c archivo_origen.txt archivo_comprimido.huf
```

### Descomprimir manualmente:

```bash
./huffman -d archivo_comprimido.huf archivo_recuperado.txt
```

### Limpiar archivos generados:

```bash
make clean
```
