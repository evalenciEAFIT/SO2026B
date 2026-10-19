/**
 * @file consumer.c
 * @brief Consumidor en memoria compartida con mmap (Zero-Copy SPSC) con Color
 * @course Sistemas Operativos - DOCENCIA 2026-B
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <time.h>
#include "ring_buffer.h"

/* Códigos de Color ANSI */
#define CLR_RESET   "\033[0m"
#define CLR_BOLD    "\033[1m"
#define CLR_RED     "\033[31m"
#define CLR_GREEN   "\033[32m"
#define CLR_YELLOW  "\033[33m"
#define CLR_MAGENTA "\033[35m"
#define CLR_CYAN    "\033[36m"

static double get_time_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

int main(void) {
    printf("%s[CONSUMIDOR]%s Conectando a memoria compartida Zero-Copy...\n", CLR_MAGENTA, CLR_RESET);

    int fd = -1;
    while (fd < 0) {
        fd = open(SHM_FILE_PATH, O_RDWR);
        if (fd < 0) {
            usleep(1000);
        }
    }

    SharedRingBuffer *rb = (SharedRingBuffer *)mmap(
        NULL,
        sizeof(SharedRingBuffer),
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        fd,
        0
    );

    if (rb == MAP_FAILED) {
        perror(CLR_RED "mmap" CLR_RESET);
        close(fd);
        return EXIT_FAILURE;
    }

    printf("%s[CONSUMIDOR]%s Listo. Procesando flujo de mensajes...\n", CLR_MAGENTA, CLR_RESET);

    uint64_t consumed = 0;
    double t_start = get_time_sec();

    while (1) {
        if (rb->read_idx < rb->write_idx) {
            uint64_t slot = rb->read_idx % RING_CAPACITY;
            __atomic_thread_fence(__ATOMIC_ACQUIRE);

            volatile uint64_t seq = rb->slots[slot].seq_num;
            (void)seq;

            consumed++;
            rb->read_idx++;
        } else if (rb->finished && rb->read_idx >= rb->write_idx) {
            break;
        } else {
            #if defined(__x86_64__) || defined(_M_X64)
            __builtin_ia32_pause();
            #else
            usleep(1);
            #endif
        }
    }

    double t_end = get_time_sec();
    double elapsed = t_end - t_start;

    printf("%s[CONSUMIDOR]%s Total mensajes recibidos : %s%lu%s\n", 
           CLR_MAGENTA, CLR_RESET, CLR_BOLD, consumed, CLR_RESET);
    printf("%s[CONSUMIDOR]%s Tiempo total recepción   : %s%.4f segundos%s\n", 
           CLR_MAGENTA, CLR_RESET, CLR_YELLOW, elapsed, CLR_RESET);
    printf("%s[CONSUMIDOR]%s Rendimiento IPC          : %s%s%.2f millones de mensajes/segundo%s (%sCERO SYSCALLS%s)\n",
           CLR_MAGENTA, CLR_RESET, CLR_GREEN, CLR_BOLD, ((double)consumed / elapsed) / 1e6, CLR_RESET,
           CLR_CYAN, CLR_RESET);

    munmap(rb, sizeof(SharedRingBuffer));
    close(fd);
    return EXIT_SUCCESS;
}
