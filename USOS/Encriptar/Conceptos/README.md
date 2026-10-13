# Suite de Cifrado de Archivos en C (Linux) orientada a System Calls

Aplicación modular en **C (estándar C11)** para el cifrado y descifrado de archivos en sistemas Linux, diseñada con un enfoque pedagógico y práctico en **Conceptos de Sistemas Operativos** y **Llamadas al Sistema (System Calls POSIX)**.

---

## 1. Arquitectura Modular del Proyecto

El proyecto sigue una separación estricta de responsabilidades: **un archivo de código fuente por algoritmo de cifrado**, una capa de abstracción de E/S pura en System Calls, y un controlador principal CLI.

```
.
├── Makefile                      # Automatización de compilación y suite de pruebas
├── README.md                     # Documentación técnica de arquitectura y guía de uso
├── TEORIA_ENCRIPTACION.md        # Documento teórico completo sobre criptografía y conceptos de SO
├── run.sh                        # Script lanzador interactivo y de pruebas
├── include/
│   ├── common.h                  # Definiciones globales, códigos de estado y tipos
│   ├── sys_io.h                  # Declaraciones de la capa de E/S de System Calls
│   ├── crypto_caesar.h           # Módulo Cifrado César (Byte-shift)
│   ├── crypto_xor.h              # Módulo Cifrado XOR Polialfabético
│   ├── crypto_rc4.h              # Módulo Cifrado de Flujo RC4 (KSA + PRGA)
│   ├── crypto_transposition.h    # Módulo Cifrado por Transposición Columnar
│   └── crypto_mmap.h             # Módulo Cifrado por Mapeo de Memoria (mmap)
└── src/
    ├── main.c                    # Punto de entrada, parseo CLI y benchmark
    ├── sys_io.c                  # Implementación de llamadas al sistema y manejo de errores
    ├── crypto_caesar.c           # Algoritmo César sobre buffers de bloque
    ├── crypto_xor.c              # Algoritmo XOR continuo sobre bloques de disco
    ├── crypto_rc4.c              # Cifrador de flujo con S-Box en espacio de usuario
    ├── crypto_transposition.c    # Cifrador de bloque matricial con cabecera de metadatos
    └── crypto_mmap.c             # Cifrado en memoria virtual (Zero-Copy)
```

---

## 2. Conceptos de Sistemas Operativos Aplicados

### A. Descriptores de Archivo (File Descriptors) vs Biblioteca Estándar (`FILE*`)
- En lugar de utilizar las funciones de alto nivel de `stdio.h` (`fopen`, `fread`, `fwrite`), la aplicación interactúa directamente con el núcleo a través de **enteros descriptores** (`int fd`), los cuales son índices en la **Tabla de Descriptores de Archivos del Proceso** (*Process File Descriptor Table*).

### B. Llamadas al Sistema (System Calls)
El proyecto implementa y explica las siguientes llamadas al sistema:
| Syscall | Propósito en la Aplicación |
| :--- | :--- |
| `open(2)` | Solicita al kernel un descriptor con flags como `O_RDONLY`, `O_WRONLY`, `O_CREAT`, `O_TRUNC` y permisos `mode_t`. |
| `read(2)` | Transfiere bloques de datos desde la caché del kernel (*Page Cache*) al buffer en espacio de usuario. |
| `write(2)` | Transfiere bloques procesados desde el espacio de usuario a la caché del kernel. |
| `close(2)` | Libera el descriptor en la tabla del proceso. |
| `fstat(2)` | Consulta los metadatos del inodo en el VFS (`st_size`, `st_ino`, `st_blksize`, `st_mode`). |
| `mmap(2)` | Mapea un archivo directamente en el espacio de direcciones virtuales del proceso (*Zero-Copy*). |
| `munmap(2)` | Libera el mapeo de memoria virtual asignado al proceso. |
| `msync(2)` | Fuerza la sincronización de páginas virtuales sucias (*dirty pages*) al disco físico (`MS_SYNC`). |
| `ftruncate(2)` | Ajusta el tamaño de archivo en el sistema de archivos antes de proyectarlo en memoria virtual. |

### C. Manejo Defensivo de E/S (Short Reads / Short Writes / Señales)
En `src/sys_io.c`:
- Las llamadas `read(2)` y `write(2)` no garantizan transferir todos los bytes solicitados en una única invocación. Se implementaron bucles de control (`sys_read_chunk` y `sys_write_chunk`) que garantizan la transferencia completa.
- Se contempla la interrupción por señales POSIX (`EINTR`), reanudando la operación sin abortar el proceso.
- Manejo de errores con la variable global del kernel `errno` y funciones de reporte `strerror(errno)`.

### D. Optimización de Bloque de I/O (`st_blksize`)
- Se consulta `st.st_blksize` del inodo para que el tamaño del buffer de usuario coincida con el tamaño de bloque óptimo del sistema de archivos subyacente (típicamente 4096 bytes en ext4/xfs), minimizando cambios de contexto (*Context Switches*) entre Modo Usuario y Modo Núcleo.

### E. Memoria Virtual y Copia Cero (*Zero-Copy*) con `mmap(2)`
- En `src/crypto_mmap.c`, se demuestra cómo el proceso puede cifrar datos sin copiar memoria intermedia: el procesador interactúa directamente con las páginas de memoria gestionadas por la MMU y el subsistema de memoria virtual de Linux. Soporta cifrado **In-Situ** (modificando las páginas del archivo directamente) y cifrado con archivo de copia.

### F. Preservación de Metadatos y Permisos
- Al crear el archivo cifrado de salida, se consultan los permisos UNIX originales del archivo de entrada (`st.st_mode & 0777`) y se aplican al archivo resultante mediante `open(..., st_in.st_mode)`.

### G. Medición de Rendimiento con Reloj Monótono del Kernel
- Se utiliza la syscall POSIX `clock_gettime(CLOCK_MONOTONIC)` para medir tiempos de ejecución exactos sin verse afectados por cambios en la hora del sistema (NTP/ajustes manuales), calculando la tasa de transferencia en MB/s.

---

## 3. Algoritmos de Cifrado Incluidos

1. **César Extendido (Byte-Shift)** (`crypto_caesar.c`):
   - Desplaza cada byte en aritmética modular $256$. Acepta cualquier tipo de archivo (binario o texto).
2. **XOR Polialfabético (Stream)** (`crypto_xor.c`):
   - Aplica una clave cíclica byte a byte. Mantiene el índice de rotación de clave consistente entre bloques continuos leídos por `read(2)`.
3. **RC4 / ARC4 (Stream Cipher)** (`crypto_rc4.c`):
   - Cifrador de flujo real con permutación de estado de 256 bytes (*S-Box*), KSA (*Key-Scheduling Algorithm*) y PRGA (*Pseudo-Random Generation Algorithm*).
4. **Transposición Columnar por Bloques** (`crypto_transposition.c`):
   - Cifrador de bloque matricial. Permuta columnas según el orden lexicográfico de la clave.
   - Incluye una cabecera binaria con número mágico `TRSP` y tamaño original en bytes para revertir el relleno (*padding*) de forma exacta.
5. **Cifrado en Memoria Virtual vía `mmap`** (`crypto_mmap.c`):
   - Cifrado de alto rendimiento con llamadas a memoria virtual. Soporta cifrado en archivo nuevo o in-place (in-situ).

---

## 4. Compilación y Ejecución

### Requisitos
- Sistema operativo Linux (kernel 2.6+).
- Compilador GCC con soporte para C11 (`gcc`).
- Herramienta GNU Make (`make`).

### Compilar el proyecto
```bash
make
```
Genera el binario ejecutable `./file_cipher`.

### Ejecutar mediante el script `run.sh`
Se proporciona el script ejecutable [`run.sh`](file:///home/edi/DOCENCIA2026/SO2026B/ENCRIPTACION/Conceptos/run.sh) que compila automáticamente (si no existe el binario) y ofrece tanto un **menú interactivo** como ejecución directa por argumentos:

```bash
# Lanzar el menú interactivo con opciones guiadas:
./run.sh

# Ejecutar pruebas completas:
./run.sh test

# Limpiar archivos compilados:
./run.sh clean

# Ver ayuda general:
./run.sh help
```

### Ejecutar las pruebas automatizadas manualmente

### Limpiar artefactos compilados
```bash
make clean
```

---

## 5. Guía de Uso del CLI (`file_cipher`)

### Sintaxis
```bash
./file_cipher -a <algoritmo> -m <enc|dec> -i <archivo_origen> -o <archivo_destino> -k <clave> [opciones]
```

### Parámetros
- `-a <algoritmo>`: `caesar`, `xor`, `rc4`, `transp`, `mmap`
- `-m <modo>`: `enc` (cifrar) o `dec` (descifrar) (o banderas directas `-e`, `-d`)
- `-i <ruta>`: Archivo de entrada
- `-o <ruta>`: Archivo de salida (opcional para `mmap` si se cifra in-place)
- `-k <clave>`: Clave de cifrado (número entero para `caesar`, texto para el resto)
- `-b`: Activa el modo **Benchmark** (muestra tiempo en milisegundos y MB/s)
- `-s`: Muestra los **Metadatos del Kernel** del archivo (Inodo, permisos, st_blksize, etc.)
- `-h`: Muestra el menú de ayuda detallado

### Ejemplos Prácticos

#### 1. Cifrado César con inspección de metadatos del inodo:
```bash
./file_cipher -a caesar -e -i documento.pdf -o documento.enc -k 27 -s
./file_cipher -a caesar -d -i documento.enc -o documento.restaurado.pdf -k 27
```

#### 2. Cifrado XOR Polialfabético con Benchmark:
```bash
./file_cipher -a xor -e -i base_datos.db -o base_datos.enc -k "LlaveUltraSecreta2026" -b
./file_cipher -a xor -d -i base_datos.enc -o base_datos.dec.db -k "LlaveUltraSecreta2026"
```

#### 3. Cifrado de Flujo RC4:
```bash
./file_cipher -a rc4 -e -i video.mp4 -o video.rc4 -k "MiPassphraseSegura" -b
./file_cipher -a rc4 -d -i video.rc4 -o video.recuperado.mp4 -k "MiPassphraseSegura"
```

#### 4. Cifrado por Transposición Columnar por Bloques:
```bash
./file_cipher -a transp -e -i informe.txt -o informe.transp -k "PERMUTACION"
./file_cipher -a transp -d -i informe.transp -o informe.recuperado.txt -k "PERMUTACION"
```

#### 5. Cifrado por Mapeo de Memoria (`mmap`) hacia nuevo archivo:
```bash
./file_cipher -a mmap -e -i datos.bin -o datos.mmap.enc -k "VirtualPageKey" -b
./file_cipher -a mmap -d -i datos.mmap.enc -o datos.restaurado.bin -k "VirtualPageKey"
```

#### 6. Cifrado In-Situ (In-Place) sobre páginas virtuales directas:
```bash
# Cifra el archivo modificándolo directamente en su espacio virtual en disco
./file_cipher -a mmap -e -i confidencial.txt -k "InPlacePass"

# Descifra el mismo archivo in-situ
./file_cipher -a mmap -d -i confidencial.txt -k "InPlacePass"
```
