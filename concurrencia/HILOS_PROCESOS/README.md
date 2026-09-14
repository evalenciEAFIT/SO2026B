1

# Concurrencia en C: Procesos vs Hilos

Este proyecto es un ejemplo didáctico en C para comparar cómo funcionan los **procesos** (aislamiento de memoria e IPC) frente a los **hilos** (memoria compartida).

## Archivos Principales

1. **`procesos.c` (Uso de `fork`)**:

   * **Memoria:** Al usar `fork()`, el proceso hijo hereda una copia de la memoria del padre. A partir de ahí, **sus memorias son independientes**. Si el padre modifica su matriz, no afecta al hijo, y viceversa.
   * **Comunicación:** Para compartir resultados, deben utilizar mecanismos de comunicación entre procesos (IPC), en este caso tuberías (`pipes`).
2. **`hilos.c` (Uso de `pthread`)**:

   * **Memoria:** Los hilos creados dentro de un mismo proceso **comparten el mismo espacio de memoria**.
   * **Comunicación:** No necesitan tuberías ni otros IPC complejos para pasar mensajes. Pueden escribir directamente en variables, estructuras o arreglos globales o pasados por referencia (punteros), y los otros hilos podrán leer esos cambios inmediatamente. (En escenarios más complejos, esto requiere de mecanismos de sincronización como *mutex* para evitar condiciones de carrera).

## Modularización y Programación Estilo Funcional

Las operaciones con matrices se separaron en los archivos `matrix.h` y `matrix.c`. Estas funciones están diseñadas de forma más "pura", es decir, reciben la matriz original como sólo lectura (`const`) y guardan el resultado en una nueva matriz, evitando efectos secundarios destructivos en la matriz de origen.

La lógica de envío de mensajes usando *pipes* está separada en `ipc.h` y `ipc.c`. Note que el programa `hilos.c` no requiere incluirlos, ya que la comunicación se da directamente leyendo las variables de la estructura compartida en memoria.

## Compilación y Ejecución

Se incluye un `Makefile` para compilar todo.

Para compilar ambos programas:

```bash
make
```

Para ejecutar el programa con procesos:

```bash
./procesos
```

Para ejecutar el programa con hilos:

```bash
./hilos
```
