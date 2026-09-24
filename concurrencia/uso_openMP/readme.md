# Guía Práctica de OpenMP en C

Este proyecto ilustra el uso básico e intermedio de OpenMP, una interfaz de programación de aplicaciones (API) que soporta la programación en paralelo de memoria compartida en C.

## Relación con los Segmentos de Memoria del Sistema

OpenMP trabaja bajo el paradigma de **memoria compartida**. Para entender cómo funcionan los hilos en OpenMP, es crucial relacionarlos con los segmentos de memoria clásicos de un proceso en C:

1.  **El Segmento HEAP (Montículo)**:
    *   **Contexto:** La memoria asignada dinámicamente con funciones como `malloc()` o `calloc()` reside en el Heap.
    *   **En OpenMP:** El Heap es **global y compartido por defecto**. Todos los hilos creados por OpenMP pueden acceder, leer y modificar los datos que residen en el Heap al mismo tiempo. En nuestro código, el enorme arreglo de 10 millones de elementos existe aquí, y todos los hilos acceden a él paralelamente para sumar sus partes.

2.  **El Segmento STACK (Pila)**:
    *   **Contexto:** Las variables locales y los parámetros de las funciones residen en la Pila.
    *   **En OpenMP:** **Cada hilo tiene su propio Stack (Pila) privado e independiente**. 
        * Cuando declaras una variable local *dentro* de un bloque `#pragma omp parallel` (como el iterador `int i` de un bucle `for`), la variable se aloja en el Stack privado de cada hilo.
        * Cuando usas la cláusula `private(...)` o `reduction(...)`, OpenMP "clona" la variable global temporalmente, asignando una copia privada en el Stack de cada hilo para evitar conflictos (condiciones de carrera) mientras trabajan.

3.  **El Segmento de Datos (Data / BSS)**:
    *   **Contexto:** Las variables globales y estáticas residen aquí.
    *   **En OpenMP:** Por defecto, estas variables son **compartidas** por todos los hilos. (Si se deseara hacerlas exclusivas por hilo, OpenMP dispone de la directiva `#pragma omp threadprivate`).


## Explicación del Código (`main.c`)

El código está estructurado para comparar la ejecución secuencial clásica con la ejecución paralela proporcionada por OpenMP, además de contar con una sección educativa.

### 1. Las Funciones de Cálculo
*   **`suma_secuencial`**: Itera a través de un arreglo enorme utilizando un solo hilo (ejecución clásica sin paralelismo). 
*   **`suma_concurrente`**: Utiliza `#pragma omp parallel for reduction(+:suma)`. 
    * **Visión de Memoria:** Esta directiva divide el bucle `for` equitativamente entre los hilos de tu procesador. Como el arreglo está en el **Heap**, los hilos lo leen compartidamente. La cláusula `reduction` es vital: le asigna a cada hilo una copia temporal en su propio **Stack** de la variable `suma`. Cada hilo suma su segmento de manera independiente y segura. Al finalizar, los resultados de los Stacks individuales se combinan en la variable final en el Stack del hilo principal.

### 2. Exploración de Directivas Adicionales
La función **`explorar_otras_directivas`** muestra otras capacidades clave:
*   Inicia una región paralela masiva. Se define `int variable_compartida = 0;` en el Stack del hilo maestro, que por el alcance de OpenMP se trata como variable COMPARTIDA en la región paralela. Si varios hilos intentan modificar esta variable compartida simultáneamente, ocurre una inconsistencia en la memoria (condición de carrera), y por ello es imperativo el uso de `#pragma omp critical`.

---

## Lista de Directivas y Cláusulas de OpenMP (Propósitos, Utilidad y Ejemplos)

A continuación, un resumen detallado de las directivas y cláusulas fundamentales en OpenMP. Se detalla su propósito teórico, situaciones de la vida real donde son útiles y ejemplos prácticos.

### Directivas de Ejecución Paralela y Repartición de Trabajo

*   **`#pragma omp parallel`**
    *   **Propósito:** Crear un "equipo" (team) de hilos y abrir una región paralela. Todos los hilos del equipo ejecutarán el bloque de código que sigue.
    *   **¿Para qué es útil?** Es el punto de partida fundamental de OpenMP. Se usa cuando quieres inicializar el paralelismo y mantener la misma cantidad de hilos a lo largo de un gran bloque de código, reduciendo la penalización (overhead) de crear y destruir hilos constantemente.
    ```c
    #pragma omp parallel
    {
        // Todos los hilos imprimen este mensaje
        printf("¡Hola desde el hilo %d!\n", omp_get_thread_num());
    }
    ```

*   **`#pragma omp for`**
    *   **Propósito:** Distribuir las iteraciones de un bucle `for` de manera equitativa entre los hilos del equipo *actual*. **Debe estar dentro de una región `parallel` previamente abierta.**
    *   **¿Para qué es útil?** Paralelismo de datos. Ideal para procesar grandes arreglos, matrices o imágenes donde el procesamiento de cada elemento (cada iteración) es completamente independiente del resto.
    ```c
    #pragma omp parallel
    {
        // ... otras tareas paralelas ...
        #pragma omp for
        for (int i = 0; i < 100; i++) {
            arreglo[i] = i * 2;
        }
    }
    ```

*   **`#pragma omp parallel for`**
    *   **Propósito:** Atajo que combina de forma directa `#pragma omp parallel` y `#pragma omp for`.
    *   **¿Para qué es útil?** Cuando tu único objetivo es paralelizar un bucle específico de forma rápida, sin necesidad de ejecutar código paralelo adicional antes o después del bucle.
    ```c
    #pragma omp parallel for
    for (int i = 0; i < 100; i++) {
        // Hace lo mismo que el ejemplo anterior, todo en uno
        arreglo[i] = i * 2;
    }
    ```

*   **`#pragma omp sections` y `#pragma omp section`**
    *   **Propósito:** Asignar diferentes bloques de código a distintos hilos. `sections` engloba el grupo, y cada `section` individual se ejecuta solo una vez por un único hilo.
    *   **¿Para qué es útil?** Paralelismo de tareas (Task parallelism). Perfecto para cuando tienes funciones pesadas pero completamente distintas que puedes ejecutar al mismo tiempo (ej. mientras un hilo descarga un archivo, otro hilo procesa la base de datos local).
    ```c
    #pragma omp parallel sections
    {
        #pragma omp section
        {
            // Tarea 1 ejecutada por el Hilo A
            procesar_audio();
        }
        #pragma omp section
        {
            // Tarea 2 ejecutada simultáneamente por el Hilo B
            procesar_video();
        }
    }
    ```

*   **`#pragma omp single`**
    *   **Propósito:** Asegurar que el bloque de código sea ejecutado por un único hilo (generalmente el primero que alcance el bloque).
    *   **¿Para qué es útil?** Tareas que deben hacerse exactamente una vez dentro de una región paralela masiva, como: alojar memoria dinámica, leer un archivo de configuración, o inicializar una estructura de datos compartida antes de que el resto del equipo trabaje con ella.
    ```c
    #pragma omp parallel
    {
        #pragma omp single
        {
            printf("Inicializando sistema... solo lo hace un hilo.\n");
            cargar_base_de_datos();
        } // Barrera implícita: Los demás hilos esperan aquí a que el hilo solitario termine.
        
        // Todos los hilos leen la base de datos aquí
        procesar_consultas();
    }
    ```

### Directivas de Sincronización y Exclusión

*   **`#pragma omp critical`**
    *   **Propósito:** Definir una sección crítica de exclusión mutua global. Solo permite el paso de un hilo a la vez en todo el programa.
    *   **¿Para qué es útil?** Prevenir condiciones de carrera. Esencial al modificar estructuras de datos compartidas complejas (como insertar un nodo en una lista enlazada compartida o escribir en un archivo log), donde si dos hilos intervienen al mismo tiempo, los datos se corromperían.
    ```c
    int contador_global = 0;
    #pragma omp parallel
    {
        int resultado_local = calcular_pesado();
        
        #pragma omp critical
        {
            // Evita condición de carrera, los hilos entran de a uno
            agregar_a_log("Calculado: %d", resultado_local);
            contador_global += resultado_local; 
        }
    }
    ```

*   **`#pragma omp atomic`**
    *   **Propósito:** Instruir al hardware para asegurar un acceso atómico (indivisible) estrictamente para operaciones matemáticas simples y de actualización de memoria (`++`, `--`, `+=`, `*=`, etc.).
    *   **¿Para qué es útil?** Como un `critical` súper rápido, optimizado a nivel de procesador. Útil para llevar un conteo global, actualizar flags o sumar estadísticas rápidamente sin bloquear todo el equipo por mucho tiempo.
    ```c
    int visitas = 0;
    #pragma omp parallel
    {
        // ... procesamiento ...
        #pragma omp atomic
        visitas++; // Mucho más rápido y escalable que critical
    }
    ```

*   **`#pragma omp barrier`**
    *   **Propósito:** Sincronización explícita. Actúa como un muro; obliga a que todos los hilos del equipo se detengan en este punto hasta que el último rezagado haya llegado.
    *   **¿Para qué es útil?** Para separar fases de un cálculo dependiente. Por ejemplo, si en la Fase 1 todos los hilos llenan una matriz de datos, la Fase 2 (leer la matriz) no puede empezar hasta estar 100% seguros de que la matriz está completamente llena.
    ```c
    #pragma omp parallel
    {
        // Fase 1: Cada hilo escribe en su porción de la matriz
        calcular_matriz();
        
        #pragma omp barrier // Nadie cruza hasta que la matriz esté completa
        
        // Fase 2: Todos leen de la matriz ya terminada
        analizar_matriz();
    }
    ```

*   **`#pragma omp master`**
    *   **Propósito:** Restringir la ejecución del bloque exclusivamente al "hilo maestro" (el hilo con ID 0).
    *   **¿Para qué es útil?** Imprimir mensajes de progreso en consola o manejar rutinas de administración donde no necesitas que el resto de los hilos se detengan a esperar (ya que `master` **no** tiene una barrera implícita al final, a diferencia de `single`).
    ```c
    #pragma omp parallel
    {
        hacer_trabajo_comun();
        
        #pragma omp master
        {
            printf("Progreso: 50%% completado.\n");
        } // Los demás hilos no esperan aquí, siguen trabajando
        
        hacer_mas_trabajo();
    }
    ```

### Cláusulas Comunes (Modificadores de las Directivas)

*   **`reduction(operador:variable)`**
    *   **Propósito:** Crea copias privadas de la variable para cada hilo en su Pila local. Al terminar el bloque, consolida de manera segura (y sin `critical`) todas las copias locales hacia la variable original mediante el operador (`+`, `*`, `max`, `min`, etc.).
    *   **¿Para qué es útil?** Esencial para sumatorias, productos acumulativos o buscar el número mayor/menor en un arreglo gigantesco de manera concurrente y sin penalización de rendimiento.
    ```c
    int suma_total = 0;
    #pragma omp parallel for reduction(+:suma_total)
    for (int i = 0; i < 1000; i++) {
        // Cada hilo suma a su propia copia, luego se juntan todas
        suma_total += arreglo[i];
    }
    ```

*   **`private(variable)`**
    *   **Propósito:** Indica explícitamente que cada hilo debe crear su propia versión nueva (y no inicializada) de la variable en su Stack, ocultando la variable global.
    *   **¿Para qué es útil?** Evitar que los hilos compartan variables temporales de paso (como índices secundarios o variables de retención intermedio) que podrían sobrescribirse mutuamente, causando resultados caóticos.
    ```c
    int temporal; // Declarada fuera, pero la queremos privada adentro
    #pragma omp parallel for private(temporal)
    for (int i = 0; i < 100; i++) {
        temporal = hacer_calculo(i); // Sin riesgo de que otro hilo modifique temporal
        arreglo[i] = temporal * 2;
    }
    ```

*   **`shared(variable)`**
    *   **Propósito:** Indica explícitamente que la variable mencionada es compartida y visible por todos los hilos, y que usarán la misma posición de memoria (usualmente en el Heap o Data segment).
    *   **¿Para qué es útil?** Aunque las variables externas son compartidas por defecto en la mayoría de los casos, declararlo hace el código más legible y estricto, ideal para grandes estructuras de datos de solo lectura o punteros base de matrices.
    ```c
    int multiplicador = 5; // Constante de solo lectura compartida
    #pragma omp parallel for shared(multiplicador)
    for (int i = 0; i < 100; i++) {
        arreglo[i] = i * multiplicador;
    }
    ```

*   **`nowait`**
    *   **Propósito:** Anular el comportamiento por defecto de OpenMP que obliga a los hilos a esperarse mutuamente al final de ciertas directivas (como `for`, `single` o `sections`).
    *   **¿Para qué es útil?** Para optimizar el rendimiento (reducir tiempos ociosos). Si un hilo termina su parte de un bucle rápidamente y el código siguiente no depende de que el bucle completo haya terminado, el hilo puede usar `nowait` para saltarse la espera y seguir trabajando inmediatamente en la siguiente tarea.
    ```c
    #pragma omp parallel
    {
        // Los hilos que terminen rápido la parte 1 no esperarán a los lentos
        #pragma omp for nowait
        for (int i = 0; i < 50; i++) {
            tarea_independiente_a(i);
        } 
        
        // Los rápidos empiezan esta tarea inmediatamente
        #pragma omp for
        for (int j = 0; j < 50; j++) {
            tarea_independiente_b(j);
        }
    }
    ```
