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

## Lista de Directivas y Cláusulas de OpenMP (con Ejemplos)

A continuación, un resumen de las directivas exploradas en el proyecto y otras fundamentales en OpenMP, acompañadas de ejemplos simples.

### Directivas de Ejecución Paralela y Repartición de Trabajo

*   **`#pragma omp parallel`**
    Crea un equipo de hilos y hace que el bloque de código que le sigue sea ejecutado simultáneamente por todos ellos.
    ```c
    #pragma omp parallel
    {
        // Todos los hilos imprimen este mensaje
        printf("¡Hola desde el hilo %d!\n", omp_get_thread_num());
    }
    ```

*   **`#pragma omp for`**
    Divide las iteraciones del bucle que le sigue entre los hilos del equipo actual. **Debe estar dentro de una región `parallel` previa**.
    ```c
    #pragma omp parallel
    {
        #pragma omp for
        for (int i = 0; i < 100; i++) {
            // El bucle se divide entre los hilos
            arreglo[i] = i * 2;
        }
    }
    ```

*   **`#pragma omp parallel for`**
    Un atajo directo que combina las dos directivas anteriores en una sola línea.
    ```c
    #pragma omp parallel for
    for (int i = 0; i < 100; i++) {
        // Hace lo mismo que el ejemplo anterior, todo en uno
        arreglo[i] = i * 2;
    }
    ```

*   **`#pragma omp sections` y `#pragma omp section`**
    `sections` engloba un conjunto de bloques de código. Cada `#pragma omp section` representa una tarea distinta, ejecutada una sola vez por un hilo diferente.
    ```c
    #pragma omp parallel sections
    {
        #pragma omp section
        {
            // Tarea 1 ejecutada por el Hilo A
            hacer_tarea_pesada_1();
        }
        #pragma omp section
        {
            // Tarea 2 ejecutada simultáneamente por el Hilo B
            hacer_tarea_pesada_2();
        }
    }
    ```

*   **`#pragma omp single`**
    Garantiza que el bloque asociado sea ejecutado por un único hilo del equipo (el primero en llegar). Útil para inicializaciones dentro de un bloque paralelo.
    ```c
    #pragma omp parallel
    {
        #pragma omp single
        {
            printf("Esto se imprime solo una vez, aunque haya muchos hilos.\n");
            inicializar_datos();
        } // Barrera implícita: Los demás hilos esperan aquí a que termine el single
        
        // Todos los hilos continúan el trabajo aquí
        procesar_datos();
    }
    ```

### Directivas de Sincronización y Exclusión

*   **`#pragma omp critical`**
    Define una sección crítica. Asegura exclusión mutua: solo un hilo a la vez accede al bloque.
    ```c
    int contador_global = 0;
    #pragma omp parallel
    {
        int resultado_local = calcular();
        
        #pragma omp critical
        {
            // Evita condición de carrera al sumar
            contador_global += resultado_local; 
        }
    }
    ```

*   **`#pragma omp atomic`**
    Similar a `critical` pero optimizado a nivel de hardware. Se usa exclusivamente para una sola operación de actualización en memoria.
    ```c
    int contador_global = 0;
    #pragma omp parallel
    {
        int resultado_local = calcular();
        
        #pragma omp atomic
        contador_global += resultado_local; // Más eficiente que critical para operaciones simples
    }
    ```

*   **`#pragma omp barrier`**
    Sincronización explícita. Obliga a que todos los hilos del equipo esperen en ese punto exacto hasta que todos hayan llegado.
    ```c
    #pragma omp parallel
    {
        fase_1();
        
        #pragma omp barrier // Ningún hilo empieza la fase 2 hasta que todos terminen la fase 1
        
        fase_2();
    }
    ```

*   **`#pragma omp master`**
    Especifica que el bloque de código solo debe ser ejecutado por el hilo maestro (el hilo con ID 0).
    ```c
    #pragma omp parallel
    {
        hacer_trabajo_comun();
        
        #pragma omp master
        {
            printf("El hilo maestro notifica que la primera parte concluyó.\n");
        } // A diferencia de 'single', aquí NO hay barrera de espera implícita
        
        hacer_mas_trabajo();
    }
    ```

### Cláusulas Comunes (se añaden a las directivas)

*   **`reduction(operador:variable)`**
    Crea copias locales para cada hilo y las combina al finalizar usando el operador indicado.
    ```c
    int suma = 0;
    // Cada hilo suma a su propia copia privada, y al final todas se suman al original
    #pragma omp parallel for reduction(+:suma)
    for (int i = 0; i < 100; i++) {
        suma += arreglo[i];
    }
    ```

*   **`private(variable)`**
    Declara que cada hilo tendrá su propia copia local e independiente (sin inicializar) de la variable.
    ```c
    int temporal;
    #pragma omp parallel for private(temporal)
    for (int i = 0; i < 100; i++) {
        // 'temporal' es independiente para cada hilo (su propio Stack)
        temporal = calcular_algo(i); 
        arreglo[i] = temporal * 2;
    }
    ```

*   **`shared(variable)`**
    Indica que la variable es compartida y visible por todos los hilos simultáneamente.
    ```c
    int factor = 5;
    // 'factor' es compartido por todos, 'i' es privado por defecto en el 'for'
    #pragma omp parallel for shared(factor)
    for (int i = 0; i < 100; i++) {
        arreglo[i] = i * factor;
    }
    ```

*   **`nowait`**
    Elimina la barrera de espera que OpenMP coloca al final de directivas como `for`, `single` o `sections`.
    ```c
    #pragma omp parallel
    {
        #pragma omp for nowait
        for (int i = 0; i < 50; i++) {
            tarea_a(i);
        } // Los hilos que terminen no esperarán a los demás
        
        // Empiezan esta tarea inmediatamente
        #pragma omp for
        for (int j = 0; j < 50; j++) {
            tarea_b(j);
        }
    }
    ```
