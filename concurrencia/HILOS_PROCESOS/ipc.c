#include "ipc.h"
#include <unistd.h> // PARA QUÉ: read() y write(). POR QUÉ: Es el conducto IPC por donde fluyen los bytes (números enteros) a través del Pipe entre el Padre y el Hijo.
#include <stdlib.h> // PARA QUÉ: exit(EXIT_FAILURE). POR QUÉ: Ante un colapso en el puente IPC, el proceso debe abortar la ejecución inmediatamente por seguridad.
#include <stdio.h>  // PARA QUÉ: perror(). POR QUÉ: Si la tubería falla, nos imprime la alerta roja exacta enviada por el Kernel de Linux.

void enviar_dato_largo(int fd, long long dato) {
    write(fd, &dato, sizeof(long long));
}

long long recibir_dato_largo(int fd) {
    long long dato = 0;
    read(fd, &dato, sizeof(long long));
    return dato;
}
