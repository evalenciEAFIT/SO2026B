#include "operaciones.h"

// --- REDUCCIÓN: Suma ---
long long op_sumar(const int* matriz, int inicio, int fin) {
    long long suma = 0;
    for (int i = inicio; i < fin; i++) {
        suma += matriz[i];
    }
    return suma;
}

// --- REDUCCIÓN: Encontrar el Máximo ---
int op_maximo(const int* matriz, int inicio, int fin) {
    if (inicio >= fin) return 0;
    int max = matriz[inicio];
    for (int i = inicio + 1; i < fin; i++) {
        if (matriz[i] > max) {
            max = matriz[i];
        }
    }
    return max;
}

// --- MAPEO: Escalar (Multiplicar cada elemento por un valor) ---
void op_escalar(int* matriz, int inicio, int fin, int factor) {
    for (int i = inicio; i < fin; i++) {
        matriz[i] *= factor;
    }
}

// --- TRANSFORMACIÓN: Rotar 90 grados a la derecha ---
void op_rotar(const int* origen, int* destino, int N, int fila_inicio, int fila_fin) {
    for (int i = fila_inicio; i < fila_fin; i++) {
        for (int j = 0; j < N; j++) {
            // Mapeo de coordenadas para rotar 90° a la derecha
            destino[j * N + (N - 1 - i)] = origen[i * N + j];
        }
    }
}

// --- PRODUCTO MATRICIAL: Multiplicación de Matrices NxN ---
void op_multiplicar(const int* A, const int* B, int* C, int N, int fila_inicio, int fila_fin) {
    for (int i = fila_inicio; i < fila_fin; i++) {
        for (int j = 0; j < N; j++) {
            int suma = 0;
            for (int k = 0; k < N; k++) {
                suma += A[i * N + k] * B[k * N + j];
            }
            C[i * N + j] = suma;
        }
    }
}
