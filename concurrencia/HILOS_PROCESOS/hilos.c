#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include "matrix.h"

// Estructura para compartir datos entre hilos (simulando paso de mensajes/datos por memoria compartida)
typedef struct {
    int matriz[FILAS][COLUMNAS]; // Matriz original compartida
    int nueva_matriz[FILAS][COLUMNAS];
    int suma;
    char mensaje[100];
} MemoriaCompartida;

void* trabajo_hilo(void* arg) {
    MemoriaCompartida* mem = (MemoriaCompartida*)arg;
    
    printf("[Hilo Secundario] Ejecutando... Modificando matriz compartida de forma funcional.\n");
    
    // El hilo tiene acceso directo a la memoria del proceso principal a través del puntero
    escalar_matriz(mem->matriz, mem->nueva_matriz, 10);
    
    // En lugar de usar pipes (IPC), simplemente escribimos en la memoria compartida
    snprintf(mem->mensaje, sizeof(mem->mensaje), "Hola Hilo Principal, he terminado mi trabajo con la matriz.");
    mem->suma = sumar_matriz(mem->nueva_matriz);
    
    printf("[Hilo Secundario] Terminando.\n\n");
    pthread_exit(NULL);
}

int main() {
    MemoriaCompartida mem = {
        .matriz = {
            {1, 2},
            {3, 4}
        },
        .suma = 0
    };
    
    printf("--- Estado Inicial ---\n");
    imprimir_matriz("Matriz Compartida", mem.matriz);
    
    pthread_t hilo;
    
    // Creamos el hilo y le pasamos el puntero a nuestra estructura de memoria
    if (pthread_create(&hilo, NULL, trabajo_hilo, &mem) != 0) {
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }
    
    printf("[Hilo Principal] Esperando al hilo secundario...\n");
    
    // ATENCIÓN: Si modificamos la matriz aquí, podríamos afectar lo que ve el hilo
    // de forma concurrente, porque comparten el MISMO espacio de memoria.
    // Esto es distinto a los procesos, donde la memoria se copia y se aísla (fork).
    
    // Esperamos a que el hilo termine para evitar condiciones de carrera al leer
    pthread_join(hilo, NULL);
    
    printf("[Hilo Principal] El hilo secundario ha terminado. Revisando memoria compartida:\n");
    
    // Leemos directamente desde la estructura, sin necesidad de funciones IPC (pipes)
    imprimir_matriz("[Hilo Principal] Matriz Original", mem.matriz); 
    imprimir_matriz("[Hilo Principal] Matriz Resultado", mem.nueva_matriz); 
    
    printf("[Hilo Principal] Mensaje leído de memoria: '%s'\n", mem.mensaje);
    printf("[Hilo Principal] Suma leída de memoria: %d\n", mem.suma);
    
    printf("[Hilo Principal] Terminando.\n");
    
    return 0;
}
