#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdint.h>
#include <stdbool.h>

#define SHM_FILE_PATH   "/tmp/demo_shm_ringbuffer.bin"
#define RING_CAPACITY   1024   /* Capacidad de slots en el buffer circular */
#define MSG_PAYLOAD_LEN 64

/* Registro de mensaje individual */
typedef struct {
    uint64_t seq_num;
    char payload[MSG_PAYLOAD_LEN];
} Message;

/*
 * Estructura de Ring Buffer en memoria compartida (mmap).
 * Utiliza índices atómicos de lectura y escritura para sincronización lock-free
 * (Single Producer Single Consumer - SPSC).
 */
typedef struct {
    volatile uint64_t write_idx;  /* Modificado exclusivamente por el productor */
    volatile uint64_t read_idx;   /* Modificado exclusivamente por el consumidor */
    volatile bool finished;       /* Bandera de finalización */
    Message slots[RING_CAPACITY];
} SharedRingBuffer;

#endif /* RING_BUFFER_H */
