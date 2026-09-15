CC = gcc
CFLAGS = -Wall -Wextra -O2 -pthread

all: matrix_concurrency

matrix_concurrency: matrix_concurrency.c
	$(CC) $(CFLAGS) -o matrix_concurrency matrix_concurrency.c

clean:
	rm -f matrix_concurrency
