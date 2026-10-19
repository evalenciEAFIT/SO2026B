/**
 * @file producer.c
 * @brief Productor en memoria compartida con mmap (Zero-Copy SPSC) con Color
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

#define NUM_MESSAGES 1000000

/* Códigos de Color ANSI */
#define CLR_RESET   "\033[0m"
#define CLR_BOLD    "\033[1m"
#define CLR_RED     "\033[31m"
#define CLR_GREEN   "\033[32m"
#define CLR_YELLOW  "\033[33m"
#define CLR_CYAN    "\033[36m"

static double get_time_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

int main(void) {
    printf("%s[PRODUCTOR]%s Iniciando transmisión Zero-Copy de %s%d mensajes%s...\n", 
           CLR_CYAN, CLR_RESET, CLR_BOLD, NUM_MESSAGES, CLR_RESET);

    int fd = open(SHM_FILE_PATH, O_RDWR | O_CREAT, 0666);
    if (fd < 0) {
        perror(CLR_RED "open" CLR_RESET);
        return EXIT_FAILURE;
    }

    if (ftruncate(fd, sizeof(SharedRingBuffer)) != 0) {
        perror(CLR_RED "ftruncate" CLR_RESET);
        close(fd);
        return EXIT_FAILURE;
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

    rb->write_idx = 0;
    rb->read_idx = 0;
    rb->finished = false;
    __atomic_thread_fence(__ATOMIC_SEQ_CST);

    double t_start = get_time_sec();

    for (uint64_t i = 1; i <= NUM_MESSAGES; i++) {
        while (rb->write_idx - rb->read_idx >= RING_CAPACITY) {
            #if defined(__x86_64__) || defined(_M_X64)
            __builtin_ia32_pause();
            #else
            usleep(1);
            #endif
        }

        uint64_t slot = rb->write_idx % RING_CAPACITY;
        rb->slots[slot].seq_num = i;
        snprintf(rb->slots[slot].payload, MSG_PAYLOAD_LEN, "Mensaje_ZeroCopy_#%lu", i);

        __atomic_thread_fence(__ATOMIC_RELEASE);
        rb->write_idx = i;
    }

    rb->finished = true;
    __atomic_thread_fence(__ATOMIC_SEQ_CST);

    double t_end = get_time_sec();
    double elapsed = t_end - t_start;

    printf("%s[PRODUCTOR]%s Finalizado: %d mensajes escritos en %s%.4f seg%s (%s%s%.2f millones msg/s%s)\n",
           CLR_CYAN, CLR_RESET, NUM_MESSAGES, CLR_YELLOW, elapsed, CLR_RESET, 
           CLR_BOLD, CLR_GREEN, ((double)NUM_MESSAGES / elapsed) / 1e6, CLR_RESET);

    munmap(rb, sizeof(SharedRingBuffer));
    close(fd);
    return EXIT_SUCCESS;
}
