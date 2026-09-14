#ifndef OPERACIONES_H
#define OPERACIONES_H

// Operaciones Unidimensionales (Arreglos planos)
long long op_sumar(const int* matriz, int inicio, int fin);
int op_maximo(const int* matriz, int inicio, int fin);
void op_escalar(int* matriz, int inicio, int fin, int factor);

// Operaciones Bidimensionales (Matrices N x N representadas en 1D)
void op_rotar(const int* origen, int* destino, int N, int fila_inicio, int fila_fin);
void op_multiplicar(const int* A, const int* B, int* C, int N, int fila_inicio, int fila_fin);

#endif
