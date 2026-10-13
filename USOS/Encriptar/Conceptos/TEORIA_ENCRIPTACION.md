# Fundamentos Teóricos de Criptografía y Mecánica de Cifrado en Sistemas Operativos

Este documento complementa la suite de software desarrollada en C para la asignatura de **Sistemas Operativos (SO2026B)**. Su objetivo es desglosar en profundidad los fundamentos matemáticos, conceptuales y prácticos de los procesos de **encriptación (cifrado)** y **desencriptación (descifrado)**, analizando cada algoritmo implementado, sus propósitos, fortalezas, debilidades y su interacción con las llamadas al sistema (*System Calls*) del núcleo Linux.

---

## 1. Fundamentos de Criptografía

### 1.1 Definición y Objetivos Clave
La **criptografía** es la disciplina científica encargada de transformar la información para proteger su confidencialidad, integridad, autenticidad y el no repudio.

* **Texto en claro ($P$ - *Plaintext*):** Los datos legibles originales (código fuente, documentos, binarios ejecutables, imágenes).
* **Texto cifrado ($C$ - *Ciphertext*):** La secuencia de bytes resultante tras aplicar una transformación matemática ininteligible para un tercero no autorizado.
* **Clave secreta ($K$ - *Key*):** Parámetro que parametrizará la función matemática del cifrador.
* **Cifrado ($E$ - *Encryption*):** Función de transformación tal que $C = E_K(P)$.
* **Descifrado ($D$ - *Decryption*):** Función inversa tal que $P = D_K(C) = D_K(E_K(P))$.

```
                +-----------------+
  Plaintext (P) |  "Hola Mundo"   |
                +--------+--------+
                         |
                         v
             +-----------------------+
             |  Función de Cifrado   |<--- Clave Secreta (K)
             |       E_K(P)          |
             +-----------+-----------+
                         |
                         v
                +-----------------+
 Ciphertext (C) |  0xA4 0x1F 0x89 |  (Ininteligible sin K)
                +--------+--------+
                         |
                         v
             +-----------------------+
             | Función de Descifrado |<--- Clave Secreta (K)
             |       D_K(C)          |
             +-----------+-----------+
                         |
                         v
                +-----------------+
  Plaintext (P) |  "Hola Mundo"   |  (Restauración exacta)
                +-----------------+
```

### 1.2 El Principio de Kerckhoffs
Formulado por Auguste Kerckhoffs en el siglo XIX, establece una máxima central de la seguridad informática moderna:

> *"Un sistema criptográfico debe ser seguro incluso si todo sobre el sistema es del dominio público, con la única excepción de la clave secreta."*

La seguridad por oscuridad (ocultar cómo funciona el código) no es seguridad real. Todos los algoritmos implementados en este proyecto son de dominio público y su seguridad matemática recae exclusivamente en la preservación de la clave $K$.

### 1.3 Clasificación: Cifradores de Flujo vs Cifradores de Bloque
Desde la perspectiva de la gestión de memoria y E/S en Sistemas Operativos, los cifradores simétricos se dividen en dos categorías:

1. **Cifradores de Flujo (*Stream Ciphers*):**
   - Procesan los datos byte a byte (o bit a bit) de forma continua.
   - Generan una secuencia pseudoaleatoria de bytes llamada *keystream* ($S$) que se combina mediante la operación XOR con los datos de entrada.
   - **Ventaja para el SO:** No requieren rellenar el archivo (*padding*). El tamaño del archivo de salida es **exactamente idéntico** al de entrada. Son óptimos para procesamiento continuo sobre tuberías (*pipes*), sockets y lecturas secuenciales con `read(2)`.
   - **Ejemplos en el proyecto:** XOR Polialfabético y RC4.

2. **Cifradores de Bloque (*Block Ciphers*):**
   - Dividen el archivo en fragmentos de tamaño fijo ($B$ bytes).
   - Si el último bloque tiene menos de $B$ bytes, es indispensable aplicar una técnica de relleno (*padding*).
   - **Ventaja para el SO:** Permiten estructurar los buffers en el espacio de usuario alineados con los sectores del disco o bloques del sistema de archivos (`st_blksize`).
   - **Ejemplo en el proyecto:** Transposición Columnar por Bloques (con cabecera para control de longitud exacta).

---

## 2. La Interacción Criptografía - Sistema Operativo

Un software criptográfico no opera en el vacío; su desempeño y robustez dependen estrechamente de cómo interactúa con el Núcleo (*Kernel*) del Sistema Operativo.

```
+---------------------------------------------------------------+
|                      ESPACIO DE USUARIO                       |
|                                                               |
|   [ file_cipher ]                                             |
|        |                                                      |
|   Buffers de Usuario (malloc / heap)                          |
|   Transformaciones Criptográficas (César, XOR, RC4, Matriz)   |
+-------------------------------+-------------------------------+
                                |
                   Llamadas al Sistema (System Calls)
       open(2), read(2), write(2), close(2), fstat(2), mmap(2)
                                |
+-------------------------------+-------------------------------+
|                       ESPACIO DE NÚCLEO                       |
|                                                               |
|   VFS (Virtual File System)                                   |
|   Page Cache (Caché de páginas de memoria RAM)                |
|   Gestor de Memoria Virtual (MMU / Tablas de Páginas)         |
|   Drivers de Almacenamiento (ext4, btrfs, NVMe, SATA)         |
+---------------------------------------------------------------+
```

### 2.1 Descriptores de Archivos y Tabla del Proceso
Cuando se ejecuta `open(2)`, el kernel busca una entrada libre en la **Process File Descriptor Table**. Esta tabla apunta a una estructura abierta en la tabla global de archivos abiertos del kernel (*System-wide Open File Table*), la cual a su vez referencia el **inodo** (*inode*) físico en el sistema de archivos.

Operar a nivel de `int fd` en lugar del tipo abstracto `FILE*` de la biblioteca de C:
- Elimina capas de doble búfer ocultas en espacio de usuario.
- Brinda control granular sobre banderas atómicas como `O_CREAT | O_TRUNC | O_WRONLY`.
- Permite invocar primitivas avanzadas del kernel como `fstat(2)`, `ftruncate(2)` y `mmap(2)` sobre el mismo descriptor.

### 2.2 Buffers de Bloque vs Tamaño del Sistema de Archivos (`st_blksize`)
El campo `st_blksize` devuelto por `fstat(2)` indica el tamaño preferido por el controlador de disco para transferencias de E/S (comúnmente 4096 bytes). 

Si un programa lee o escribe en bloques desalineados (por ejemplo de 13 bytes en 13 bytes), el kernel debe realizar operaciones de lectura-modificación-escritura en la *Page Cache*, generando un elevado número de **cambios de contexto** (*Context Switches*) entre Modo Usuario y Modo Núcleo. Nuestra aplicación dimensiona dinámicamente los buffers de trabajo utilizando este valor, maximizando el rendimiento (*throughput*).

### 2.3 Memoria Virtual y Copia Cero (*Zero-Copy*) con `mmap(2)`
La llamada al sistema `mmap(2)` asocia un rango de páginas del archivo directamente a las direcciones virtuales del proceso.

1. **Sin mmap (Flujo tradicional con read/write):**
   - Disco $\rightarrow$ Page Cache (Kernel) $\rightarrow$ Buffer de Usuario (`malloc`) $\rightarrow$ Cifrado $\rightarrow$ Page Cache (Kernel) $\rightarrow$ Disco.
   - Requiere 2 copias de memoria en RAM por cada bloque transferido.
2. **Con mmap (*Zero-Copy*):**
   - El kernel crea una entrada en la tabla de páginas del proceso apuntando directamente a las páginas de la *Page Cache*.
   - El algoritmo cifra modificando la memoria virtual. Al acceder a cada página se dispara un fallo de página menor (*Minor Page Fault*), cargando los datos de forma transparente.
   - La llamada `msync(..., MS_SYNC)` instruye al kernel para que vacíe las páginas sucias (*dirty pages*) al almacenamiento físico de forma atómica.

---

## 3. Análisis Detallado de los Algoritmos Implementados

---

### Algoritmo 1: Cifrado César Extendido (Byte-Shift Substitution)

#### Fundamento Matemático
El cifrado César clásico operaba históricamente sobre las 26 letras del alfabeto latino. En este proyecto se implementa la versión **Extendida a Nivel de Byte**, operando en el anillo algebraico modular $\mathbb{Z}_{256}$:

$$\text{Espacio de estados: } \Sigma = \{0, 1, 2, \dots, 255\} \quad (\text{1 byte})$$

* **Función de Cifrado:**
  $$C_i = (P_i + k) \pmod{256}$$
* **Función de Descifrado:**
  $$P_i = (C_i - k + 256) \pmod{256}$$

Donde $P_i$ es el $i$-ésimo byte del archivo de entrada, $C_i$ es el byte resultante y $k \in \mathbb{Z}$ es el desplazamiento (*shift*).

#### Propósito
Demostrar el concepto más básico de **sustitución monoalfabética** y el procesamiento secuencial por bloques a nivel de byte con las llamadas al sistema `read(2)` y `write(2)`.

#### Bondades y Ventajas
* **Universalidad binaria:** Puede cifrar cualquier tipo de archivo (PDFs, ejecutables ELF, imágenes PNG, audio MP3 o texto plano) sin alterar la longitud del archivo ni corromper bytes no imprimibles.
* **Consumo de memoria mínimo:** Requiere un único buffer de tamaño fijo en RAM (un bloque de 4 KiB).
* **Alta velocidad de ejecución:** La transformación se reduce a una adición y una máscara a nivel de registro (`(byte + shift) & 0xFF`).

#### Limitaciones y Debilidades
* **Espacio de claves ínfimo:** Solo existen 256 claves posibles ($k \in [0, 255]$). Un ataque por fuerza bruta toma fracciones de milisegundo.
* **Preservación del histograma de frecuencias:** La distribución estadística de los bytes se traslada idénticamente; los patrones binarios originales se conservan con un simple desfase constante.

#### Aplicabilidad y Casos de Uso
* Fines pedagógicos para ilustrar la mecánica de transformación en el espacio de usuario.
* Ofuscación elemental de cadenas (*obfuscation*) en binarios donde no se requiera resistencia criptográfica ante atacantes.

---

### Algoritmo 2: Cifrado XOR Polialfabético (Cyclic XOR Stream)

#### Fundamento Matemático
Basado en el álgebra booleana y la compuerta lógica XOR ($\oplus$). La operación XOR goza de propiedades algebraicas fundamentales:

1. **Conmutatividad:** $A \oplus B = B \oplus A$
2. **Asociatividad:** $(A \oplus B) \oplus C = A \oplus (B \oplus C)$
3. **Elemento neutro:** $A \oplus 0 = A$
4. **Involución (Autoinversión):** $A \oplus A = 0 \implies (A \oplus K) \oplus K = A$

Dada una clave $K$ de longitud $L$ ($K = [k_0, k_1, \dots, k_{L-1}]$):
* **Función de Cifrado:**
  $$C_i = P_i \oplus K_{i \pmod L}$$
* **Función de Descifrado:**
  $$P_i = C_i \oplus K_{i \pmod L}$$

#### Propósito
Ilustrar la **involución simétrica** y la necesidad de **mantener la sincronización de estado de la clave** entre múltiples llamadas sucesivas al sistema `read(2)`.

#### El Reto en Sistemas Operativos: Continuidad en Fronteras de Bloque
Cuando un archivo se lee en fragmentos (por ejemplo de 4096 bytes con `read(2)`), el índice de la clave **no debe reiniciarse en cero al comenzar el siguiente bloque**. Si se reiniciara, el descifrado desfasaría la clave y corrompería el archivo.

En `src/crypto_xor.c`, la variable:
```c
key_idx = (key_idx + 1) % key_len;
```
se preserva entre llamadas de lectura consecutivas, garantizando un flujo matemáticamente ininterrumpido a lo largo de todo el archivo.

#### Bondades y Ventajas
* **Código idéntico para cifrar y descifrar:** Gracias a la involución de la compuerta XOR, la lógica de recuperación es simétrica.
* **Excelente velocidad de procesamiento:** La instrucción XOR se ejecuta en un solo ciclo de reloj de la CPU por palabra de máquina.
* **Cero sobrecosto de almacenamiento:** No añade cabeceras ni relleno.

#### Limitaciones y Debilidades
* **Ataque de clave repetida:** Si la longitud de la clave $L$ es corta, un analista puede determinar $L$ mediante el método de Kasiski o el Índice de Coincidencia de Friedman, descomponiendo el problema en $L$ cifradores César independientes.
* **Vulnerabilidad a texto en claro conocido (*Known-Plaintext Attack*):** Si el atacante conoce o adivina parte del texto original (como la cabecera `%PDF` o `\x7fELF`), puede obtener la clave inmediatamente:
  $$K_i = P_i \oplus C_i$$

#### Aplicabilidad y Casos de Uso
* Si la clave fuera verdaderamente aleatoria, de la misma longitud del archivo y se utilizara una sola vez, se convertiría en un **One-Time Pad (Libreta de un solo uso / Cifrado Vernam)**, el cual posee **seguridad matemática perfecta (irrompible según la teoría de la información de Claude Shannon)**.
* Protocolos ligeros de ofuscación de telecomunicaciones y firmware embebido.

---

### Algoritmo 3: Cifrador de Flujo RC4 (Rivest Cipher 4)

#### Fundamento Matemático y Mecánica Interna
Diseñado por Ronald Rivest en 1987, RC4 es uno de los cifradores de flujo más utilizados en la historia de la informática (empleado históricamente en SSL/TLS, WEP y WPA para Wi-Fi).

RC4 opera manteniendo una permutación interna de todos los 256 bytes posibles en un vector denominado **S-Box** ($S = [0, 1, \dots, 255]$), controlado por dos punteros de 8 bits ($i, j$).

```
                +------------------------------------+
  Clave (K) --->| KSA: Key-Scheduling Algorithm      |
                | Mezcla inicial de S-Box con la clave|
                +-----------------+------------------+
                                  |
                                  v
                        [ S-Box inicializada ]
                                  |
            +---------------------+---------------------+
            |                                           |
            v                                           v
+------------------------+                     +-----------------+
| PRGA: Generador        |                     | Bloque de Datos |
| de Flujo Pseudoaleatorio|                     | Leído con read()|
+-----------+------------+                     +--------+--------+
            | Keystream (Z)                             | Bytes (P)
            +--------------------->(+)------------------+
                                    | Operación XOR
                                    v
                            Bytes Cifrados (C)
                            Escritos con write()
```

#### Paso 1: Algoritmo de Programación de Claves (KSA - *Key-Scheduling Algorithm*)
Inicializa la S-Box como la permutación identidad y luego la permuta pseudoaleatoriamente utilizando los bytes de la clave secreta:

```c
for i = 0 to 255:
    S[i] = i
j = 0
for i = 0 to 255:
    j = (j + S[i] + Key[i % key_length]) mod 256
    swap(S[i], S[j])
```

#### Paso 2: Algoritmo de Generación Pseudoaleatoria (PRGA - *Pseudo-Random Generation Algorithm*)
Para cada byte a cifrar o descifrar, se altera dinámicamente la S-Box y se emite un byte de flujo pseudoaleatorio ($Z$):

```c
i = (i + 1) mod 256
j = (j + S[i]) mod 256
swap(S[i], S[j])
t = (S[i] + S[j]) mod 256
keystream_byte = S[t]

ciphertext_byte = plaintext_byte ^ keystream_byte
```

#### Propósito
Representar en el proyecto un **algoritmo de grado industrial clásico** que desacopla por completo la generación matemática de la clave de flujo de la dimensión de los bloques de E/S del sistema de archivos.

#### Bondades y Ventajas
* **Extraordinaria eficiencia:** No requiere tablas trigonométricas, exponenciación modular ni operaciones de división compleja. Funciona exclusivamente con sumas en módulo 256 e intercambios de bytes en memoria.
* **Huella de memoria insignificante:** El contexto completo del cifrador ocupa exactamente **258 bytes** en la memoria del proceso (256 bytes para la S-Box + 2 bytes para los índices $i$ y $j$).
* **Streaming nativo:** Excelente integración con flujos continuos sobre descriptores de archivo en Linux.

#### Limitaciones y Debilidades
* **Sesgo en los primeros bytes (*Fluhrer, Mantin and Shamir Attack - FMS*):** Los primeros bytes generados por el PRGA filtran información estadística sobre la clave secreta (debilidad que condujo a la ruptura del protocolo Wi-Fi WEP). Para mitigar esto, protocolos modernos descartaban los primeros 768 o 3072 bytes generados (*RC4-drop*).
* Prohibido en versiones contemporáneas de TLS (RFC 7465).

#### Aplicabilidad y Casos de Uso
* Estudio del diseño de generadores congruenciales y cifradores de flujo basados en permutaciones de estado.
* Entornos con procesadores de muy bajo consumo o microcontroladores donde las operaciones aritméticas complejas no están disponibles en hardware.

---

### Algoritmo 4: Cifrado por Transposición Columnar por Bloques

#### Fundamento Matemático y Mecánica
A diferencia de los tres anteriores (que son cifradores de sustitución o flujo), la transposición **no modifica el valor de los bytes, sino que permuta sus posiciones espaciales**.

El algoritmo implementado organiza los datos en matrices bidimensionales en memoria (*Heap*):
- Número de columnas: $C = \text{longitud de la clave}$.
- Número de filas por bloque: $R = 64$.
- Dimensión del bloque: $B = R \times C$ bytes.

```
Texto a cifrar: "SISTEMAS_OPERATIVOS_LINUX"
Clave: "CLAVE" (5 columnas)

1. Ordenamiento lexicográfico de la clave:
   Clave original :  C  L  A  V  E
   Índice original:  0  1  2  3  4
   Orden alfabético: 'A' < 'C' < 'E' < 'L' < 'V'
   Permutación    : [2,  0,  4,  1,  3]

2. Llenado matricial por filas:
   Fila 0:  S   I   S   T   E
   Fila 1:  M   A   S   _   O
   Fila 2:  P   E   R   A   T
   Fila 3:  I   V   O   S   _
   ...

3. Lectura de salida transpuesta (por columnas en el orden permutado):
   Columna 2 primero ("SSRO...") -> Columna 0 ("SMPI...") -> Columna 4 ...
```

#### Manejo de Metadatos del Sistema de Archivos: El Problema del Relleno (*Padding*)
En un cifrador de bloques, si el tamaño del archivo de entrada no es un múltiplo exacto de $B$, el último bloque debe rellenarse (con ceros).

**El desafío:** Al descifrar, ¿cómo sabe el programa cuántos bytes son datos reales y cuántos son relleno para no corromper el archivo original?

**Nuestra solución a nivel de SO:** Se diseñó una estructura de cabecera binaria empaquetada de 16 bytes que se graba al inicio del archivo cifrado:

```c
#pragma pack(push, 1)
typedef struct {
    uint32_t magic;         // 0x54525350 ("TRSP" en ASCII)
    uint64_t original_size; // Tamaño exacto original del archivo (st_size)
    uint32_t col_count;     // Cantidad de columnas utilizadas
} TranspositionHeader;
#pragma pack(pop)
```

Al descifrar:
1. El programa lee la cabecera mediante `read(fd_in, &hdr, sizeof(hdr))`.
2. Verifica el número mágico `TRSP`.
3. Al procesar el último bloque, escribe exactamente los bytes restantes hasta alcanzar `original_size`, descartando el relleno de forma limpia y garantizando la correspondencia exacta bit a bit.

#### Bondades y Ventajas
* **Dispersión de patrones (*Difusión*):** Altera drásticamente la correlación espacial de los bytes en el archivo.
* **Modelo pedagógico de cifrador de bloques:** Ilustra claramente el uso de cabeceras binarias, alineación de estructuras y gestión de matrices dinámicas en el Heap con `malloc` y `qsort`.

#### Limitaciones y Debilidades
* **Consumo de memoria:** Requiere almacenar bloques completos en memoria antes de poder emitir un solo byte de salida.
* **Latencia de bloque:** No puede operar en streaming estricto byte a byte.
* Vulnerable al criptoanálisis de anagramación si la longitud de bloque es reducida.

---

### Algoritmo 5: Cifrado en Memoria Virtual vía `mmap(2)` (*Zero-Copy*)

#### Fundamento y Mecánica de Sistemas Operativos
Este algoritmo representa la cúspide de la integración entre criptografía y los subsistemas del Núcleo Linux: **el Administrador de Memoria Virtual (*Virtual Memory Manager - VMM*) y la Unidad de Gestión de Memoria (*MMU*) del procesador**.

En lugar de utilizar llamadas `read(2)` o `write(2)`, el programa ejecuta:

```c
uint8_t *addr = (uint8_t *)mmap(NULL, file_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
```

```
Espacio Virtual del Proceso                   Páginas Físicas en Memoria RAM
(Direcciones 0x7FFF...)                              (Page Cache del Kernel)
+-----------------------+                            +-----------------------+
| Página Virtual 0      | -------------------------> | Página Física A       |
+-----------------------+                            +-----------------------+
| Página Virtual 1      | -------------------------> | Página Física B       |
+-----------------------+                            +-----------------------+
| Página Virtual 2      | -------------------------> | Página Física C       |
+-----------------------+                            +-----------------------+
           |                                                     |
           | Puntero 'addr[i]' modificado                        | msync(MS_SYNC)
           v                                                     v
   Acceso directo CPU                                Almacenamiento Secundario (Disco)
```

#### Modo In-Situ (*In-Place*) vs Modo Copia

1. **Modo In-Situ (`path_out == NULL` o igual a `path_in`):**
   - El archivo se abre con `open(path, O_RDWR)`.
   - Se mapea con `MAP_SHARED`. Las escrituras realizadas en el puntero `addr[i]` modifican directamente las páginas de la caché del kernel.
   - La llamada `msync(addr, size, MS_SYNC)` fuerza al kernel a volcar los bloques sucios a los sectores del disco.
   - **Resultado:** El archivo se cifra **en su propio espacio en disco sin requerir espacio libre adicional ni archivos temporales**.

2. **Modo Copia (`path_out != path_in`):**
   - El archivo origen se abre en modo `O_RDONLY` y se mapea con `PROT_READ`.
   - El archivo destino se crea con `open(path_out, O_RDWR | O_CREAT | O_TRUNC, mode)`.
   - **Uso crítico de `ftruncate(2)`:** En sistemas tipo UNIX, un archivo recién creado mide 0 bytes. Si se intentara mapear un archivo de 0 bytes y acceder a una dirección, la CPU recibiría una excepción de fallo de bus (`SIGBUS`). Por ende, se invoca `ftruncate(fd_out, st_in.st_size)` para asignar el tamaño en el inodo antes de mapearlo con `PROT_READ | PROT_WRITE`.
   - La transformación se ejecuta transfiriendo directamente los datos entre ambos mapeos de memoria virtual.

#### Bondades y Ventajas
* **Rendimiento Máximo (*Zero-Copy*):** Elimina el costo de copiar buffers repetidamente entre el espacio de usuario y el espacio de núcleo.
* **Cifrado In-Situ:** Capacidad de cifrar archivos de gigabytes en discos casi llenos, ya que no duplica el archivo en el sistema de archivos.
* **Gestión automática de paginación:** El núcleo de Linux se encarga automáticamente de cargar y descargar páginas bajo demanda mediante su algoritmo de reemplazo de páginas (LRU / Page Reclaim).

#### Limitaciones y Consideraciones
* No es aplicable sobre flujos continuos que no permitan acceso aleatorio (como tuberías FIFO o sockets de red).
* En arquitecturas de 32 bits, el espacio de direcciones virtuales limita los archivos mapeables a un máximo de ~2 a 3 GB (en 64 bits este límite asciende a 128 TB).

---

## 4. Matriz Comparativa de los Algoritmos

| Característica | César Extendido | XOR Polialfabético | RC4 (Stream) | Transposición Columnar | Memoria Virtual (`mmap`) |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Categoría** | Sustitución Monoalfabética | Cifrador de Flujo | Cifrador de Flujo | Cifrador de Bloque | Flujo en Memoria Virtual |
| **Operación Principal** | Adición modular ($+ k \pmod{256}$) | Compuerta XOR ($\oplus$) | Permutación S-Box + XOR | Permutación matricial | Transformación bit a bit |
| **Simetría** | Asimétrico ($+k$ vs $-k$) | Totalmente Simétrico | Totalmente Simétrico | Inversa por permutación | Totalmente Simétrico |
| **Sobrecosto de Tamaño** | 0 bytes | 0 bytes | 0 bytes | +16 bytes (Cabecera `TRSP`) | 0 bytes |
| **Complejidad Temporal** | $O(N)$ | $O(N)$ | $O(N)$ | $O(N \log K + N)$ | $O(N)$ |
| **Complejidad Espacial** | $O(1)$ (Buffer de bloque) | $O(1)$ (Buffer de bloque) | $O(1)$ (258 bytes contexto) | $O(R \times C)$ (Matriz en Heap) | $O(1)$ en espacio de usuario |
| **Llamadas Clave del SO** | `read`, `write`, `fstat` | `read`, `write`, `fstat` | `read`, `write`, `fstat` | `read`, `write`, `fstat` | `mmap`, `msync`, `ftruncate`, `munmap` |
| **Soporte In-Situ** | No (Requiere archivo destino) | No (Requiere archivo destino) | No (Requiere archivo destino) | No (Requiere archivo destino) | **Sí (Modifica disco directo)** |
| **Nivel de Seguridad** | Nulo (256 claves) | Bajo (clave repetida) | Medio-Bajo (vulnerable FMS) | Bajo si la clave es corta | Depende de la función de byte |

---

## 5. Glosario de Conceptos Integrados

* **Inodo (*Inode*):** Estructura de datos en el sistema de archivos de Linux que almacena los metadatos de un archivo (permisos, propietario, tamaño, marcas temporales, punteros a bloques de disco), identificada por un número entero único (`st_ino`).
* **Page Cache:** Mecanismo del kernel de Linux que mantiene páginas de archivos en la memoria RAM principal para evitar lecturas lentas recurrentes desde dispositivos de almacenamiento secundario.
* **Fallo de Página (*Page Fault*):** Interrupción de hardware generada por la MMU cuando un hilo intenta acceder a una dirección de memoria virtual que no se encuentra mapeada físicamente en la memoria principal.
* **Dirty Page (Página Sucia):** Página de memoria virtual que ha sido modificada por el proceso y cuyo contenido difiere del estado almacenado en el disco físico, requiriendo sincronización (`msync` o *flushing* periódico del hilo `kworker`).
* **Short Read / Short Write:** Situación habitual en llamadas al sistema de bajo nivel donde `read(2)` o `write(2)` procesan menos bytes que los solicitados por el programador debido a límites de buffers del kernel o interrupciones.
* **Context Switch (Cambio de Contexto):** Procedimiento del kernel para salvar el estado de ejecución de la CPU (registros, puntero de instrucción) y cambiar entre el espacio de usuario (*ring 3*) y el modo núcleo (*ring 0*).
