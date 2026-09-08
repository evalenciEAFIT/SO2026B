#include <stdio.h>    // Provee funciones de Entrada/Salida estándar como printf()
#include <stdlib.h>   // Provee funciones como atoi() para conversiones, rand() y srand() para números aleatorios
#include <string.h>   // Provee funciones para manipular cadenas de texto como strcmp() para comparar argumentos
#include <unistd.h>   // Provee acceso a la API del sistema operativo POSIX, incluye sleep() para pausar la ejecución
#include <time.h>     // Provee funciones para trabajar con el reloj del sistema, como time() para la semilla aleatoria

/* ============================================================================
 * SIMULADOR DE PLANIFICADOR DE PROCESOS (PROCESS SCHEDULER)
 * Algoritmos soportados: RR, FCFS, SJF
 * ============================================================================
 */

#define QUANTUM 2
#define MAX_TICKS 100
#define MAX_OPS 15

// Códigos de color ANSI
#define COLOR_ESPERANDO       "\x1b[94m"   
#define COLOR_EJECUTANDO_HIST "\x1b[92m"   
#define COLOR_EJECUTANDO_ACT  "\x1b[92;5m" 
#define COLOR_DURMIENDO       "\x1b[91m"   
#define COLOR_TERMINADO       "\x1b[90m"   
#define COLOR_RESET           "\x1b[0m"    
#define COLOR_BOLD            "\x1b[1m"    

typedef enum { ESPERANDO, EJECUTANDO, DURMIENDO, TERMINADO } Estado;
typedef enum { RR, FCFS, SJF } Algoritmo;

// Estructura que simula el Bloque de Control de Proceso (PCB - Process Control Block)
// Contiene toda la información y estado ("contexto") de un proceso individual en el sistema.
typedef struct {
    int id;                 // Identificador único del proceso (equivalente al PID real)
    Estado estado;          // Estado actual en el ciclo de vida (ESPERANDO, EJECUTANDO, DURMIENDO, TERMINADO)
    
    int tiempo_total_cpu;   // Ráfaga de CPU total requerida por el proceso para terminar su trabajo
    int tiempo_restante_cpu;// Cuántos ticks de CPU le faltan al proceso para finalizar
    
    int tiempo_restante_dormir; // Ticks restantes que el proceso permanecerá en E/S (estado DURMIENDO)
    int duracion_sueno;         // Cuánto tiempo en total durará la operación de E/S actual
    int tick_para_dormir;       // En qué momento (tiempo restante de CPU) el proceso hará una solicitud de E/S
    
    int tiempo_en_cpu_actual;   // Cuánto tiempo ininterrumpido lleva usando la CPU en su turno actual (útil para el Quantum en RR)
    int tick_entrada_listo;     // Marca de tiempo indicando cuándo ingresó por última vez a la cola de ESPERANDO
    int cpu_actual;             // El ID de la CPU que está ejecutando este proceso actualmente (si aplica)
    
    Estado historia[MAX_TICKS];       // Arreglo para registrar el historial de estados y dibujar el gráfico final
    const char* operaciones[MAX_OPS]; // Arreglo de strings simulando las operaciones/instrucciones del código del proceso
    const char* op_io;                // Descripción de la operación de Entrada/Salida que está bloqueando al proceso
} Proceso;

const char* POOL_OPERACIONES[] = {
    "Cargar_Memoria", "Validar_Datos", "Multiplicar_Matriz", 
    "Comprimir_Fichero", "Desencriptar", "Calcular_Hash", 
    "Procesar_Cola", "Renderizar_UI", "Parsear_JSON", "Liberar_Recursos"
};

const char* POOL_IO[] = {
    "Esperando_Respuesta_BD", "Cargando_Frames_Disco", 
    "Peticion_HTTP_API", "Esperando_Input_Usuario", "Lectura_Sensor"
};

// Función auxiliar para limpiar la consola usando secuencias de escape ANSI
void limpiar_pantalla() {
    printf("\033[H\033[J");
}

// Función para imprimir las instrucciones y uso del simulador
void imprimir_ayuda(const char* nombre_prog) {
    printf("%s=== SIMULADOR DE PLANIFICADOR DE PROCESOS ===%s\n\n", COLOR_BOLD, COLOR_RESET);
    printf("Uso: %s [numero_procesos] [ALGORITMO] [numero_cpus]\n\n", nombre_prog);
    printf("%sOpciones:%s\n", COLOR_BOLD, COLOR_RESET);
    printf("  numero_procesos : Cantidad de procesos a simular (1-20). Por defecto: 4\n");
    printf("  ALGORITMO       : Algoritmo de planificación a usar. Por defecto: RR\n");
    printf("  numero_cpus     : Cantidad de CPUs. Por defecto: 1\n\n");
    printf("%sAlgoritmos disponibles:%s\n", COLOR_BOLD, COLOR_RESET);
    printf("  %sRR%s   : Round Robin (Quantum = %d). Expropiativo, equitativo. Asigna CPU por turnos circulares.\n", COLOR_BOLD, COLOR_RESET, QUANTUM);
    printf("  %sFCFS%s : First-Come, First-Served. No expropiativo. El primero que llega se ejecuta hasta soltar la CPU.\n", COLOR_BOLD, COLOR_RESET);
    printf("  %sSJF%s  : Shortest Job First. No expropiativo. Prioriza siempre al proceso con la ráfaga de CPU más corta.\n\n", COLOR_BOLD, COLOR_RESET);
    printf("%sEjemplos de uso:%s\n", COLOR_BOLD, COLOR_RESET);
    printf("  %s 5 FCFS       (5 procesos, FCFS, 1 CPU)\n", nombre_prog);
    printf("  %s 3 SJF 2      (3 procesos, SJF, 2 CPUs)\n", nombre_prog);
    printf("  %s 6 RR 4       (6 procesos, Round Robin, 4 CPUs)\n", nombre_prog);
}

// Función principal de renderizado: Dibuja la línea de tiempo (Diagrama de Gantt) de la simulación
// Muestra el estado actual y pasado de todos los procesos en cada tick de tiempo.
void imprimir_grafo_monitor(Proceso procesos[], int num_procesos, int tiempo_actual, const char* str_algo, int num_cpus) {
    limpiar_pantalla();
    printf("%s=== Monitor de Planificador (%s) - %d CPU(s) ===%s\n\n", COLOR_BOLD, str_algo, num_cpus, COLOR_RESET);
    
    // Imprimir cabecera de tiempo
    printf("Tick:  ");
    for (int t = 0; t <= tiempo_actual; t++) {
        if (t % 5 == 0) printf("%-2d", t);
        else printf("  ");
    }
    printf("\n       ");
    for (int t = 0; t <= tiempo_actual; t++) {
        printf("| ");
    }
    printf("\n");

    // Imprimir línea de tiempo para cada proceso
    for (int i = 0; i < num_procesos; i++) {
        printf("P%-2d:   ", procesos[i].id);
        for (int t = 0; t <= tiempo_actual; t++) {
            Estado e = procesos[i].historia[t];
            switch(e) {
                case ESPERANDO:  
                    printf("%s█ %s", COLOR_ESPERANDO, COLOR_RESET); break;
                case EJECUTANDO: 
                    if (t == tiempo_actual) {
                        printf("%s█ %s", COLOR_EJECUTANDO_ACT, COLOR_RESET); 
                    } else {
                        printf("%s█ %s", COLOR_EJECUTANDO_HIST, COLOR_RESET); 
                    }
                    break;
                case DURMIENDO:  
                    printf("%s█ %s", COLOR_DURMIENDO, COLOR_RESET); break;
                case TERMINADO:  
                    printf("%s· %s", COLOR_TERMINADO, COLOR_RESET); break;
            }
        }
        
        printf("  [CPU faltante: %2d] ", procesos[i].tiempo_restante_cpu);
        
        if (procesos[i].estado == EJECUTANDO) {
            int op_index = procesos[i].tiempo_total_cpu - procesos[i].tiempo_restante_cpu;
            if(op_index < 0) op_index = 0;
            if(op_index >= MAX_OPS) op_index = MAX_OPS - 1;
            printf("=> %sEJECUTANDO (CPU %d):%s %s", COLOR_EJECUTANDO_ACT, procesos[i].cpu_actual, COLOR_RESET, procesos[i].operaciones[op_index]);
        } 
        else if (procesos[i].estado == DURMIENDO) {
            printf("=> %sDURMIENDO:%s %s (Zzz: %d)", COLOR_DURMIENDO, COLOR_RESET, procesos[i].op_io, procesos[i].tiempo_restante_dormir);
        }
        else if (procesos[i].estado == ESPERANDO) {
            printf("=> %sESPERANDO%s", COLOR_ESPERANDO, COLOR_RESET);
        }
        else if (procesos[i].estado == TERMINADO) {
            printf("=> %sTERMINADO%s", COLOR_TERMINADO, COLOR_RESET);
        }
        printf("\n");
    }
    
    printf("\nLeyenda: %s█%s ESPERANDO | %s█%s EJECUTANDO | %s█%s DURMIENDO | %s·%s TERMINADO\n", 
            COLOR_ESPERANDO, COLOR_RESET, 
            COLOR_EJECUTANDO_HIST, COLOR_RESET, 
            COLOR_DURMIENDO, COLOR_RESET,
            COLOR_TERMINADO, COLOR_RESET);
}

int main(int argc, char *argv[]) {
    // Verificar si se solicita ayuda
    if (argc >= 2 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        imprimir_ayuda(argv[0]);
        return 0;
    }

    if (argc == 1) {
        printf("\n%s[INFO]%s Ejecutando simulación con valores por defecto. Usa '%s -h' para ver la ayuda y más opciones de configuración.\n\n", COLOR_BOLD, COLOR_RESET, argv[0]);
        sleep(2);
    }

    int num_procesos = 4;
    Algoritmo algo = RR;
    const char* str_algo = "Round Robin (Q=2)";

    // Parsear argumentos
    if (argc >= 2) {
        num_procesos = atoi(argv[1]);
        if (num_procesos <= 0 || num_procesos > 20) {
            printf("Error: Cantidad de procesos inválida. Usa un número entre 1 y 20.\n");
            return 1;
        }
    }
    if (argc >= 3) {
        if (strcmp(argv[2], "FCFS") == 0 || strcmp(argv[2], "fcfs") == 0) {
            algo = FCFS;
            str_algo = "First-Come, First-Served";
        } else if (strcmp(argv[2], "SJF") == 0 || strcmp(argv[2], "sjf") == 0) {
            algo = SJF;
            str_algo = "Shortest Job First";
        } else if (strcmp(argv[2], "RR") == 0 || strcmp(argv[2], "rr") == 0) {
            algo = RR;
        } else {
            printf("Error: Algoritmo desconocido '%s'. Opciones válidas: RR, FCFS, SJF\n", argv[2]);
            return 1;
        }
    }
    
    int num_cpus = 1;
    if (argc >= 4) {
        num_cpus = atoi(argv[3]);
        if (num_cpus <= 0 || num_cpus > 16) {
            printf("Error: Cantidad de CPUs inválida. Usa un número entre 1 y 16.\n");
            return 1;
        }
    }

    srand(time(NULL));
    Proceso procesos[num_procesos];
    
    // Inicializar procesos
    for (int i = 0; i < num_procesos; i++) {
        procesos[i].id = i + 1;
        procesos[i].estado = ESPERANDO; 
        procesos[i].tick_entrada_listo = 0; // Todos ingresan en T=0
        
        procesos[i].tiempo_total_cpu = (rand() % 9) + 2; 
        procesos[i].tiempo_restante_cpu = procesos[i].tiempo_total_cpu;
        procesos[i].tiempo_en_cpu_actual = 0;
        procesos[i].cpu_actual = -1;
        
        if (procesos[i].tiempo_total_cpu > 2 && (rand() % 10) < 6) {
            procesos[i].tick_para_dormir = (rand() % (procesos[i].tiempo_total_cpu - 2)) + 1; 
            procesos[i].duracion_sueno = (rand() % 4) + 1; 
            procesos[i].tiempo_restante_dormir = procesos[i].duracion_sueno;
            procesos[i].op_io = POOL_IO[rand() % 5];
        } else {
            procesos[i].tick_para_dormir = 0;
            procesos[i].tiempo_restante_dormir = 0;
            procesos[i].duracion_sueno = 0;
            procesos[i].op_io = "";
        }

        for (int j = 0; j < MAX_TICKS; j++) procesos[i].historia[j] = ESPERANDO; 
        for (int op = 0; op < MAX_OPS; op++) procesos[i].operaciones[op] = POOL_OPERACIONES[rand() % 10];
    }

    int tiempo = 0;
    int procesos_activos = num_procesos;
    int proceso_en_cpu[num_cpus];
    for (int c = 0; c < num_cpus; c++) proceso_en_cpu[c] = -1;
    int ultimo_revisado = 0;

    // Bucle principal de simulación
    while (procesos_activos > 0 && tiempo < MAX_TICKS) {
        for(int i=0; i < num_procesos; i++) {
            procesos[i].historia[tiempo] = procesos[i].estado;
        }

        imprimir_grafo_monitor(procesos, num_procesos, tiempo, str_algo, num_cpus);
        sleep(1); 

        // 1. Manejar E/S (Despertar procesos)
        for (int i = 0; i < num_procesos; i++) {
            if (procesos[i].estado == DURMIENDO) {
                procesos[i].tiempo_restante_dormir--;
                if (procesos[i].tiempo_restante_dormir <= 0) {
                    procesos[i].estado = ESPERANDO;
                    procesos[i].tick_entrada_listo = tiempo; // Ingresa al final de la cola
                }
            }
        }

        // 2. Ejecutar proceso en CPU
        for (int c = 0; c < num_cpus; c++) {
            if (proceso_en_cpu[c] != -1) {
                int p = proceso_en_cpu[c];
                procesos[p].tiempo_restante_cpu--;
                procesos[p].tiempo_en_cpu_actual++;

                if (procesos[p].tiempo_restante_cpu <= 0) {
                    procesos[p].estado = TERMINADO;
                    procesos_activos--;
                    proceso_en_cpu[c] = -1; 
                } 
                else if (procesos[p].tiempo_restante_cpu == procesos[p].tick_para_dormir) {
                    procesos[p].estado = DURMIENDO;
                    proceso_en_cpu[c] = -1; 
                } 
                else if (algo == RR && procesos[p].tiempo_en_cpu_actual >= QUANTUM) {
                    // Preemption solo en Round Robin
                    procesos[p].estado = ESPERANDO;
                    procesos[p].tick_entrada_listo = tiempo; // Vuelve al final de la cola
                    proceso_en_cpu[c] = -1;
                }
            }
        }

        // 3. Planificador: Asignar CPU
        for (int c = 0; c < num_cpus; c++) {
            if (proceso_en_cpu[c] == -1) {
                int elegido = -1;
                
                if (algo == RR) {
                    // ALGORITMO ROUND ROBIN (RR):
                    // Asigna a cada proceso un turno equitativo de CPU ("Quantum").
                    // Usa una cola circular para buscar al siguiente proceso ESPERANDO a partir 
                    // de donde se quedó el 'ultimo_revisado', garantizando justicia y evitando inanición.
                    for (int i = 0; i < num_procesos; i++) {
                        int idx = (ultimo_revisado + i) % num_procesos;
                        if (procesos[idx].estado == ESPERANDO) {
                            elegido = idx;
                            ultimo_revisado = (idx + 1) % num_procesos;
                            break;
                        }
                    }
                } 
                else if (algo == FCFS) {
                    // ALGORITMO FIRST-COME, FIRST-SERVED (FCFS):
                    // "El primero en llegar es el primero en ser atendido".
                    // Busca exhaustivamente el proceso con el menor valor de 'tick_entrada_listo'
                    // (el que lleva más tiempo en la cola de espera sin ser atendido).
                    int min_tick = 999999;
                    for (int i = 0; i < num_procesos; i++) {
                        if (procesos[i].estado == ESPERANDO) {
                            if (procesos[i].tick_entrada_listo < min_tick) {
                                min_tick = procesos[i].tick_entrada_listo;
                                elegido = i;
                            }
                        }
                    }
                }
                else if (algo == SJF) {
                    // ALGORITMO SHORTEST JOB FIRST (SJF):
                    // Prioriza el proceso que requiere menos trabajo total por parte de la CPU.
                    // Busca exhaustivamente el proceso con el menor valor en 'tiempo_restante_cpu',
                    // lo que teóricamente minimiza el tiempo de espera promedio, aunque puede causar inanición.
                    int min_tiempo = 999999;
                    for (int i = 0; i < num_procesos; i++) {
                        if (procesos[i].estado == ESPERANDO) {
                            if (procesos[i].tiempo_restante_cpu < min_tiempo) {
                                min_tiempo = procesos[i].tiempo_restante_cpu;
                                elegido = i;
                            }
                        }
                    }
                }

                if (elegido != -1) {
                    proceso_en_cpu[c] = elegido;
                    procesos[elegido].estado = EJECUTANDO; 
                    procesos[elegido].tiempo_en_cpu_actual = 0; 
                    procesos[elegido].cpu_actual = c;
                }
            }
        }

        tiempo++;
    }

    // Ultimo frame
    for(int i=0; i < num_procesos; i++) procesos[i].historia[tiempo] = procesos[i].estado;
    imprimir_grafo_monitor(procesos, num_procesos, tiempo, str_algo, num_cpus);
    printf("\n%sSimulación finalizada en el tiempo %d.%s\n", COLOR_BOLD, tiempo, COLOR_RESET);

    return 0;
}
