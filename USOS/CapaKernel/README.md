# Proyecto Docente: Arquitectura de la Capa Kernel y Optimización con `mmap`

> **Asignatura:** Sistemas Operativos (DOCENCIA 2026-B)  
> **Tema:** Interacción Espacio de Usuario vs. Espacio de Kernel, Entrada/Salida Tradicional, Memoria Virtual y Mecanismo Zero-Copy con `mmap` y Drivers de Dispositivo.

---

## 1. Introducción y Fundamentos Teóricos

En los sistemas operativos modernos como GNU/Linux, la memoria y la ejecución se dividen rígidamente en dos niveles de privilegio del procesador (arquitectura x86/ARM):
- **Espacio de Usuario (*Ring 3*):** Donde corren los procesos ordinarios de las aplicaciones. No tienen acceso directo al hardware ni a la memoria física arbitraria.
- **Espacio de Kernel (*Ring 0*):** Donde reside el núcleo de Linux, con acceso privilegiado al hardware, controladores (drivers) y tablas de páginas de la MMU.

### 1.1 El Enfoque Tradicional: `read()` y `write()` (Doble Búfer y Cambios de Contexto)

Cuando un programa de usuario desea leer datos de un archivo o dispositivo usando `read(fd, buf, size)`:

```
+-----------------------------------------------------------------------------------+
| ESPACIO DE USUARIO (Ring 3)                                                      |
|   Proceso: user_buf[4096]                                                         |
|      ^                                                                            |
|      |  4. copy_to_user() (Copia byte a byte por CPU)                             |
+------|----------------------------------------------------------------------------+
|      |  1. Syscall read() -> Cambio de contexto (Ring 3 -> Ring 0)               |
|      v                                                                            |
| ESPACIO DE KERNEL (Ring 0)                                                        |
|   2. VFS busca descriptor en task_struct->files                                   |
|   3. Page Cache / Driver almacena datos en búfer de kernel (kernel_buffer)         |
|   5. Retorno de Syscall -> Cambio de contexto (Ring 0 -> Ring 3)                  |
+-----------------------------------------------------------------------------------+
```

#### Cuellos de botella de `read()`:
1. **Cambios de Contexto (*Context Switches*):** Cada llamada a `read()` obliga a la CPU a cambiar de anillo de privilegio, guardar registros de usuario, cargar registros de kernel y volver. Para archivos grandes con bloques de 4 KB, esto se repite decenas de miles de veces.
2. **Doble Búfer (*Double Buffering*):** Los datos existen simultáneamente en la memoria del kernel (*Page Cache*) y en el búfer de usuario.
3. **Sobrecarga de CPU en `copy_to_user()`:** La CPU debe leer cada byte de la RAM del kernel a sus registros y escribirlo en la RAM del usuario, duplicando el ancho de banda consumido en el bus de memoria.

---

### 1.2 El Enfoque Optimizado: `mmap()` (Zero-Copy y Paginación por Demanda)

Con `mmap()`, el proceso le solicita al kernel mapear directamente las páginas físicas a su propio espacio de direcciones virtuales:

```
+-----------------------------------------------------------------------------------+
| ESPACIO DE USUARIO (Ring 3)                                                      |
|   Proceso: ptr = mmap(...)                                                        |
|   Dirección Virtual: 0x7f2d45200000 -------------------------\                    |
+---------------------------------------------------------------|-------------------+
| ESPACIO DE KERNEL / HARDWARE (MMU)                             |                   |
|   Tabla de Páginas del Proceso (PTE)                          | Acceso DIRECTO    |
|   mapea la dirección virtual directamente al -----------------+ a velocidad       |
|   Page Frame Number (PFN)                                     | de bus RAM        |
|                                                               v                   |
|   MEMORIA FÍSICA (RAM): Marco de Página Física (Zero-Copy) [ Page Frame ]         |
+-----------------------------------------------------------------------------------+
```

#### Ventajas clave de `mmap()`:
1. **Zero-Copy Real:** No existe `copy_to_user()`. Las direcciones virtuales del proceso apuntan exactamente a las mismas celdas de memoria RAM física del kernel/driver.
2. **Cero llamadas al sistema posteriores:** Una vez establecida la tabla de páginas, millones de lecturas o escrituras se ejecutan mediante instrucciones directas de máquina (`MOV`), sin invocar a `sys_read` ni cambiar a Ring 0.
3. **Paginación por Demanda (*Demand Paging*):** El kernel no carga todo el archivo de golpe; asigna marcos solo cuando ocurre una excepción de hardware **Page Fault** (`#PF`).

---

## 2. Diagrama de Flujo: `read()` vs `mmap()`

```mermaid
flowchart TD
    subgraph Metodo_Tradicional ["1. E/S Tradicional (read)"]
        A1[Proceso llama a read] --> B1[Syscall: Trap a Ring 0]
        B1 --> C1[Kernel localiza datos en RAM / Page Cache]
        C1 --> D1["copy_to_user: CPU copia memoria de kernel a usuario"]
        D1 --> E1[Retorno a Ring 3]
        E1 --> F1[Repetir por cada bloque de 4KB]
    end

    subgraph Metodo_mmap ["2. Memoria Mapeada (mmap)"]
        A2[Proceso llama a mmap una sola vez] --> B2[Kernel crea struct vm_area_struct]
        B2 --> C2[Retorna puntero virtual al usuario]
        C2 --> D2["Lectura: ptr[i]"]
        D2 --> E2{"¿Página en PTE?"}
        E2 -- "No (1er acceso)" --> F2["Hardware: Page Fault (#PF)"]
        F2 --> G2["Kernel: enlaza marco físico a PTE (remap_pfn_range)"]
        G2 --> H2["Acceso directo en RAM"]
        E2 -- "Sí (accesos siguientes)" --> H2
        H2 --> I2["¡Zero-Copy y CERO Syscalls!"]
    end
```

---

## 3. Estructura Incremental del Proyecto

El proyecto está diseñado de forma incremental en 5 pasos didácticos:

```
CapaKernel/
├── Makefile                       # Orquestador general del proyecto
├── README.md                      # Esta guía didáctica
├── paso1_io_clasico/              # Paso 1: Lectura estándar con read()
│   ├── main.c
│   └── Makefile
├── paso2_mmap_usuario/            # Paso 2: Memoria mapeada en espacio de usuario
│   ├── main.c
│   └── Makefile
├── paso3_driver_copy_user/        # Paso 3: Capa Kernel con .read y copy_to_user()
│   ├── kmod_copy_user.c           # Módulo de Kernel (Driver de caracteres)
│   ├── test_copy_user.c           # Aplicación cliente en Espacio de Usuario
│   └── Makefile
├── paso4_driver_mmap_kernel/      # Paso 4: Capa Kernel Optimizada con .mmap (Zero-Copy)
│   ├── kmod_mmap.c                # Módulo de Kernel con remap_pfn_range()
│   ├── test_mmap.c                # Aplicación cliente Zero-Copy
│   └── Makefile
├── paso5_benchmark_comparativo/   # Paso 5: Suite de comparación empírica y strace
│   ├── benchmark.c                # Medición de métricas con getrusage()
│   ├── run_benchmark.sh           # Automatización con strace y comparativa
│   └── Makefile
├── paso6_demand_paging_fault/     # Paso 6: Paginación por Demanda Avanzada en Kernel
│   ├── kmod_demand_paging.c       # Módulo con vm_operations_struct -> fault
│   ├── test_demand_paging.c       # Cliente de prueba con inspección de fallos
│   └── Makefile
├── paso7_ipc_ringbuffer/          # Paso 7: IPC Ultra-Rápido Zero-Copy (13M msg/s)
│   ├── ring_buffer.h              # Estructura de Ring Buffer SPSC lock-free
│   ├── producer.c                 # Productor de mensajes
│   ├── consumer.c                 # Consumidor de mensajes
│   ├── run_ipc.sh                 # Lanzador concurrente
│   └── Makefile
├── herramientas/                  # Herramientas de diagnóstico didáctico
│   ├── vma_inspector.c            # Inspección de /proc/self/pagemap y bits PTE
│   └── Makefile
├── GUIA_LABORATORIO.md            # Guía práctica con 4 experimentos y preguntas de examen
└── scripts/
    ├── load_modules.sh            # Carga de módulos de kernel de forma segura
    └── unload_modules.sh          # Descarga y limpieza de módulos
```

---

## 4. Detalle Paso a Paso de los Códigos

### Paso 1: E/S Clásica en Espacio de Usuario (`paso1_io_clasico`)
- **Código:** [`paso1_io_clasico/main.c`](file:///home/edi/DOCENCIA2026/SO2026B/CapaKernel/paso1_io_clasico/main.c)
- **Concepto clave:** Demuestra cómo para leer un archivo de 32 MB en bloques de 4 KB se ejecutan **8,192 llamadas al sistema `sys_read`**, con 8,192 cambios de contexto y 8,192 operaciones `copy_to_user` en el kernel.
- **Ejecución:**
  ```bash
  make run-paso1
  ```

---

### Paso 2: Optimización con `mmap` en Archivos (`paso2_mmap_usuario`)
- **Código:** [`paso2_mmap_usuario/main.c`](file:///home/edi/DOCENCIA2026/SO2026B/CapaKernel/paso2_mmap_usuario/main.c)
- **Concepto clave:** Mapea el archivo completo con una única llamada `mmap()`. El acceso a los datos ocurre mediante un puntero C (`mapped_data[i]`). 
- **Métricas:** Cero llamadas a `sys_read`. Utiliza `getrusage()` para reportar los *Minor Page Faults* resueltos por la MMU.
- **Ejecución:**
  ```bash
  make run-paso2
  ```

---

### Paso 3: ¿Qué pasa en la Capa Kernel? Driver con `copy_to_user()` (`paso3_driver_copy_user`)
- **Módulo de Kernel:** [`paso3_driver_copy_user/kmod_copy_user.c`](file:///home/edi/DOCENCIA2026/SO2026B/CapaKernel/paso3_driver_copy_user/kmod_copy_user.c)
- **Cliente:** [`paso3_driver_copy_user/test_copy_user.c`](file:///home/edi/DOCENCIA2026/SO2026B/CapaKernel/paso3_driver_copy_user/test_copy_user.c)
- **Concepto en la Capa Kernel:**
  En `struct file_operations`:
  ```c
  static const struct file_operations fops = {
      .owner = THIS_MODULE,
      .read  = dev_read,
  };
  ```
  En la función `dev_read`:
  ```c
  /* El kernel NO puede usar memcpy(buf, kernel_buf) directamente */
  copy_to_user(buf, kernel_buffer + *ppos, bytes_to_copy);
  ```
  **¿Por qué es obligatorio `copy_to_user()` en el kernel?**
  1. **Aislamiento de Espacios de Memoria:** `buf` es una dirección virtual en el espacio del proceso, no del kernel.
  2. **Seguridad (SMAP):** Evita que un proceso malicioso pase punteros hacia memoria crítica del kernel.
  3. **Fallos de Página Seguros:** Si la memoria del usuario no está en RAM o fue enviada al Swap, `copy_to_user` maneja la excepción sin provocar un *Kernel Panic*.

---

### Paso 4: Programación en la Capa Kernel con `mmap` y `remap_pfn_range()` (`paso4_driver_mmap_kernel`)
- **Módulo de Kernel:** [`paso4_driver_mmap_kernel/kmod_mmap.c`](file:///home/edi/DOCENCIA2026/SO2026B/CapaKernel/paso4_driver_mmap_kernel/kmod_mmap.c)
- **Cliente Zero-Copy:** [`paso4_driver_mmap_kernel/test_mmap.c`](file:///home/edi/DOCENCIA2026/SO2026B/CapaKernel/paso4_driver_mmap_kernel/test_mmap.c)
- **¿Cómo se programa en el Kernel?**
  1. **Reserva y fijación de páginas físicas:**
     ```c
     kernel_pages = alloc_pages(GFP_KERNEL | __GFP_ZERO, PAGE_ORDER);
     for (i = 0; i < ALLOC_PAGES_CNT; i++) {
         SetPageReserved(kernel_pages + i); /* Ancla las páginas para que no vayan a swap */
     }
     ```
  2. **Implementación del callback `.mmap` en `struct file_operations`:**
     ```c
     static int dev_mmap(struct file *filp, struct vm_area_struct *vma)
     {
         unsigned long size = vma->vm_end - vma->vm_start;
         unsigned long pfn  = page_to_pfn(kernel_pages);

         /* Reprograma la tabla de páginas del proceso directamente al PFN del kernel */
         return remap_pfn_range(vma, vma->vm_start, pfn, size, vma->vm_page_prot);
     }
     ```
  3. **Estructuras del Kernel explicadas:**
     - `struct vm_area_struct *vma`: Descriptor del rango virtual concedido al proceso.
     - `vma->vm_start` y `vma->vm_end`: Inicio y fin de la dirección virtual de usuario.
     - `vma->vm_page_prot`: Bits de protección de hardware para la entrada PTE.
     - `page_to_pfn()`: Convierte la dirección física del kernel en su *Page Frame Number* para la MMU.
     - `remap_pfn_range()`: Escribe directamente las entradas PTE en la tabla de páginas del proceso.

- **Demostración de Comunicación Bidireccional Zero-Copy:**
  El programa [`test_mmap.c`](file:///home/edi/DOCENCIA2026/SO2026B/CapaKernel/paso4_driver_mmap_kernel/test_mmap.c) escribe directamente en el puntero `shared_mem`. El driver del kernel lee inmediatamente esa modificación física en RAM sin ejecutar ninguna llamada `sys_write` ni `copy_from_user`.

---

### Paso 5: Suite de Benchmarks Comparativos y Análisis con `strace` (`paso5_benchmark_comparativo`)
- **Código:** [`paso5_benchmark_comparativo/benchmark.c`](file:///home/edi/DOCENCIA2026/SO2026B/CapaKernel/paso5_benchmark_comparativo/benchmark.c)
- **Script:** [`paso5_benchmark_comparativo/run_benchmark.sh`](file:///home/edi/DOCENCIA2026/SO2026B/CapaKernel/paso5_benchmark_comparativo/run_benchmark.sh)
- **Métricas empíricas obtenidas con un conjunto de datos de 64 MB:**

| Métrica | `read()` (4 KB) | `read()` (64 KB) | `mmap()` (Zero-Copy) |
| :--- | :---: | :---: | :---: |
| **Tiempo de CPU en Kernel (`sys_cpu`)** | 0.00692 s | 0.00486 s | **0.00002 s** (↓ 99.7%) |
| **Llamadas a `sys_read`** | 16,384 syscalls | 1,024 syscalls | **0 syscalls** |
| **Copia de datos en RAM** | Sí (`copy_to_user`) | Sí (`copy_to_user`) | **No (Zero-Copy)** |
| **Throughput relativo** | Base (1x) | 1x | **> 300x más rápido** |

#### Evidencia de `strace` (Análisis de llamadas al sistema):
- En **Paso 1 (`read`)**, el **99.07% del tiempo** del programa se consume en el kernel atendiendo llamadas `read`:
  ```
  % time     seconds  usecs/call     calls    errors syscall
   99,07    0,020311           2      8194           read
  ```
- En **Paso 2 (`mmap`)**, el tiempo en llamadas al sistema se reduce drásticamente porque **no hay lecturas por syscall**:
  ```
  % time     seconds  usecs/call     calls    errors syscall
   11,06    0,000073           8         9           mmap
    0,61    0,000004           4         1           read
  ```

---

### Paso 6: Paginación por Demanda Avanzada en Kernel (`paso6_demand_paging_fault`)
- **Módulo de Kernel:** [`paso6_demand_paging_fault/kmod_demand_paging.c`](file:///home/edi/DOCENCIA2026/SO2026B/CapaKernel/paso6_demand_paging_fault/kmod_demand_paging.c)
- **Cliente:** [`paso6_demand_paging_fault/test_demand_paging.c`](file:///home/edi/DOCENCIA2026/SO2026B/CapaKernel/paso6_demand_paging_fault/test_demand_paging.c)
- **Concepto clave:** En lugar de llamar a `remap_pfn_range()`, el driver asigna `vma->vm_ops = &demo_vm_ops;`. Cuando el proceso intenta acceder a una página que aún no está mapeada, la CPU genera la interrupción de hardware *Page Fault* (#PF, vector 14). El kernel ejecuta nuestro callback:
  ```c
  static vm_fault_t demo_vma_fault(struct vm_fault *vmf) {
      unsigned long offset = vmf->pgoff;
      struct page *page = driver_pages[offset];
      get_page(page);
      vmf->page = page;
      return 0;
  }
  ```
  Esto demuestra cómo los drivers modernos (como NVIDIA/AMD DRM o bases de datos) reservan y asignan memoria física **exclusivamente bajo demanda real**, logrando que páginas no accedidas consuman 0 bytes en las tablas de páginas.

---

### Paso 7: Comunicación Inter-Procesos (IPC) Zero-Copy (`paso7_ipc_ringbuffer`)
- **Archivos:** [`ring_buffer.h`](file:///home/edi/DOCENCIA2026/SO2026B/CapaKernel/paso7_ipc_ringbuffer/ring_buffer.h), [`producer.c`](file:///home/edi/DOCENCIA2026/SO2026B/CapaKernel/paso7_ipc_ringbuffer/producer.c), [`consumer.c`](file:///home/edi/DOCENCIA2026/SO2026B/CapaKernel/paso7_ipc_ringbuffer/consumer.c).
- **Concepto clave:** Implementa un buffer circular SPSC (*Single Producer Single Consumer*) en memoria compartida respaldada por `mmap`. 
- **Rendimiento:** Alcanza **> 13 millones de mensajes por segundo** entre procesos independientes con **cero llamadas al sistema** en el bucle principal de transmisión.
- **Ejecución:**
  ```bash
  make run-ipc
  ```

---

### Herramienta de Inspección de la MMU (`herramientas/vma_inspector.c`)
- **Código:** [`herramientas/vma_inspector.c`](file:///home/edi/DOCENCIA2026/SO2026B/CapaKernel/herramientas/vma_inspector.c)
- **Concepto clave:** Inspecciona `/proc/self/pagemap` para observar directamente el bit de presencia en RAM y el Page Frame Number (PFN) de las páginas virtuales antes y después de provocar un *Page Fault*.
- **Ejecución:**
  ```bash
  make run-inspector
  ```

---

## 5. Guía Rápida de Compilación y Ejecución

### 5.1 Menú Interactivo Maestro (`run.sh`)
El proyecto incluye un script maestro con interfaz interactiva por terminal o ejecución por argumentos:

```bash
# Lanzar el menú interactivo con todas las opciones
./run.sh

# O ejecutar directamente un paso específico:
./run.sh 1          # Paso 1: E/S clásica read()
./run.sh 2          # Paso 2: mmap en usuario
./run.sh 5          # Paso 5: Benchmarks y strace
./run.sh 7          # Paso 7: IPC Ring Buffer Zero-Copy
./run.sh inspector  # Inspección de MMU y pagemap
./run.sh --all      # Ejecución guiada completa
```

### 5.2 Compilar todo el proyecto manualmente
Desde la raíz del repositorio:
```bash
make all
```

### 5.2 Ejecutar las pruebas en espacio de usuario
```bash
# Probar E/S clásica con read()
make run-paso1

# Probar E/S con mmap() en espacio de usuario
make run-paso2

# Ejecutar el benchmark comparativo con reporte de strace
make run-benchmark
```

### 5.3 Probar los Drivers de la Capa Kernel (Requiere privilegios `sudo`)
Para cargar y probar los módulos de kernel:

```bash
# 1. Cargar ambos módulos y crear nodos /dev
sudo ./scripts/load_modules.sh

# 2. Probar el driver clásico con copy_to_user
./paso3_driver_copy_user/test_copy_user

# 3. Probar el driver optimizado con mmap (Zero-Copy)
./paso4_driver_mmap_kernel/test_mmap

# 4. Verificar los mensajes generados en el kernel
sudo dmesg | tail -n 15

# 5. Descargar los módulos al terminar
sudo ./scripts/unload_modules.sh
```

---

## 6. Conclusiones Pedagógicas para el Estudiante

1. `read()` y `write()` son interfaces seguras y universales, pero introducen una sobrecarga inevitable de **cambios de contexto** y **duplicación de memoria (`copy_to_user`)**.
2. `mmap()` delega la gestión de transferencia al hardware de paginación de la CPU (**MMU y TLB**), reduciendo la intervención del kernel a la creación del mapeo (`remap_pfn_range`) y resolución de fallos de página (*Page Faults*).
3. En la **Capa Kernel**, programar `.mmap` permite a los desarrolladores de controladores exponer regiones de hardware (MMIO) o buffers de DMA directamente al proceso de usuario, técnica indispensable en redes de alto rendimiento (DPDK), drivers de GPU, bases de datos en memoria y sistemas embebidos de tiempo real.
