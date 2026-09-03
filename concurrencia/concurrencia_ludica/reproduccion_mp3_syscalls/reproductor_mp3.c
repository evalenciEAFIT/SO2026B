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

#define BUFFER_SIZE 4096

extern char **environ; 

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

// ====================================================================
// MÓDULO 2: I/O (Lectura de Disco Duro)
// ====================================================================
int modulo_abrir_disco(const char *ruta_archivo) {
    // ---> SYSTEM CALL: open() <---
    int fd = open(ruta_archivo, O_RDONLY);
    if (fd < 0) {
        perror("Error al abrir el archivo MP3");
        exit(1);
    }
    return fd;
}

// ====================================================================
// MÓDULO 3: Creación de IPC (Inter-Process Communication)
// ====================================================================
void modulo_crear_ipc_pipe(int pipefd[2]) {
    // ---> SYSTEM CALL: pipe() <---
    // Crea el espacio de memoria protegido en el Kernel para el IPC.
    if (pipe(pipefd) == -1) {
        perror("Error al crear el pipe");
        exit(1);
    }
}

// ====================================================================
// MÓDULO 4: Ejecución del Proceso Hijo (Decodificador y Audio Out)
// ====================================================================
void modulo_hijo_decodificador(int pipefd[2]) {
    close(pipefd[1]); // El hijo no va a escribir, cerramos la escritura
    
    // ---> SYSTEM CALL: dup2() <---
    // Redirige la entrada estándar (0) para que lea del Pipe en lugar del teclado.
    dup2(pipefd[0], 0); 
    close(pipefd[0]);

    // ====================================================================
    // ---> SYSTEM CALL: execve() <---
    //  ¡AQUÍ ES DONDE SE REPRODUCE FÍSICAMENTE EL SONIDO! 
    // ====================================================================
    // execve() reemplaza este proceso en la RAM por el binario 'ffplay'.
    // A partir de esta línea, nuestro código C del hijo desaparece y ffplay 
    // toma el control absoluto: decodifica el MP3 en tiempo real y usa el 
    // Driver ALSA/PulseAudio del Kernel para enviar los pulsos eléctricos 
    // que hacen vibrar los parlantes físicos de la computadora.
    char *args_ffplay[] = {"/usr/bin/ffplay", "-i", "pipe:0", "-nodisp", "-autoexit", "-hide_banner", "-loglevel", "error", NULL};
    execve("/usr/bin/ffplay", args_ffplay, environ);
    
    char *args_mpg123[] = {"/usr/bin/mpg123", "-", NULL}; 
    execve("/usr/bin/mpg123", args_mpg123, environ);
    
    fprintf(stderr, "Error: No se encontró 'ffplay' ni 'mpg123' instalados.\n");
    exit(1);
}

// ====================================================================
// MÓDULO 5: Transmisión del Padre (Lectura de Disco a Memoria Pipe)
// ====================================================================
void modulo_padre_transmisor(int fd_mp3, int pipefd[2]) {
    close(pipefd[0]); // El padre no va a leer del pipe
    
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    size_t total_bytes = 0;
    
    // Obtenemos la capacidad máxima del Pipe en Kernel Space
    int pipe_size = 65536; 
#ifdef F_GETPIPE_SZ
    pipe_size = fcntl(pipefd[1], F_GETPIPE_SZ);
    if (pipe_size < 0) pipe_size = 65536;
#endif

    const char *notas[] = {"Do ", "Do#", "Re ", "Re#", "Mi ", "Fa ", "Fa#", "Sol", "Sol#", "La ", "La#", "Si "};
    int frecuencias[] = {261, 277, 293, 311, 329, 349, 369, 392, 415, 440, 466, 493}; 
    const char *bloques[] = {" ", "▂", "▃", "▄", "▅", "▆", "▇", "█"};

    while ((bytes_read = read(fd_mp3, buffer, BUFFER_SIZE)) > 0) {
        // Enviar datos al Kernel Pipe
        write(pipefd[1], buffer, bytes_read);
        total_bytes += bytes_read;
        
        // Consultar uso de memoria del Pipe (ioctl) y núcleo actual (sched_getcpu)
        int bytes_in_pipe = 0;
        ioctl(pipefd[1], FIONREAD, &bytes_in_pipe);
        if (bytes_in_pipe < 0) bytes_in_pipe = 0;
        float percent = ((float)bytes_in_pipe / (float)pipe_size) * 100.0;
        int core_id = sched_getcpu();
        
        // Simulación de Canales, Notas y Espectrómetro de Ondas (basado en entropía de compresión)
        unsigned char byte_L = (unsigned char)buffer[bytes_read / 4];
        unsigned char byte_R = (unsigned char)buffer[bytes_read - 10];
        int idx_L = byte_L % 12, idx_R = byte_R % 12;
        
        char onda_L[128] = "", onda_R[128] = "";
        int offset_L = 0, offset_R = 0;
        for (int i = 1; i <= 6; i++) {
            offset_L += sprintf(&onda_L[offset_L], "%s", bloques[((unsigned char)buffer[bytes_read / 10 * i]) % 8]);
            offset_R += sprintf(&onda_R[offset_R], "%s", bloques[((unsigned char)buffer[bytes_read - (bytes_read / 10 * i)]) % 8]);
        }
        
        // Monitor CLI animado de Flujo de Hardware y OS (2 líneas)
        // Usamos \033[K para limpiar la línea antes de dibujar, evitando parpadeos y solapamientos.
        printf("\033[K\033[1;36m[DISCO DURO: %7zu B]\033[0m --> \033[1;31m[MEMORIA PIPE: %5d/%5d B (%.1f%%)]\033[0m --> \033[1;33m[CPU Core %2d]\033[0m --> \033[1;37m[I/O PARLANTE]\033[0m\n"
               "\033[K                                     \033[1;36mL: %s(%3dHz) [%s] \033[1;32mR: %s(%3dHz) [%s]\033[0m\033[1A\r", 
               total_bytes, bytes_in_pipe, pipe_size, percent, core_id,
               notas[idx_L], frecuencias[idx_L], onda_L, 
               notas[idx_R], frecuencias[idx_R], onda_R);
        fflush(stdout); 
    }
    printf("\n\n\n");
    
    close(pipefd[1]); 
    close(fd_mp3);
}

// ====================================================================
// MAIN: Orquestador Principal
// ====================================================================
int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Uso: ./reproductor_mp3 <cancion.mp3>\n");
        return 1;
    }

    printf("Iniciando reproductor de MP3 (Arquitectura Modular)...\n");
    
    modulo_imprimir_teoria();

    int fd_mp3 = modulo_abrir_disco(argv[1]);
    
    int pipefd[2];
    modulo_crear_ipc_pipe(pipefd);

    // ---> SYSTEM CALL: fork() <---
    pid_t pid = fork();
    if (pid < 0) {
        perror("Error en fork()");
        return 1;
    }

    if (pid == 0) {
        modulo_hijo_decodificador(pipefd);
    } else {
        modulo_padre_transmisor(fd_mp3, pipefd);
        waitpid(pid, NULL, 0); 
        printf("Reproducción finalizada.\n");
    }

    return 0;
}
