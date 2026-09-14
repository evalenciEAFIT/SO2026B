CC=gcc
CFLAGS=-Wall -Wextra -pthread -O2

all: main

main: main.o matrix.o ipc.o operaciones.o
	$(CC) $(CFLAGS) -o main main.o matrix.o ipc.o operaciones.o

main.o: main.c matrix.h ipc.h
	$(CC) $(CFLAGS) -c main.c

matrix.o: matrix.c matrix.h
	$(CC) $(CFLAGS) -c matrix.c

ipc.o: ipc.c ipc.h
	$(CC) $(CFLAGS) -c ipc.c

operaciones.o: operaciones.c operaciones.h
	$(CC) $(CFLAGS) -c operaciones.c

clean:
	rm -f main *.o
