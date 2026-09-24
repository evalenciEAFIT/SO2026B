CC = gcc
COpenMP = -fopenmp
CFLAGS = -Wall -Wextra -O2 
OBJETIVO = ejemplo_openmp

all: $(OBJETIVO)

$(OBJETIVO): main.c
	$(CC) $(COpenMP) $(CFLAGS) -o $(OBJETIVO) main.c

clean:
	rm -f $(OBJETIVO)
