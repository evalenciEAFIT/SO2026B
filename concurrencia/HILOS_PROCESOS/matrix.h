#ifndef MATRIZ_H
#define MATRIZ_H

// Funciones para matrices dinámicas unidimensionales (simulando 2D)
int* crear_matriz(int tamano);
void liberar_matriz(int* matriz);
void inicializar_matriz(int* matriz, int tamano);
void imprimir_fragmento(const int* matriz, int filas, int columnas);

#endif
