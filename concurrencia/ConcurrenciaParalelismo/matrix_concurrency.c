#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/time.h>

// ==============================================================================
// MACROS DE COLOR PARA LA TERMINAL (ANSI)
// ==============================================================================
#define C_RESET   "\x1b[0m"
#define C_BOLD    "\x1b[1m"
#define C_RED     "\x1b[31m"
#define C_GREEN   "\x1b[32m"
#define C_YELLOW  "\x1b[33m"
#define C_BLUE    "\x1b[34m"
#define C_CYAN    "\x1b[36m"

// ==============================================================================
// ESTRUCTURAS DE DATOS Y VARIABLES GLOBALES
// ==============================================================================
// Estructura para pasar argumentos a los hilos
typedef struct {
    int id;
    int start_row;
    int end_row;
    int n;          // Tamaño de la matriz
    int *A;
    int *B;
    int *C;
} ThreadData;

// ==============================================================================
// UTILIDADES
// ==============================================================================
// Función para medir el tiempo en segundos
double get_time() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1e6;
}

// Inicializar la matriz con números aleatorios
void init_matrix(int *mat, int size) {
    for (int i = 0; i < size * size; i++) {
        mat[i] = rand() % 10;
    }
}

// Verificar si dos matrices son iguales (para comprobar que los algoritmos están bien)
int check_matrices(int *mat1, int *mat2, int size) {
    for (int i = 0; i < size * size; i++) {
        if (mat1[i] != mat2[i]) return 0; // Falso, hay diferencias
    }
    return 1; // Verdadero, son iguales
}

// ==============================================================================
// 1. ALGORITMO SECUENCIAL
// ==============================================================================
void multiply_sequential(int *A, int *B, int *C, int n) {
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            int sum = 0;
            for (int k = 0; k < n; k++) {
                sum += A[i * n + k] * B[k * n + j];
            }
            C[i * n + j] = sum;
        }
    }
}

// ==============================================================================
// 2. ALGORITMO CON HILOS (Pthreads)
// ==============================================================================
void *thread_worker(void *arg) {
    ThreadData *data = (ThreadData *)arg;
    int n = data->n;
    
    // Cada hilo procesa un bloque de filas
    for (int i = data->start_row; i < data->end_row; i++) {
        for (int j = 0; j < n; j++) {
            int sum = 0;
            for (int k = 0; k < n; k++) {
                sum += data->A[i * n + k] * data->B[k * n + j];
            }
            data->C[i * n + j] = sum;
        }
    }
    return NULL;
}

void multiply_threads(int *A, int *B, int *C, int n, int num_threads) {
    pthread_t threads[num_threads];
    ThreadData thread_data[num_threads];
    
    int rows_per_thread = n / num_threads;

    for (int i = 0; i < num_threads; i++) {
        thread_data[i].id = i;
        thread_data[i].start_row = i * rows_per_thread;
        thread_data[i].end_row = (i == num_threads - 1) ? n : (i + 1) * rows_per_thread;
        thread_data[i].n = n;
        thread_data[i].A = A;
        thread_data[i].B = B;
        thread_data[i].C = C;
        
        pthread_create(&threads[i], NULL, thread_worker, &thread_data[i]);
    }

    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
}

// ==============================================================================
// 3. ALGORITMO CON PROCESOS (Fork)
// ==============================================================================
void multiply_processes(int *A, int *B, int *C_shared, int n, int num_procs) {
    int rows_per_proc = n / num_procs;

    for (int i = 0; i < num_procs; i++) {
        pid_t pid = fork();
        
        if (pid < 0) {
            perror("Error en fork");
            exit(1);
        } else if (pid == 0) {
            // CÓDIGO DEL PROCESO HIJO
            int start_row = i * rows_per_proc;
            int end_row = (i == num_procs - 1) ? n : (i + 1) * rows_per_proc;

            for (int r = start_row; r < end_row; r++) {
                for (int c = 0; c < n; c++) {
                    int sum = 0;
                    for (int k = 0; k < n; k++) {
                        sum += A[r * n + k] * B[k * n + c];
                    }
                    C_shared[r * n + c] = sum;
                }
            }
            exit(0); // Vital: el hijo muere aquí
        }
    }

    // CÓDIGO DEL PROCESO PADRE
    for (int i = 0; i < num_procs; i++) {
        wait(NULL);
    }
}

// ==============================================================================
// FUNCIÓN PRINCIPAL MAIN
// ==============================================================================
int main(int argc, char *argv[]) {
    int n = 800;          
    int num_workers = 4;  

    // Leer parámetros de configuración
    if (argc >= 2) n = atoi(argv[1]);
    if (argc >= 3) num_workers = atoi(argv[2]);
    
    if (n <= 0 || num_workers <= 0) {
        printf(C_RED "Error:" C_RESET " Tamaño y número de trabajadores deben ser > 0\n");
        return 1;
    }
    
    if (num_workers > n) {
        num_workers = n;
        printf(C_YELLOW "[!] Ajustando trabajadores a %d (límite máximo igual al número de filas).\n" C_RESET, num_workers);
    }

    size_t n_bytes = (size_t)n * n * sizeof(int);
    double start_time, end_time;
    double t_seq, t_thr, t_proc;

    printf("\n");
    printf(C_CYAN C_BOLD "===================================================================\n" C_RESET);
    printf(C_CYAN C_BOLD "        LABORATORIO DE CONCURRENCIA: MULTIPLICACIÓN DE MATRICES\n" C_RESET);
    printf(C_CYAN C_BOLD "===================================================================\n\n" C_RESET);
    
    printf(C_BOLD " ⚙️  CONFIGURACIÓN ACTUAL:\n" C_RESET);
    printf("    ▶ Uso: " C_YELLOW "./matrix_concurrency [TAMAÑO] [TRABAJADORES]\n" C_RESET);
    printf("    ▶ Tamaño de Matrices: " C_GREEN "%dx%d" C_RESET " (Total: %ld celdas)\n", n, n, (long)n*n);
    printf("    ▶ Número de Trabajadores (Hilos/Procesos): " C_GREEN "%d\n\n" C_RESET, num_workers);

    printf(C_BOLD " 📦 1. PREPARANDO MEMORIA Y MATRICES...\n" C_RESET);
    // Asignar memoria para A y B
    int *A = (int *)malloc(n_bytes);
    int *B = (int *)malloc(n_bytes);
    int *C_seq = (int *)malloc(n_bytes);
    int *C_thr = (int *)malloc(n_bytes);
    
    // Memoria compartida para Procesos (mmap)
    int *C_proc = (int *)mmap(NULL, n_bytes, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (C_proc == MAP_FAILED) {
        perror(C_RED "Error en mmap" C_RESET);
        exit(1);
    }

    srand(42);
    init_matrix(A, n);
    init_matrix(B, n);
    printf(C_GREEN "    ✔ Matrices generadas correctamente en memoria.\n\n" C_RESET);

    // =========================================================
    // EJECUCIÓN SECUENCIAL
    // =========================================================
    printf(C_BOLD " 🏃 2. EJECUTANDO MODO SECUENCIAL (1 TRABAJADOR)\n" C_RESET);
    start_time = get_time();
    multiply_sequential(A, B, C_seq, n);
    end_time = get_time();
    t_seq = end_time - start_time;
    printf("    " C_BLUE "[SECUENCIAL]" C_RESET " Tiempo de ejecución: \t" C_YELLOW "%.4f segundos\n\n" C_RESET, t_seq);

    // =========================================================
    // EJECUCIÓN CON HILOS
    // =========================================================
    printf(C_BOLD " 🧵 3. EJECUTANDO MODO HILOS (PTHREADS) (%d TRABAJADORES)\n" C_RESET, num_workers);
    start_time = get_time();
    multiply_threads(A, B, C_thr, n, num_workers);
    end_time = get_time();
    t_thr = end_time - start_time;
    
    printf("    " C_BLUE "[HILOS]" C_RESET " Tiempo de ejecución: \t" C_YELLOW "%.4f segundos\n" C_RESET, t_thr);
    printf("    " C_BLUE "[HILOS]" C_RESET " Rendimiento (Speedup): \t" C_GREEN "%.2fx más rápido" C_RESET " que el secuencial\n", t_seq / t_thr);
    
    if (check_matrices(C_seq, C_thr, n)) 
        printf("    " C_BLUE "[HILOS]" C_RESET " Verificación Matemática: \t" C_GREEN "✔ CORRECTO\n\n" C_RESET);
    else 
        printf("    " C_BLUE "[HILOS]" C_RESET " Verificación Matemática: \t" C_RED "❌ INCORRECTO\n\n" C_RESET);

    // =========================================================
    // EJECUCIÓN CON PROCESOS
    // =========================================================
    printf(C_BOLD " 🖥️  4. EJECUTANDO MODO PROCESOS (FORK + MMAP) (%d TRABAJADORES)\n" C_RESET, num_workers);
    start_time = get_time();
    multiply_processes(A, B, C_proc, n, num_workers);
    end_time = get_time();
    t_proc = end_time - start_time;
    
    printf("    " C_BLUE "[PROCESOS]" C_RESET " Tiempo de ejecución: \t" C_YELLOW "%.4f segundos\n" C_RESET, t_proc);
    printf("    " C_BLUE "[PROCESOS]" C_RESET " Rendimiento (Speedup): \t" C_GREEN "%.2fx más rápido" C_RESET " que el secuencial\n", t_seq / t_proc);
    
    if (check_matrices(C_seq, C_proc, n)) 
        printf("    " C_BLUE "[PROCESOS]" C_RESET " Verificación Matemática: \t" C_GREEN "✔ CORRECTO\n\n" C_RESET);
    else 
        printf("    " C_BLUE "[PROCESOS]" C_RESET " Verificación Matemática: \t" C_RED "❌ INCORRECTO\n\n" C_RESET);

    printf(C_CYAN C_BOLD "===================================================================\n\n" C_RESET);

    // Liberar memoria
    free(A);
    free(B);
    free(C_seq);
    free(C_thr);
    munmap(C_proc, n_bytes);

    return 0;
}
