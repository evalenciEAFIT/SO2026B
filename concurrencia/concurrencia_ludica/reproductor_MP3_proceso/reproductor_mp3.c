#define _GNU_SOURCE            // Habilita macros avanzadas del Kernel de Linux (ej. F_GETPIPE_SZ)

// =========================================================================
// LIBRERÍAS DEL SISTEMA Y SU PROPÓSITO
// =========================================================================
#include <stdio.h>             // Standard I/O: Provee printf(), sprintf() y perror() para la terminal.
#include <stdlib.h>            // Standard Library: Provee exit() para terminar procesos en caso de error.
#include <fcntl.h>             // File Control: Provee open() y flags como O_RDONLY para manipular archivos directos.
#include <unistd.h>            // UNIX Standard: Provee las Syscalls vitales: fork(), pipe(), dup2(), read(), write(), execve().
#include <sys/wait.h>          // Wait: Provee waitpid() para que el Padre sincronice y espere la muerte de sus Hijos.
#include <sys/ioctl.h>         // I/O Control: Provee ioctl() para espiar cuántos bytes físicos hay dentro del Pipe.
#include <sched.h>             // Scheduler: Provee sched_getcpu() para consultar en qué núcleo físico procesamos.
#include <getopt.h>            // CLI Args: Provee getopt para parsear argumentos de la terminal de forma estructurada.
#include <string.h>            // Manejo de cadenas para parseo de opciones y ensamblado de comandos.
#include <time.h>              // Time: Para calcular el tiempo real de reproducción (reloj del sistema)

#define BUFFER_SIZE 4096       // Tamaño del bloque de lectura/escritura (4 KB)

extern char **environ;         // Puntero global al entorno de variables del sistema (environment variables)

/* 
 * =========================================================================
 *  EXPLICACIÓN DEL ERROR DE HARDWARE EN ENTORNOS VIRTUALES (ALSA) 
 * Si obtienes "ALSA: Couldn't open audio device: Host is down":
 * El código C funcionó perfecto (leyó y transmitió el MP3 correctamente),
 * pero la terminal/contenedor no tiene hardware de sonido (parlantes) asignado.
 * =========================================================================
 */

// ====================================================================
// MÓDULO 1: Teoría para Estudiantes de Sistemas Operativos
// ====================================================================
void modulo_imprimir_teoria() {
    // Imprime conceptos importantes sobre cómo funcionan los Pipes y la memoria
    printf("\n\033[1;36m[Configuración I/O Out (Syscall ioctl() sobre Hardware)]\033[0m\n");
    printf(" ├── \033[1;32mFormato (SNDCTL_DSP_SETFMT):\033[0m PCM 16-bit (S16_LE)\n");
    printf(" ├── \033[1;32mCanales (SNDCTL_DSP_CHANNELS):\033[0m 2 (Estéreo)\n");
    printf(" └── \033[1;32mFrecuencia (SNDCTL_DSP_SPEED):\033[0m 44100 Hz\n");
    
    printf("\n\033[1;36m[¿Por qué se usa un IPC y dónde vive realmente en la Memoria?]\033[0m\n");
    printf(" ├── \033[1;32m¿Por qué no usar RAM normal?:\033[0m En Linux, los procesos están aislados.\n");
    printf(" │   El Padre y el Hijo tienen direcciones de Memoria Virtual separadas.\n");
    printf(" │   Si el Padre guarda la canción en una variable local o global en su RAM,\n");
    printf(" │   el Hijo no puede leerla por seguridad (lanzaría un Segmentation Fault).\n");
    printf(" └── \033[1;32m¿El Pipe es Stack o Heap?:\033[0m ¡Ninguno de los dos (en Espacio de Usuario)!\n");
    printf("     El búfer circular de la tubería vive dentro del \033[1;31mESPACIO DEL KERNEL\033[0m.\n");
    printf("     Es memoria que administra el SO directamente (usualmente vía kmalloc).\n");
    
    printf("\n\033[1;36m[Magia de la Reproducción: La Syscall execve()]\033[0m\n");
    printf(" ├── \033[1;32m¿Cómo suena la música?:\033[0m ¡Nuestro código C no la decodifica!\n");
    printf(" │   El Padre lee el disco y empuja los bytes por el Pipe. El Hijo usa execve().\n");
    printf(" └── \033[1;32m¿Qué hace execve() realmente?:\033[0m ¡Destruye y Reemplaza!\n");
    printf("     Al invocarla, el código C del proceso Hijo muere instantáneamente. Su espacio\n");
    printf("     de Memoria (Text, Data, Stack) es devorado y sobreescrito por el binario de\n");
    printf("     'ffplay', quien hereda el Pipe y usa ALSA para hacer vibrar los parlantes.\n\n");
    
    printf("\033[1;33m[ ] Presiona ENTER para iniciar la simulación en tiempo real...\033[0m");
    getchar();
    printf("\n");
}

void mostrar_ayuda(const char *nombre_programa) {
    // Función de ayuda que se despliega al usar la bandera -h o --ayuda
    printf("\n\033[1;32mUso del Reproductor MP3 Modular:\033[0m\n");
    printf("  %s -c <cancion.mp3> [Opciones]\n\n", nombre_programa);
    printf("\033[1;36mOpciones Principales:\033[0m\n");
    printf("  -c, --cancion <archivo>   Ruta al archivo MP3 a reproducir.\n");
    printf("  -s, --velocidad <valor>   Factor de velocidad (ej. 1.0, 1.5, 2.0). Por defecto: 1.0\n");
    printf("  -v, --volumen <valor>     Factor de volumen (ej. 0.5, 1.0, 2.0). Por defecto: 1.0\n");
    printf("  -d, --sentido <valor>     Sentido de la reproducción ('directo' o 'inverso'). Por defecto: directo\n");
    printf("  -q, --quiet               Desactiva los mensajes visuales del monitor (Disco, Pipe, CPU, Notas).\n");
    printf("\n\033[1;36mOpciones de Elementos del Monitor:\033[0m\n");
    printf("  --hide-cpu                Oculta la visualización del núcleo de CPU.\n");
    printf("  --hide-channels           Oculta la visualización de Canales L/R y Espectro.\n");
    printf("  --hide-pipe               Oculta la visualización del estado del IPC (Pipe).\n");
    printf("  --hide-disk               Oculta la visualización de los bytes leídos del disco.\n");
    printf("\n\033[1;36mOpciones del Monitor (CLI):\033[0m\n");
    printf("  -n, --nombre <cadena>     Nombre del proceso en el monitor. Por defecto: Proceso Independiente\n");
    printf("  -C, --color <codigo>      Código de color ANSI para la terminal (ej. 32=Verde, 36=Cyan).\n");
    printf("  -i, --indentacion <esp>   Espacios de indentación para alinear en salidas concurrentes.\n");
    printf("  -o, --offset <num>        Sube N líneas el cursor antes de imprimir (útil para concurrencia).\n");
    printf("  -h, --ayuda               Muestra este mensaje de ayuda y termina.\n\n");
}

// ====================================================================
// ESTRUCTURA Y FUNCIONES PARA ARGUMENTOS CLI
// ====================================================================
// Estructura para agrupar todas las opciones procesadas de la línea de comandos
typedef struct {
    char *cancion;
    char *velocidad;
    char *volumen;
    int inverso; // 0 para directo, 1 para inverso
    int quiet;   // 1 si está activado el modo silencioso (sin monitor visual)
    int hide_cpu; // Ocultar bloque CPU
    int hide_channels; // Ocultar bloque canales (L/R)
    int hide_pipe; // Ocultar bloque Pipe
    int hide_disk; // Ocultar bloque Disco
    char *nombre_proceso;
    char *codigo_color;
    char *indentacion;
    int offset;
} OpcionesCLI;

// Función encargada de parsear los argumentos usando getopt_long
void modulo_procesar_argumentos(int argc, char *argv[], OpcionesCLI *opciones) {
    // 1. Establecemos valores por defecto para que el programa no falle si faltan argumentos
    opciones->cancion = NULL;
    opciones->velocidad = "1.0";
    opciones->volumen = "1.0";
    opciones->inverso = 0;
    opciones->quiet = 0; // Por defecto el monitor visual está ACTIVO
    opciones->hide_cpu = 0;
    opciones->hide_channels = 0;
    opciones->hide_pipe = 0;
    opciones->hide_disk = 0;
    opciones->nombre_proceso = "Proceso Independiente";
    opciones->codigo_color = "35"; // Magenta
    opciones->indentacion = "";
    opciones->offset = 0;

    int opt, option_index = 0;
    
    // 2. Definimos las banderas largas equivalentes a las banderas cortas
    struct option long_options[] = {
        {"cancion", required_argument, 0, 'c'},
        {"velocidad", required_argument, 0, 's'},
        {"volumen", required_argument, 0, 'v'},
        {"sentido", required_argument, 0, 'd'},
        {"nombre", required_argument, 0, 'n'},
        {"color", required_argument, 0, 'C'},
        {"indentacion", required_argument, 0, 'i'},
        {"offset", required_argument, 0, 'o'},
        {"quiet", no_argument, 0, 'q'},
        {"ayuda", no_argument, 0, 'h'},
        // Banderas específicas para ocultar elementos: 
        {"hide-cpu", no_argument, 0, 1001},
        {"hide-channels", no_argument, 0, 1002},
        {"hide-pipe", no_argument, 0, 1003},
        {"hide-disk", no_argument, 0, 1004},
        {0, 0, 0, 0}
    };

    optind = 1; // Reiniciamos el índice de getopt por seguridad
    
    // 3. Iteramos por todos los argumentos dados en la terminal
    while ((opt = getopt_long(argc, argv, "c:s:v:d:n:C:i:o:qh", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'c': opciones->cancion = optarg; break;
            case 's': opciones->velocidad = optarg; break;
            case 'v': opciones->volumen = optarg; break;
            case 'd': 
                // Detecta si la cadena inicia con 'i' o 'I' para considerarlo inverso
                if (optarg && (optarg[0] == 'i' || optarg[0] == 'I')) opciones->inverso = 1;
                else opciones->inverso = 0;
                break;
            case 'n': opciones->nombre_proceso = optarg; break;
            case 'C': opciones->codigo_color = optarg; break;
            case 'i': opciones->indentacion = optarg; break;
            case 'o': opciones->offset = atoi(optarg); break;
            case 'q': opciones->quiet = 1; break; // Desactiva los mensajes visuales de monitoreo
            case 'h': mostrar_ayuda(argv[0]); exit(EXIT_SUCCESS); // Imprime ayuda y aborta ejecución
            // Casos especiales (Sin letra corta)
            case 1001: opciones->hide_cpu = 1; break;
            case 1002: opciones->hide_channels = 1; break;
            case 1003: opciones->hide_pipe = 1; break;
            case 1004: opciones->hide_disk = 1; break;
            default:
                mostrar_ayuda(argv[0]); // Si el argumento es inválido, muestra la ayuda
                exit(EXIT_FAILURE);
        }
    }
    
    // 4. Soporte para argumentos posicionales antiguos (si se usó sin banderas "-c", "-s", etc.)
    if (opciones->cancion == NULL && optind < argc) {
        opciones->cancion = argv[optind++];
        if (optind < argc) opciones->velocidad = argv[optind++];
        if (optind < argc) opciones->nombre_proceso = argv[optind++];
        if (optind < argc) opciones->codigo_color = argv[optind++];
        if (optind < argc) opciones->indentacion = argv[optind++];
    }

    // 5. Verificamos que obligatoriamente se haya definido el archivo a reproducir
    if (opciones->cancion == NULL) {
        fprintf(stderr, "\033[1;31mError: Debe especificar una canción.\033[0m\n");
        mostrar_ayuda(argv[0]);
        exit(EXIT_FAILURE);
    }
}

// Función encargada de concatenar la lógica del filtro de audio que usará FFmpeg/FFplay
void modulo_construir_filtro_audio(char *af_arg, size_t size, OpcionesCLI *opciones) {
    char filter[256] = ""; // Buffer temporal para la cadena del filtro
    int added = 0;         // Bandera para saber si ya agregamos un filtro (y poner comas)
    
    if (opciones->inverso) {
        // En inverso, los filtros los aplica popen() desde el padre.
        snprintf(af_arg, size, "anull"); // Filtro nulo para evitar errores de sintaxis en ffplay
        return;
    }
    
    // Si hay una velocidad diferente de 1.0, concatenamos 'atempo'
    if (opciones->velocidad && strcmp(opciones->velocidad, "1.0") != 0) {
        if (added) strcat(filter, ","); // Agrega la coma separadora de filtros si es necesario
        snprintf(filter + strlen(filter), sizeof(filter) - strlen(filter), "atempo=%s", opciones->velocidad);
        added = 1;
    } else {
        // En caso de que no haya filtro, ffplay puede fallar si mandamos cadena vacia
        // Mandamos atempo=1.0 como dummy filter seguro
        snprintf(filter + strlen(filter), sizeof(filter) - strlen(filter), "atempo=1.0");
        added = 1;
    }
    
    // Si hay un volumen diferente de 1.0, concatenamos 'volume'
    if (opciones->volumen && strcmp(opciones->volumen, "1.0") != 0) {
        if (added) strcat(filter, ",");
        snprintf(filter + strlen(filter), sizeof(filter) - strlen(filter), "volume=%s", opciones->volumen);
        added = 1;
    }
    
    // Guardamos la cadena final armada dentro de af_arg (Ej: "atempo=1.5,volume=2.0")
    snprintf(af_arg, size, "%s", filter);
}

// ====================================================================
// UTILIDAD: Obtener duración del archivo MP3 usando ffprobe
// ====================================================================
double obtener_duracion_audio(const char *ruta_archivo) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "ffprobe -v error -show_entries format=duration -of default=noprint_wrappers=1:nokey=1 \"%s\" 2>/dev/null", ruta_archivo);
    FILE *fp = popen(cmd, "r");
    if (!fp) return 0.0;
    
    char buffer[64];
    if (fgets(buffer, sizeof(buffer), fp) != NULL) {
        pclose(fp);
        return atof(buffer);
    }
    pclose(fp);
    return 0.0;
}

// ====================================================================
// MÓDULO 2: I/O (Lectura de Disco Duro)
// ====================================================================
int modulo_abrir_disco(OpcionesCLI *opciones, FILE **popen_file) {
    *popen_file = NULL;
    
    if (opciones->inverso) {
        char cmd[1024];
        // En modo inverso, inyectamos ffmpeg desde el Padre. 
        // ffmpeg lee el archivo, hace el búfer pesado para el 'areverse', lo invierte y lo decodifica a PCM WAV.
        // Como el Padre lee la salida WAV, ¡el Padre puede monitorizar en TIEMPO REAL el flujo invertido!
        snprintf(cmd, sizeof(cmd), "ffmpeg -i \"%s\" -af areverse,atempo=%s,volume=%s -f wav pipe:1 2>/dev/null", 
                 opciones->cancion, opciones->velocidad, opciones->volumen);
        
        *popen_file = popen(cmd, "r");
        if (!*popen_file) {
            perror("Error al ejecutar popen con ffmpeg");
            exit(1);
        }
        return fileno(*popen_file); // Retornamos el File Descriptor asociado a la tubería de popen
    } else {
        // ---> SYSTEM CALL: open() <---
        // Abre el archivo MP3 en modo Sólo Lectura (O_RDONLY) y nos da un File Descriptor
        int fd = open(opciones->cancion, O_RDONLY);
        if (fd < 0) {
            perror("Error al abrir el archivo MP3");
            exit(1);
        }
        return fd;
    }
}

// ====================================================================
// MÓDULO 3: Creación de IPC (Inter-Process Communication)
// ====================================================================
void modulo_crear_ipc_pipe(int pipefd[2]) {
    // ---> SYSTEM CALL: pipe() <---
    // Crea el espacio de memoria protegido en el Kernel para el IPC.
    // pipefd[0] será usado para LEER (Hijo)
    // pipefd[1] será usado para ESCRIBIR (Padre)
    if (pipe(pipefd) == -1) {
        perror("Error al crear el pipe");
        exit(1);
    }
}

// ====================================================================
// MÓDULO 4: Ejecución del Proceso Hijo (Decodificador y Audio Out)
// ====================================================================
void modulo_hijo_decodificador(int pipefd[2], OpcionesCLI *opciones) {
    // 1. Seguridad: El hijo NO va a escribir en la tubería, así que cerramos ese extremo.
    close(pipefd[1]); 
    
    // 2. ---> SYSTEM CALL: dup2() <---
    // Redirige la Entrada Estándar (STDIN = 0) hacia nuestro extremo de lectura del Pipe (pipefd[0]).
    // Esto engañará a ffplay haciéndole creer que los bytes vienen de STDIN, 
    // cuando en realidad provienen del Kernel Space enviado por el Padre.
    dup2(pipefd[0], 0); 
    
    // 3. Cerramos el File Descriptor original porque ya lo duplicamos en 0.
    close(pipefd[0]);

    // 4. Construimos la cadena de filtros en base a los argumentos CLI
    char af_arg[256];
    modulo_construir_filtro_audio(af_arg, sizeof(af_arg), opciones);

    // ====================================================================
    // 5. ---> SYSTEM CALL: execve() <---
    // ¡AQUÍ ES DONDE OCURRE EL REEMPLAZO DEL CÓDIGO ACTUAL POR OTRO PROGRAMA!
    // ====================================================================
    
    char cmd[1024];
    if (opciones->inverso) {
        // En modo inverso, la música ya viene invertida, filtrada y decodificada (WAV) directamente del Padre
        snprintf(cmd, sizeof(cmd), "ffplay -i pipe:0 -nodisp -autoexit -hide_banner -loglevel error");
    } else {
        // En sentido directo (normal), ffplay recibe el MP3 original, por lo que él mismo aplica los filtros (af_arg)
        snprintf(cmd, sizeof(cmd), "ffplay -i pipe:0 -nodisp -autoexit -hide_banner -loglevel error -af %s", af_arg);
    }
    
    // Invocamos un shell (/bin/sh) para que ejecute nuestro comando
    char *args_sh[] = {"/bin/sh", "-c", cmd, NULL};
    execve("/bin/sh", args_sh, environ);
    
    // --- ZONA INALCANZABLE (A menos que falle execve) ---
    // Fallback simple por si falla el shell o no hay ffplay
    char *args_mpg123[] = {"/usr/bin/mpg123", "-", NULL}; 
    execve("/usr/bin/mpg123", args_mpg123, environ);
    
    // Si llega a esta línea, es porque ninguna SysCall execve() pudo ejecutarse.
    fprintf(stderr, "Error: No se pudo ejecutar el decodificador de audio.\n");
    exit(1);
}

// ====================================================================
// MÓDULO 5: Transmisión del Padre (Lectura de Disco a Memoria Pipe)
// ====================================================================
void modulo_padre_transmisor(int fd_mp3, FILE *popen_file, int pipefd[2], OpcionesCLI *opciones) {
    // 1. Seguridad: El padre NO va a leer de la tubería, cerramos su extremo de lectura.
    close(pipefd[0]); 
    
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    size_t total_bytes = 0;
    
    // Obtenemos la capacidad máxima de la tubería configurada en el Kernel Space
    int pipe_size = 65536; 
#ifdef F_GETPIPE_SZ
    pipe_size = fcntl(pipefd[1], F_GETPIPE_SZ); // Fcntl manipula banderas de file descriptors
    if (pipe_size < 0) pipe_size = 65536; // Fallback en caso de error
#endif

    // Datos ornamentales para simular un espectrómetro visual
    const char *notas[] = {"Do ", "Do#", "Re ", "Re#", "Mi ", "Fa ", "Fa#", "Sol", "Sol#", "La ", "La#", "Si "};
    int frecuencias[] = {261, 277, 293, 311, 329, 349, 369, 392, 415, 440, 466, 493}; 
    const char *bloques[] = {" ", "▂", "▃", "▄", "▅", "▆", "▇", "█"};

    int iteracion = 0;
    struct timespec start_time, current_time;
    int first_read = 1;

    // Calculamos la duración total de la canción para el modo de cuenta regresiva
    double duracion_original = obtener_duracion_audio(opciones->cancion);
    double duracion_total = duracion_original / atof(opciones->velocidad);

    // 2. Bucle I/O: ---> SYSTEM CALL: read() <---
    // Lee chunks (pedazos) de 4096 bytes del archivo (o del stream WAV de popen) a la RAM del padre (buffer)
    while ((bytes_read = read(fd_mp3, buffer, BUFFER_SIZE)) > 0) {
        
        if (first_read) {
            // ---> SYSTEM CALL: clock_gettime() <---
            // Guarda el instante exacto en que comenzamos a transmitir audio válido
            clock_gettime(CLOCK_MONOTONIC, &start_time);
            first_read = 0;
        }
        
        // 3. ---> SYSTEM CALL: write() <---
        // Mueve esos bytes desde la RAM del padre hacia el Kernel Space (Pipe)
        // En modo inverso, esto se bloqueará cuando ffplay llene su buffer, obligando al monitor a refrescarse en tiempo real!
        write(pipefd[1], buffer, bytes_read);
        total_bytes += bytes_read;
        iteracion++;
        
        // Si el modo silencioso no está activado, mostramos el monitor en terminal
        if (!opciones->quiet && iteracion % 15 == 0) {
            int bytes_in_pipe = 0;
            
            // 4. ---> SYSTEM CALL: ioctl() <---
            // Le pregunta al Kernel: "¿Cuántos bytes no leídos quedan flotando en el buffer del Pipe?"
            ioctl(pipefd[1], FIONREAD, &bytes_in_pipe);
            if (bytes_in_pipe < 0) bytes_in_pipe = 0;
            
            // Calcula el porcentaje de ocupación de la tubería
            float percent = ((float)bytes_in_pipe / (float)pipe_size) * 100.0;
            
            // 5. ---> SYSTEM CALL: sched_getcpu() <---
            // Pregunta en qué núcleo físico del procesador estamos ejecutando este hilo actual.
            int core_id = sched_getcpu();
            
            // Lógica visual para la simulación del espectrómetro (Totalmente decorativo)
            // Cuando es modo inverso, estos bytes son muestras PCM WAV en lugar de bytes comprimidos MP3.
            unsigned char byte_L = (unsigned char)buffer[bytes_read / 4];
            unsigned char byte_R = (unsigned char)buffer[bytes_read - 10];
            int idx_L = byte_L % 12, idx_R = byte_R % 12;
            
            char onda_L[128] = "", onda_R[128] = "";
            int offset_L = 0, offset_R = 0;
            for (int i = 1; i <= 6; i++) {
                offset_L += sprintf(&onda_L[offset_L], "%s", bloques[((unsigned char)buffer[bytes_read / 10 * i]) % 8]);
                offset_R += sprintf(&onda_R[offset_R], "%s", bloques[((unsigned char)buffer[bytes_read - (bytes_read / 10 * i)]) % 8]);
            }
            
            // Capturamos el tiempo transcurrido
            clock_gettime(CLOCK_MONOTONIC, &current_time);
            double elapsed = (current_time.tv_sec - start_time.tv_sec) + 
                             (current_time.tv_nsec - start_time.tv_nsec) / 1e9;
            
            // Si está en modo INVERSO, hacemos una CUENTA REGRESIVA
            int tiempo_mostrar = 0;
            if (opciones->inverso && duracion_total > 0) {
                tiempo_mostrar = (int)duracion_total - (int)elapsed;
                if (tiempo_mostrar < 0) tiempo_mostrar = 0; // Prevenir tiempos negativos al final
            } else {
                tiempo_mostrar = (int)elapsed;
            }
            
            int mins = tiempo_mostrar / 60;
            int secs = tiempo_mostrar % 60;
            
            // 6. Monitor CLI optimizado dinámico: Construye el string de salida
            char monitor_str[2048] = "";
            char temp[256];
            
            // Base: [Nombre | PID X | Vel Y | Vol Z] [00:00]
            snprintf(monitor_str, sizeof(monitor_str), "%s\033[1;%sm[%s | PID %5d | Vel x%s | Vol x%s]\033[0m \033[1;32m[%02d:%02d]\033[0m ", 
                opciones->indentacion, opciones->codigo_color, opciones->nombre_proceso, getpid(), opciones->velocidad, opciones->volumen, mins, secs);

            // Se agregan los bloques si no fueron ocultados explícitamente
            if (!opciones->hide_disk) {
                // NOTA: En modo inverso, la etiqueta "DISCO" representará los Bytes Descomprimidos (PCM). 
                snprintf(temp, sizeof(temp), "\033[1;36m[DISCO: %7zu B]\033[0m ", total_bytes);
                strcat(monitor_str, temp);
            }

            if (!opciones->hide_pipe) {
                snprintf(temp, sizeof(temp), "\033[1;31m[PIPE: %5d B (%.1f%%)]\033[0m ", bytes_in_pipe, percent);
                strcat(monitor_str, temp);
            }

            if (!opciones->hide_cpu) {
                snprintf(temp, sizeof(temp), "\033[1;33m[CPU %2d]\033[0m ", core_id);
                strcat(monitor_str, temp);
            }

            if (!opciones->hide_channels) {
                snprintf(temp, sizeof(temp), "\033[1;%smL: %s(%3dHz) [%s] R: %s(%3dHz) [%s]\033[0m ", 
                    opciones->codigo_color, notas[idx_L], frecuencias[idx_L], onda_L,
                    notas[idx_R], frecuencias[idx_R], onda_R);
                strcat(monitor_str, temp);
            }
            
            if (opciones->offset > 0) {
                // \033[s (guarda cursor), \033[NA (sube N líneas), \r (inicio), \033[K (borra resto de linea), \033[u (restaura)
                printf("\033[s\033[%dA\r%s\033[K\033[u", opciones->offset, monitor_str);
            } else {
                // Comportamiento por defecto (misma línea)
                printf("\r%s\033[K", monitor_str);
            }
            fflush(stdout); // Fuerzo la impresión inmediata a consola
        }
    }
    
    // Finalizamos la transmisión (se leyó todo el archivo MP3 del disco)
    if (!opciones->quiet) {
        printf("\n%s\033[1;%sm[%s | PID %5d] Transmisión de archivo completada.\033[0m\n", opciones->indentacion, opciones->codigo_color, opciones->nombre_proceso, getpid());
    } else {
        printf("%s\033[1;%sm[%s | PID %5d] Transmisión silenciosa de archivo completada.\033[0m\n", opciones->indentacion, opciones->codigo_color, opciones->nombre_proceso, getpid());
    }
    
    // 7. Limpieza: Cerramos el File Descriptor del Pipe. 
    // Al cerrar, el Kernel envía la señal "EOF" (End of File) al Hijo, avisando que ya no hay más datos.
    close(pipefd[1]); 
    
    if (popen_file) {
        pclose(popen_file);
    } else {
        close(fd_mp3);
    }
}

// ====================================================================
// MAIN: Orquestador Principal
// ====================================================================
int main(int argc, char *argv[]) {
    // 1. Extraemos y guardamos todas las opciones recibidas por la terminal.
    OpcionesCLI opciones;
    modulo_procesar_argumentos(argc, argv, &opciones);

    // Mensaje de inicio del proceso
    printf("%s\033[1;%smIniciando reproductor de MP3 (Arquitectura Modular) a velocidad x%s, volumen x%s, sentido %s [%s]...\033[0m\n", 
           opciones.indentacion, opciones.codigo_color, opciones.velocidad, opciones.volumen, 
           opciones.inverso ? "INVERSO" : "DIRECTO", opciones.nombre_proceso);
    
    if (opciones.quiet) {
        printf("%s\033[1;33mModo Silencioso Activado (Visualizador Desactivado).\033[0m\n", opciones.indentacion);
    }
    
    // 2. Abrimos el archivo en Disco (Lectura o a través de popen si es inverso)
    FILE *popen_file = NULL;
    int fd_mp3 = modulo_abrir_disco(&opciones, &popen_file);
    
    // 3. Solicitamos al Kernel crear nuestro túnel de comunicación (Pipe)
    int pipefd[2];
    modulo_crear_ipc_pipe(pipefd);

    // 4. ---> SYSTEM CALL: fork() <---
    // Clonamos este proceso. A partir de esta línea, existirán DOS procesos ejecutando lo mismo.
    pid_t pid = fork();
    
    if (pid < 0) {
        perror("Error en fork()");
        return 1;
    }

    if (pid == 0) {
        // -------------------------------------------------------------
        // RAMA DEL PROCESO HIJO (Devuelto con pid = 0)
        // -------------------------------------------------------------
        // El hijo usará la tubería para Leer la música y decodificarla
        modulo_hijo_decodificador(pipefd, &opciones);
    } else {
        // -------------------------------------------------------------
        // RAMA DEL PROCESO PADRE (Devuelto con pid = PID del hijo)
        // -------------------------------------------------------------
        // El padre leerá el MP3 del disco y lo inyectará en la tubería
        modulo_padre_transmisor(fd_mp3, popen_file, pipefd, &opciones);
        
        // 5. ---> SYSTEM CALL: waitpid() <---
        // Sincronización: El Padre NO muere de inmediato; espera (se bloquea) 
        // a que el Hijo termine de reproducir la música y finalice su ejecución.
        waitpid(pid, NULL, 0); 
        
        printf("%s\033[1;%smReproducción finalizada (Velocidad: x%s) [%s].\033[0m\n", opciones.indentacion, opciones.codigo_color, opciones.velocidad, opciones.nombre_proceso);
    }

    return 0; // Termina la ejecución de forma limpia
}
