CC ?= gcc
CFLAGS ?= -Wall -Wextra -O2 -g

TARGET = vma_inspector

all: $(TARGET)

$(TARGET): vma_inspector.c
	$(CC) $(CFLAGS) -o $@ vma_inspector.c

clean:
	rm -f $(TARGET)

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run
