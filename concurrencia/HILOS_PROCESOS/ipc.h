#ifndef IPC_H
#define IPC_H

void enviar_dato_largo(int fd, long long dato);
long long recibir_dato_largo(int fd);

#endif
