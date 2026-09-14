#include "matrix.h"
#include <stdio.h>  // PARA QUÉ: printf(). POR QUÉ: Vital para imprimir_fragmento() mostrando la validación visual de la matriz en pantalla.
#include <stdlib.h> // PARA QUÉ: malloc(), rand(), srand(). POR QUÉ: Permite pedir bloques de memoria (matrices) al SO e inyectar valores matemáticos aleatorios (Data Dummy).
#include <time.h>   // PARA QUÉ: time(NULL). POR QUÉ: Alimenta la "semilla" del generador de números al azar, garantizando que cada ejecución cree escenarios distintos.

int* crear_matriz(int tamano) {
    int* matriz = (int*)malloc(tamano * sizeof(int));
    if (!matriz) {
        perror("Error al asignar memoria para la matriz");
        exit(EXIT_FAILURE);
    }
    return matriz;
}

void liberar_matriz(int* matriz) {
    if (matriz) {
        free(matriz);
    }
}

void inicializar_matriz(int* matriz, int tamano) {
    for (int i = 0; i < tamano; i++) {
        matriz[i] = rand() % 10; // Números aleatorios del 0 al 9
    }
}

// Funciones matemáticas movidas a operaciones.c

void imprimir_fragmento(const int* matriz, int filas, int columnas) {
    int max_f = (filas > 5) ? 5 : filas;
    int max_c = (columnas > 5) ? 5 : columnas;
    
    printf("\n  Vista previa de la matriz (mostrando max 5x5):\n");
    for (int i = 0; i < max_f; i++) {
        printf("    ");
        for (int j = 0; j < max_c; j++) {
            printf("%d\t", matriz[i * columnas + j]);
        }
        if (columnas > 5) printf("...\n"); else printf("\n");
    }
    if (filas > 5) printf("    ...\n");
}
