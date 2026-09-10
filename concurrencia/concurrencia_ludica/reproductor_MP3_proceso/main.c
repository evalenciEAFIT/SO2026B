#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/wait.h>

// =========================================================================
// Este programa actúa como un "Orquestador de Procesos".
// =========================================================================

void mostrar_ayuda_main(const char *nombre_programa) {
    printf("\n\033[1;32mUso del Orquestador de Procesos (Main):\033[0m\n");
    printf("\n\033[1;36mOpción 1 (Básico):\033[0m\n");
    printf("  %s [-c <cancion.mp3>] [-h]\n", nombre_programa);
    
    printf("\n\033[1;36mOpción 2 (Avanzado / Múltiples Procesos Personalizados):\033[0m\n");
    printf("  %s P1{-c: \"cancion.mp3\", -n: \"Hijo 1\", -v: 0.5} P2{-c: \"cancion2.mp3\", -v: 1.0, -d: \"inverso\"}\n", nombre_programa);
    
    printf("\n\033[1;33mDescripción Dinámica:\033[0m\n");
    printf("  Puedes especificar tantos procesos como quieras usando la sintaxis Px{...}.\n");
    printf("  Los argumentos dentro de las llaves deben estar separados por comas (,)\n");
    printf("  y usar dos puntos (:) para separar la bandera de su valor.\n\n");
}

void trim_and_remove_quotes(char *str) {
    char *p = str;
    int l = strlen(p);
    while(l > 0 && isspace(p[l - 1])) p[--l] = 0;
    while(*p && isspace(*p)) ++p, --l;
    
    if (p[0] == '"' || p[0] == '\'') {
        p++; l--;
        if (l > 0 && (p[l-1] == '"' || p[l-1] == '\'')) {
            p[l-1] = 0;
        }
    }
    memmove(str, p, l + 1);
}

// =========================================================================
// FUNCIÓN: Monitoreo de Procesos (Waitpid Loop)
// =========================================================================
void monitorear_procesos(int cantidad_esperada) {
    int procesos_activos = cantidad_esperada;
    int status;
    
    while (procesos_activos > 0) {
        // ---> SYSTEM CALL: waitpid() <---
        pid_t pid_terminado = waitpid(-1, &status, WUNTRACED | WCONTINUED);
        if (pid_terminado == -1) break;
        
        if (WIFEXITED(status)) {
            // Finalizó por su propia cuenta (llegó al final de la canción)
            printf("\n\033[1;33m[Supervisor %d] El proceso hijo %d finalizó con código %d.\033[0m\n", getpid(), pid_terminado, WEXITSTATUS(status));
            procesos_activos--;
        } else if (WIFSIGNALED(status)) {
            // Fue asesinado (ej. Ctrl+C)
            printf("\n\033[1;31m[Supervisor %d] El proceso hijo %d fue asesinado por señal %d.\033[0m\n", getpid(), pid_terminado, WTERMSIG(status));
            procesos_activos--;
        } else if (WIFSTOPPED(status)) {
            printf("\n\033[1;34m[Supervisor %d] El proceso hijo %d fue pausado (señal %d).\033[0m\n", getpid(), pid_terminado, WSTOPSIG(status));
        } else if (WIFCONTINUED(status)) {
            printf("\n\033[1;32m[Supervisor %d] El proceso hijo %d fue reanudado.\033[0m\n", getpid(), pid_terminado);
        }
    }
}

int main(int argc, char *argv[]) {
    // Si piden ayuda directamente
    if (argc == 2 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--ayuda") == 0)) {
        mostrar_ayuda_main(argv[0]);
        return 0;
    }

    // 1. Unificar todos los argumentos en una sola cadena para facilitar el parseo de P1{...} P2{...}
    char full_input[4096] = "";
    for (int i = 1; i < argc; i++) {
        strcat(full_input, argv[i]);
        strcat(full_input, " ");
    }

    // Detectamos si el usuario está usando el modo avanzado (presencia de llaves {})
    if (strchr(full_input, '{') != NULL && strchr(full_input, '}') != NULL) {
        
        printf("=========================================================================\n");
        printf("\033[1;35m[Main Process %d] MODO ORQUESTADOR DINÁMICO ACTIVADO\033[0m\n", getpid());
        printf("=========================================================================\n");
        
        // Primera pasada: Contar cuántos procesos vamos a lanzar para calcular los offsets
        int total_procesos = 0;
        char *ptr = full_input;
        while ((ptr = strchr(ptr, '{')) != NULL) {
            total_procesos++;
            ptr++;
        }
        
        printf("[Main Process %d] Se detectaron %d procesos a instanciar concurrentemente.\n", getpid(), total_procesos);
        
        // Hacemos espacio visual
        for(int i=0; i<total_procesos; i++) printf("\n");
        
        int offset_actual = total_procesos;
        int procesos_lanzados = 0;
        
        // Segunda pasada: Extraer configuraciones y lanzar
        char *start = full_input;
        while ((start = strchr(start, '{')) != NULL) {
            start++; 
            char *end = strchr(start, '}');
            if (!end) break;
            
            // Extraer el bloque interno
            int len = end - start;
            char content[2048];
            strncpy(content, start, len);
            content[len] = '\0';
            
            // Arreglo para pasar a execvp
            char *exec_args[32];
            int arg_count = 0;
            exec_args[arg_count++] = "./reproductor_mp3";
            
            // Asignamos el offset de consola
            char offset_str[16];
            snprintf(offset_str, sizeof(offset_str), "%d", offset_actual);
            exec_args[arg_count++] = "-o";
            exec_args[arg_count++] = strdup(offset_str);
            
            // Tokenizar pares "clave: valor" separados por coma
            char *pair = strtok(content, ",");
            while (pair != NULL) {
                char *colon = strchr(pair, ':');
                if (colon) {
                    *colon = '\0'; // Dividir cadena
                    char flag[256];
                    char val[256];
                    strcpy(flag, pair);
                    strcpy(val, colon + 1);
                    
                    trim_and_remove_quotes(flag);
                    trim_and_remove_quotes(val);
                    
                    exec_args[arg_count++] = strdup(flag);
                    if (strlen(val) > 0) {
                        exec_args[arg_count++] = strdup(val);
                    }
                } else {
                    // Soporte para banderas sin valor (ej. -q, --hide-cpu)
                    char flag[256];
                    strcpy(flag, pair);
                    trim_and_remove_quotes(flag);
                    if (strlen(flag) > 0) {
                        exec_args[arg_count++] = strdup(flag);
                    }
                }
                pair = strtok(NULL, ",");
            }
            exec_args[arg_count] = NULL;
            
            // Crear el hijo
            printf("\033[s\033[%dA[Main Process %d] Lanzando proceso hijo...\033[K\033[u", total_procesos + 2, getpid());
            pid_t pid = fork();
            if (pid == 0) {
                execvp(exec_args[0], exec_args);
                perror("Error en execvp");
                exit(EXIT_FAILURE);
            }
            
            procesos_lanzados++;
            offset_actual--;
            start = end + 1;
        }
        
        printf("\033[s\033[%dA\033[K[Main Process %d] %d procesos ejecutándose.\n[Main Process %d] Entrando en modo supervisor...\033[u", total_procesos + 2, getpid(), procesos_lanzados, getpid());
        
        // Delegamos a la función de manejo de procesos
        monitorear_procesos(procesos_lanzados);
        
        printf("\n\n[Main Process %d] Todos los procesos dinámicos finalizaron.\n", getpid());
        return 0;
    }

    // =========================================================================
    // MODO ESTÁNDAR (Comportamiento por defecto con 2 hijos fijos)
    // =========================================================================
    char *mp3_file = "../music/pachelbel_canon.mp3"; 
    
    // Parseo simple para compatibilidad hacia atrás
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            mp3_file = argv[i+1];
        } else if (argv[i][0] != '-') {
            mp3_file = argv[i];
        }
    }

    printf("=========================================================================\n");
    printf("\033[1;32mTIP:\033[0m Puedes ejecutar \033[1;36m./main -h\033[0m para ver la ayuda y el nuevo modo avanzado.\n\n");
    printf("\033[1;36m[Main Process %d] ARCHIVO OBJETIVO:\033[0m %s\n", getpid(), mp3_file);
    printf("\033[1;35m[Main Process %d] CONFIGURACIÓN DE PROCESOS:\033[0m\n", getpid());
    printf(" ├── \033[1;32mHijo 1 (Monitor Superior):\033[0m Velocidad: x1.0 | Volumen: x1.0 | Sentido: Directo\n");
    printf(" └── \033[1;36mHijo 2 (Monitor Inferior) :\033[0m Velocidad: x2.0 | Volumen: x0.5 | Sentido: Inverso\n\n");
    
    printf("\n\n"); 

    pid_t pid1 = fork();
    if (pid1 == 0) {
        char *args[] = {"./reproductor_mp3", "-c", mp3_file, "-s", "1.0", "-v", "1.0", "-d", "directo", "-n", "Hijo_1", "-C", "32", "-o", "2", NULL};
        execvp(args[0], args);
        perror("Error"); exit(1);
    }

    pid_t pid2 = fork();
    if (pid2 == 0) {
        char *args[] = {"./reproductor_mp3", "-c", mp3_file, "-s", "2.0", "-v", "0.5", "-d", "inverso", "-n", "Hijo_2", "-C", "36", "-o", "1", NULL};
        execvp(args[0], args);
        perror("Error"); exit(1);
    }

    printf("\033[s\033[3A\033[K[Main Process %d] Mis dos hijos (PIDs: %d y %d) están corriendo concurrentemente.\n[Main Process %d] Entrando en modo supervisor para monitorear sus estados...\033[u", getpid(), pid1, pid2, getpid());

    monitorear_procesos(2);

    printf("\n[Main Process %d] Todos los procesos hijos han sido recolectados. Fin del programa.\n", getpid());
    return 0;
}
