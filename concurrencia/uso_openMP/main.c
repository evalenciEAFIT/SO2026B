#include <stdio.h>
#include <stdlib.h>
#include <omp.h>

#define TAMANO_ARREGLO 10000000

// Códigos de color ANSI para mejorar la estética en la terminal
#define COLOR_RESET   "\x1b[0m"
#define COLOR_NEGRITA "\x1b[1m"
#define COLOR_ROJO    "\x1b[31m"
#define COLOR_VERDE   "\x1b[32m"
#define COLOR_AMARILLO "\x1b[33m"
#define COLOR_AZUL    "\x1b[34m"
#define COLOR_CIAN    "\x1b[36m"
#define COLOR_MAGENTA "\x1b[35m"

/*
 * ============================================================================
 * Función: suma_secuencial
 * ----------------------------------------------------------------------------
 * Realiza la suma de los elementos de un arreglo usando un solo hilo 
 * (ejecución clásica sin paralelismo).
 * ============================================================================
 */
long long suma_secuencial(long long *arreglo, int tamano) {
    long long suma = 0;
    for (int i = 0; i < tamano; i++) {
        suma += arreglo[i];
    }
    return suma;
}

/*
 * ============================================================================
 * Función: suma_concurrente
 * ----------------------------------------------------------------------------
 * Demuestra el uso de la directiva: #pragma omp parallel for reduction
 * 
 * Relación con los Segmentos de Memoria:
 * - Parámetro '*arreglo': Apunta a un bloque de memoria en el HEAP (Montículo). 
 *   El Heap es una región COMPARTIDA por todos los hilos creados en el proceso.
 * - Variable 'suma' original: Reside en el STACK (Pila) del hilo principal.
 * - 'reduction(+:suma)': Crea una copia temporal privada de 'suma' para 
 *   cada hilo en su propio STACK. Cada hilo trabaja en su pila de forma segura. 
 *   Al final, las copias de los stacks locales se combinan en el stack principal.
 * - Iterador 'i': Al declararse dentro del bucle de OpenMP, reside de forma 
 *   privada en el STACK de cada hilo.
 * ============================================================================
 */
long long suma_concurrente(long long *arreglo, int tamano) {
    long long suma = 0;
    
    #pragma omp parallel for reduction(+:suma)
    for (int i = 0; i < tamano; i++) {
        suma += arreglo[i];
    }
    return suma;
}

/*
 * ============================================================================
 * Función: explorar_otras_directivas
 * ----------------------------------------------------------------------------
 * Relación con Segmentos de Memoria:
 * - 'variable_compartida' reside en el STACK del hilo que llama a la función,
 *   pero al entrar al bloque '#pragma omp parallel', se trata como COMPARTIDA 
 *   entre los hilos. Si todos intentan escribirla a la vez (por ejemplo, en el Heap
 *   o Data segment sin control), ocurre una condición de carrera. 
 *   Por eso usamos '#pragma omp critical'.
 * ============================================================================
 */
void explorar_otras_directivas() {
    printf(COLOR_MAGENTA COLOR_NEGRITA "\n--- Explorando otras Directivas OpenMP ---" COLOR_RESET "\n");
    
    int variable_compartida = 0; // Stack del hilo principal, pero compartida por defecto.

    #pragma omp parallel
    {
        #pragma omp single
        {
            printf("Directiva SINGLE: Este mensaje es impreso por un solo hilo (ID del hilo: %d).\n", omp_get_thread_num());
        } 

        #pragma omp sections
        {
            #pragma omp section
            {
                printf("Directiva SECTION 1: Tarea A ejecutada por el hilo %d\n", omp_get_thread_num());
            }
            #pragma omp section
            {
                printf("Directiva SECTION 2: Tarea B ejecutada por el hilo %d\n", omp_get_thread_num());
            }
        } 

        // Directiva CRITICAL:
        // Protege 'variable_compartida' para que los hilos no sobrescriban la memoria al mismo tiempo.
        #pragma omp critical
        {
            variable_compartida++;
        }
    } 

    printf("Resultado final variable protegida por CRITICAL: %d (Debe coincidir con el número total de hilos disponibles)\n", variable_compartida);
    printf(COLOR_MAGENTA "------------------------------------------" COLOR_RESET "\n\n");
}


int main() {
    // Relación de Memoria:
    // 'malloc' solicita memoria dinámicamente en el segmento HEAP (Montículo).
    // Toda la memoria del Heap es visible y COMPARTIDA por todos los hilos del proceso.
    long long *arreglo = (long long *)malloc(TAMANO_ARREGLO * sizeof(long long));
    
    if (arreglo == NULL) {
        fprintf(stderr, COLOR_ROJO "Fallo en la asignación de memoria" COLOR_RESET "\n");
        return 1;
    }

    // Inicializar el arreglo en el Heap
    for (int i = 0; i < TAMANO_ARREGLO; i++) {
        arreglo[i] = i + 1;
    }

    int numero_hilos = omp_get_max_threads();
    printf(COLOR_CIAN COLOR_NEGRITA "🚀 Iniciando Proyecto OpenMP 🚀" COLOR_RESET "\n");
    printf(COLOR_CIAN "Usando %d hilos para la ejecución concurrente." COLOR_RESET "\n", numero_hilos);

    explorar_otras_directivas();

    // --- Ejecución Secuencial ---
    double tiempo_inicio_sec = omp_get_wtime();
    long long resultado_sec = suma_secuencial(arreglo, TAMANO_ARREGLO);
    double tiempo_fin_sec = omp_get_wtime();
    double tiempo_secuencial = tiempo_fin_sec - tiempo_inicio_sec;

    printf(COLOR_AMARILLO COLOR_NEGRITA "--- Ejecución Secuencial ---" COLOR_RESET "\n");
    printf("Suma total:   " COLOR_AMARILLO "%lld" COLOR_RESET "\n", resultado_sec);
    printf("Tiempo tomado: " COLOR_AMARILLO "%f segundos" COLOR_RESET "\n\n", tiempo_secuencial);

    // --- Ejecución Concurrente ---
    double tiempo_inicio_conc = omp_get_wtime();
    long long resultado_conc = suma_concurrente(arreglo, TAMANO_ARREGLO);
    double tiempo_fin_conc = omp_get_wtime();
    double tiempo_concurrente = tiempo_fin_conc - tiempo_inicio_conc;

    printf(COLOR_AZUL COLOR_NEGRITA "--- Ejecución Concurrente ---" COLOR_RESET "\n");
    printf("Suma total:   " COLOR_AZUL "%lld" COLOR_RESET "\n", resultado_conc);
    printf("Tiempo tomado: " COLOR_AZUL "%f segundos" COLOR_RESET "\n\n", tiempo_concurrente);
    
    // --- Comparación ---
    printf(COLOR_VERDE COLOR_NEGRITA "--- Comparación ---" COLOR_RESET "\n");
    if (tiempo_concurrente > 0) {
        double speedup = tiempo_secuencial / tiempo_concurrente;
        printf("Aceleración (Speedup): ");
        if (speedup > 1.0) {
             printf(COLOR_VERDE COLOR_NEGRITA "%.2fx más rápido ⚡" COLOR_RESET "\n", speedup);
        } else {
             printf(COLOR_ROJO COLOR_NEGRITA "%.2fx (más lento) 🐢" COLOR_RESET "\n", speedup);
        }
    }

    free(arreglo);
    return 0;
}
