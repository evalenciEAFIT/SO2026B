#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include "matrix.h"
#include "ipc.h"

int main() {
    int matriz[FILAS][COLUMNAS] = {
        {1, 2},
        {3, 4}
    };
    
    int fd_tuberia[2];
    
    if (pipe(fd_tuberia) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }
    
    printf("--- Estado Inicial ---\n");
    imprimir_matriz("Matriz Original", matriz);
    
    pid_t pid = fork();
    
    if (pid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    }
    
    if (pid == 0) {
        // --- PROCESO HIJO ---
        close(fd_tuberia[0]); 
        
        printf("[Hijo] Mi PID es %d. Modificando matriz de forma funcional...\n", getpid());
        
        int nueva_matriz[FILAS][COLUMNAS];
        // Programación estilo funcional: no modificamos la original, creamos una nueva a partir de ella
        escalar_matriz(matriz, nueva_matriz, 10);
        
        imprimir_matriz("[Hijo] Matriz modificada", nueva_matriz);
        
        enviar_mensaje(fd_tuberia[1], "Hola Padre, he terminado mi trabajo con la matriz.");
        
        int suma = sumar_matriz(nueva_matriz);
        enviar_dato(fd_tuberia[1], suma);
        
        close(fd_tuberia[1]);
        printf("[Hijo] Terminando.\n\n");
        exit(EXIT_SUCCESS);
        
    } else {
        // --- PROCESO PADRE ---
        close(fd_tuberia[1]); 
        
        printf("[Padre] Mi PID es %d. Esperando al hijo (PID: %d)...\n", getpid(), pid);
        
        // Modificación local en el padre para demostrar separación de memoria
        matriz[0][0] = -1;
        
        wait(NULL);
        
        printf("[Padre] El hijo ha terminado. Revisando mi matriz:\n");
        imprimir_matriz("[Padre] Matriz del padre", matriz); 
        
        char bufer[TAMANO_MENSAJE];
        recibir_mensaje(fd_tuberia[0], bufer);
        printf("[Padre] Mensaje recibido del hijo: '%s'\n", bufer);
        
        int suma_hijo = recibir_dato(fd_tuberia[0]);
        printf("[Padre] Suma calculada por el hijo recibida: %d\n", suma_hijo);
        
        close(fd_tuberia[0]);
        printf("[Padre] Terminando.\n");
    }
    
    return 0;
}
