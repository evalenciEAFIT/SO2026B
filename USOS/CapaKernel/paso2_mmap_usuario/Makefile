CC ?= gcc
CFLAGS ?= -Wall -Wextra -O2 -g

TARGET = paso2_mmap_usuario

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) -o $@ main.c

clean:
	rm -f $(TARGET) archivo_prueba_mmap.bin

run: $(TARGET)
	./$(TARGET)

strace-run: $(TARGET)
	strace -c ./$(TARGET)

.PHONY: all clean run strace-run
