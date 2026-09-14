/* ============================================================================
 * IMPORTACIÓN DE LIBRERÍAS Y SUS JUSTIFICACIONES (Por Qué y Para Qué)
 * ============================================================================ */
#include <stdio.h>    // PARA QUÉ: Funciones de consola (printf, fgets, sscanf). POR QUÉ: Es el motor del Menú Interactivo y de los logs detallados del CLI.
#include <stdlib.h>   // PARA QUÉ: Control del Heap (malloc, free) y utilidades (exit). POR QUÉ: Al usar matrices enormes, es obligatorio pedir memoria dinámica al SO.
#include <pthread.h>  // PARA QUÉ: API de Hilos POSIX (pthread_create, pthread_join, Mutex). POR QUÉ: Implementa paralelismo nativo puro compartiendo la misma RAM.
#include <unistd.h>   // PARA QUÉ: Interfaz OS de UNIX (fork, pipe, getpid, close). POR QUÉ: Permite dialogar con el Kernel para clonar procesos y abrir conductos IPC.
#include <sys/wait.h> // PARA QUÉ: Función wait(). POR QUÉ: Un proceso padre debe obligatoriamente limpiar a sus hijos finalizados, o estos quedarán como "Zombies" flotando en RAM.
#include <time.h>     // PARA QUÉ: Relojes (clock_gettime). POR QUÉ: Provee precisión en nanosegundos para poder medir y comparar objetivamente quién fue más veloz (Hilos o Procesos).
#include <string.h>
#include "matrix.h"
#include "ipc.h"
#include "operaciones.h"

// --- Códigos de Color ANSI para CLI ---
#define C_RESET   "\x1b[0m"
#define C_INFO    "\x1b[36m" // Cyan
#define C_OK      "\x1b[32m" // Verde
#define C_WARN    "\x1b[33m" // Amarillo
#define C_HILO    "\x1b[35m" // Magenta
#define C_PROC    "\x1b[34m" // Azul
#define C_BOLD    "\x1b[1m"

// --- Variables Globales para Hilos ---
int* matriz_global = NULL;
long long suma_total = 0; // Coincide con la teoría
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

// Estructura de argumentos para hilos (Coincidente con la Teoría)
typedef struct {
    int id;
    int inicio;
    int fin;
} Rango;

// --- Función para medir tiempo ---
double obtener_tiempo(struct timespec start, struct timespec end) {
    return (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
}

// --- Código de los Hilos ---
/*
 * ALGORITMO DEL HILO (Mapeo y Reducción usando Memoria Compartida):
 * Se sincroniza exactamente igual que el pseudocódigo del documento teórico.
 */
void* trabajador_hilo(void* arg) {
    Rango* rango = (Rango*)arg;
    
    printf(C_HILO "  [Hilo %d] Nacimiento -> Iniciado en RAM compartida (Rango: %d - %d)" C_RESET "\n", rango->id, rango->inicio, rango->fin);
    
    // Paso 1: Mapeo puramente local (SIN BLOQUEOS para máxima velocidad)
    // printf(C_HILO "  [Hilo %d] Paso 1 -> Calculando sumatoria local sin bloquear a otros..." C_RESET "\n", rango->id);
    long long suma_local = op_sumar(matriz_global, rango->inicio, rango->fin);
    
    // Paso 2: Reducción (SECCIÓN CRÍTICA sincronizada por Mutex)
    printf(C_HILO "  [Hilo %d] Paso 2 -> Pidiendo Mutex para entrar a Sección Crítica y sumar %lld" C_RESET "\n", rango->id, suma_local);
    pthread_mutex_lock(&mutex);       // <-- Bloquea la puerta
    suma_total += suma_local;         // <-- Modifica la variable global segura
    pthread_mutex_unlock(&mutex);     // <-- Libera la puerta
    
    printf(C_HILO "  [Hilo %d] Fin -> Mutex liberado y tarea completada." C_RESET "\n", rango->id);
    pthread_exit(NULL);
}

// --- Evaluación con Hilos ---
long long evaluar_hilos(double* tiempo_tomado, int tamano_datos, int num_trabajadores) {
    struct timespec start, end;
    
    pthread_t hilos[num_trabajadores];
    Rango rangos[num_trabajadores];
    
    suma_total = 0;
    int chunk = tamano_datos / num_trabajadores;
    
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    // A) Lanzar múltiples hilos concurrentes
    printf(C_INFO " [Paso A] El hilo principal (Main) crea %d hilos compartiendo el mismo puntero a la matriz..." C_RESET "\n", num_trabajadores);
    for (int i = 0; i < num_trabajadores; i++) {
        rangos[i].id = i;
        rangos[i].inicio = i * chunk;
        rangos[i].fin = (i == num_trabajadores - 1) ? tamano_datos : (i + 1) * chunk;
        pthread_create(&hilos[i], NULL, trabajador_hilo, &rangos[i]);
    }
    
    // B) Barrera de Sincronización (Join)
    printf(C_INFO " [Paso B] El hilo principal invoca Join() operando como barrera hasta que todos terminen..." C_RESET "\n");
    for (int i = 0; i < num_trabajadores; i++) {
        pthread_join(hilos[i], NULL);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    *tiempo_tomado = obtener_tiempo(start, end);
    
    return suma_total;
}

// --- Evaluación con Procesos ---
long long evaluar_procesos(int* matriz, double* tiempo_tomado, int tamano_datos, int num_trabajadores) {
    struct timespec start, end;
    int fd_tuberia[2];
    
    // Crear Tubería (Pipe) para Inter-Process Communication (IPC)
    if (pipe(fd_tuberia) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }
    
    int chunk = tamano_datos / num_trabajadores;
    
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    // A) Lanzar múltiples procesos hijos concurrentes
    printf(C_INFO " [Paso A] Creando tubo IPC y bifurcando %d procesos hijos (Copy-On-Write)..." C_RESET "\n", num_trabajadores);
    for (int i = 0; i < num_trabajadores; i++) {
        pid_t pid = fork();
        
        if (pid == 0) {
            // --- INICIA CÓDIGO DEL PROCESO HIJO ---
            close(fd_tuberia[0]); // Por seguridad, el hijo cierra la lectura
            
            int inicio = i * chunk;
            int fin = (i == num_trabajadores - 1) ? tamano_datos : (i + 1) * chunk;
            
            printf(C_PROC "  [Proceso Hijo %d] Nacimiento -> Aislado (PID: %d | Rango: %d - %d)" C_RESET "\n", i, getpid(), inicio, fin);
            
            // Cálculo pesado sobre memoria aislada (Copy-On-Write)
            long long suma_local = op_sumar(matriz, inicio, fin);
            
            // Sincronización 1: Enviar serializado por IPC
            printf(C_PROC "  [Proceso Hijo %d] Paso 1 -> Enviando resultado %lld por túnel IPC (Pipe)..." C_RESET "\n", i, suma_local);
            enviar_dato_largo(fd_tuberia[1], suma_local);
            
            printf(C_PROC "  [Proceso Hijo %d] Fin -> Destruyendo proceso hijo." C_RESET "\n", i);
            close(fd_tuberia[1]); 
            exit(EXIT_SUCCESS); 
            // --- FIN CÓDIGO DEL PROCESO HIJO ---
        }
    }
    
    // --- CONTINÚA CÓDIGO DEL PROCESO PADRE ---
    close(fd_tuberia[1]); // Por seguridad, el padre cierra la escritura
    
    long long suma_total_procesos = 0;
    
    // B) Barrera de Lectura (Sincronización IPC)
    printf(C_INFO " [Paso B] Padre -> Bloqueado en read() esperando recolectar datos de la tubería IPC..." C_RESET "\n");
    for (int i = 0; i < num_trabajadores; i++) {
        suma_total_procesos += recibir_dato_largo(fd_tuberia[0]);
    }
    
    // C) Barrera de Limpieza (Zombies)
    printf(C_INFO " [Paso C] Padre -> Ejecutando wait() para recolectar procesos zombies (limpieza RAM)..." C_RESET "\n");
    for (int i = 0; i < num_trabajadores; i++) {
        wait(NULL);
    }
    
    close(fd_tuberia[0]);
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    *tiempo_tomado = obtener_tiempo(start, end);
    
    return suma_total_procesos;
}

void ejecutar_benchmark(int filas, int columnas, int num_trabajadores) {
    int tamano_datos = filas * columnas;
    double megabytes = (tamano_datos * sizeof(int)) / (1024.0 * 1024.0);
    
    printf(C_INFO "\n[*] Asignando matriz dinámica..." C_RESET "\n");
    printf("    Dimensiones  : %d filas x %d columnas (%d elementos)\n", filas, columnas, tamano_datos);
    printf("    Trabajadores : %d\n", num_trabajadores);
    printf("    Tamaño RAM   : %.2f MB\n", megabytes);
    
    matriz_global = crear_matriz(tamano_datos);
    inicializar_matriz(matriz_global, tamano_datos);
    
    printf(C_OK "[*] Memoria de datos aleatorios generada." C_RESET "\n");
    imprimir_fragmento(matriz_global, filas, columnas);
    printf("\n");
    
    // 1. Prueba de Hilos
    printf(C_BOLD "--- 1. EVALUACIÓN CON HILOS (Pthreads) ---" C_RESET "\n");
    printf(" Lanzando %d Hilos...\n", num_trabajadores);
    double tiempo_hilos = 0;
    long long res_hilos = evaluar_hilos(&tiempo_hilos, tamano_datos, num_trabajadores);
    printf(C_OK "\n -> Resultado Total  : %lld\n" C_RESET, res_hilos);
    printf(C_WARN " -> Tiempo de Hilos  : %f segundos\n\n" C_RESET, tiempo_hilos);
    
    // 2. Prueba de Procesos
    printf(C_BOLD "--- 2. EVALUACIÓN CON PROCESOS (Fork & IPC) ---" C_RESET "\n");
    printf(" Bifurcando %d Procesos hijos...\n", num_trabajadores);
    double tiempo_procesos = 0;
    long long res_procesos = evaluar_procesos(matriz_global, &tiempo_procesos, tamano_datos, num_trabajadores);
    printf(C_OK "\n -> Resultado Total  : %lld\n" C_RESET, res_procesos);
    printf(C_WARN " -> Tiempo Procesos  : %f segundos\n\n" C_RESET, tiempo_procesos);
    
    // 3. Conclusión
    printf(C_BOLD "======================================================\n");
    printf("                   CONCLUSIÓN FINAL\n");
    printf("======================================================" C_RESET "\n");
    
    printf(" Diferencia de Resultados: ");
    long long dif = res_hilos - res_procesos;
    if (dif == 0) printf(C_OK "%lld (Validación correcta)" C_RESET "\n", dif);
    else printf(C_WARN "%lld (¡ALERTA! Hubo un error de cálculo)" C_RESET "\n", dif);
    
    printf(" Diferencia de Tiempos   : ");
    double diff_time = tiempo_procesos - tiempo_hilos;
    if (diff_time > 0) {
        printf(C_INFO "Los Hilos fueron más rápidos por %f seg" C_RESET "\n", diff_time);
    } else {
        printf(C_INFO "Los Procesos fueron más rápidos por %f seg" C_RESET "\n", -diff_time);
    }
    printf("\n");
    
    liberar_matriz(matriz_global);
}

void demostrar_operaciones() {
    printf(C_BOLD "\n--- DEMOSTRACIÓN: OTRAS FUNCIONES MATEMÁTICAS ---" C_RESET "\n");
    
    int N_pequeno = 3;
    int* mat_pequena = crear_matriz(N_pequeno * N_pequeno);
    int* mat_rotada = crear_matriz(N_pequeno * N_pequeno);
    inicializar_matriz(mat_pequena, N_pequeno * N_pequeno); // Lleno de 1s
    
    printf(C_INFO " Matriz original 3x3 inicializada aleatoriamente." C_RESET "\n");
    
    op_escalar(mat_pequena, 0, N_pequeno * N_pequeno, 5); 
    printf(C_OK " ✓ op_escalar(matriz, 5) ejecutada." C_RESET "\n");
    
    op_rotar(mat_pequena, mat_rotada, N_pequeno, 0, N_pequeno);
    printf(C_OK " ✓ op_rotar(matriz) ejecutada a 90 grados." C_RESET "\n");
    
    int max_val = op_maximo(mat_pequena, 0, N_pequeno * N_pequeno);
    printf(C_OK " ✓ op_maximo(matriz) encontró el valor: %d." C_RESET "\n\n", max_val);
    
    printf(C_WARN " [!] Nota: op_sumar y op_maximo pueden paralelizarse gracias a las\n"
                  "     propiedades ASOCIATIVA y CONMUTATIVA. op_rotar no es\n"
                  "     conmutativa ni paralelizable tan trivialmente por IPC." C_RESET "\n\n");
    
    liberar_matriz(mat_pequena);
    liberar_matriz(mat_rotada);
}

void imprimir_teoria() {
    printf(C_BOLD "\n======================================================\n");
    printf("     VENTAJAS, DESVENTAJAS Y SOLUCIÓN DE PROBLEMAS\n");
    printf("======================================================" C_RESET "\n\n");
    
    printf(C_HILO "[*] HILOS (Pthreads)" C_RESET "\n");
    printf(C_OK "    + Ventajas   : " C_RESET "Muy ligeros, rápidos de crear, comunicación instantánea (Memoria Compartida).\n");
    printf(C_WARN "    - Desventajas: " C_RESET "Riesgo de 'Condiciones de Carrera'. Si un hilo falla (ej. Segfault), muere TODO el proceso.\n");
    printf(C_INFO "    > Solución   : " C_RESET "Usar primitivas de sincronización como Mutex (empleado aquí) o Semáforos.\n\n");
    
    printf(C_PROC "[*] PROCESOS (Fork)" C_RESET "\n");
    printf(C_OK "    + Ventajas   : " C_RESET "Aislamiento total. Si un hijo colapsa, el padre y los demás procesos siguen intactos.\n");
    printf(C_WARN "    - Desventajas: " C_RESET "Pesados al clonar memoria (overhead). Comunicación IPC (Pipes) lenta y compleja.\n");
    printf(C_INFO "    > Solución   : " C_RESET "Hacer recolección con wait() (evita procesos Zombies) y usar shm (memoria compartida del OS).\n\n");
}

int main(int argc, char* argv[]) {
    srand(time(NULL)); // Semilla para números aleatorios

    int filas = 10000;
    int columnas = 10000;
    int num_trabajadores = 4;
    
    // Si envían argumentos por CLI, los capturamos (filas, columnas, trabajadores)
    if (argc >= 2) filas = atoi(argv[1]);
    if (argc >= 3) columnas = atoi(argv[2]);
    if (argc >= 4) num_trabajadores = atoi(argv[3]);
    
    if (filas <= 0) filas = 1000;
    if (columnas <= 0) columnas = 1000;
    if (num_trabajadores <= 0) num_trabajadores = 1;

    int opcion = 0;
    do {
        printf(C_BOLD "\n======================================================\n");
        printf("         MENÚ INTERACTIVO DE CONCURRENCIA\n");
        printf("======================================================" C_RESET "\n");
        printf(" Parámetros Actuales: %d Filas x %d Columnas | %d trabajadores\n\n", filas, columnas, num_trabajadores);
        printf(" 1. Ejecutar Comparativa (Hilos vs Procesos)\n");
        printf(" 2. Demostración de Otras Operaciones (Escalar/Rotar)\n");
        printf(" 3. Ver Resumen Teórico (Ventajas y Desventajas)\n");
        printf(" 4. Configurar Parámetros (Filas, Columnas, Trabajadores)\n");
        printf(" 5. Salir\n");
        printf("======================================================\n");
        printf(" Seleccione una opción: ");
        
        char buffer_entrada[256];
        if (fgets(buffer_entrada, sizeof(buffer_entrada), stdin) != NULL) {
            if (sscanf(buffer_entrada, "%d", &opcion) != 1) {
                opcion = 0; // Entrada no válida
            }
        } else {
            opcion = 5; // Si hay EOF, salimos por seguridad
        }

        switch (opcion) {
            case 1:
                ejecutar_benchmark(filas, columnas, num_trabajadores);
                break;
            case 2:
                demostrar_operaciones();
                break;
            case 3:
                imprimir_teoria();
                break;
            case 4:
                printf(C_INFO "\n--- CONFIGURACIÓN DE PARÁMETROS ---" C_RESET "\n");
                
                printf(" Ingrese nuevas Filas (actual %d): ", filas);
                if (fgets(buffer_entrada, sizeof(buffer_entrada), stdin)) sscanf(buffer_entrada, "%d", &filas);
                
                printf(" Ingrese nuevas Columnas (actual %d): ", columnas);
                if (fgets(buffer_entrada, sizeof(buffer_entrada), stdin)) sscanf(buffer_entrada, "%d", &columnas);
                
                printf(" Ingrese cantidad de Trabajadores (actual %d): ", num_trabajadores);
                if (fgets(buffer_entrada, sizeof(buffer_entrada), stdin)) sscanf(buffer_entrada, "%d", &num_trabajadores);
                
                // Validaciones de seguridad para no romper la RAM o la concurrencia
                if (filas <= 0) filas = 1000;
                if (columnas <= 0) columnas = 1000;
                if (num_trabajadores <= 0) num_trabajadores = 1;
                
                printf(C_OK " ✓ Parámetros actualizados exitosamente." C_RESET "\n");
                break;
            case 5:
                printf(C_INFO "\nSaliendo del programa...\n" C_RESET);
                break;
            default:
                printf(C_WARN "\nOpción inválida. Intente de nuevo.\n" C_RESET);
        }
    } while (opcion != 5);
    
    return 0;
}
