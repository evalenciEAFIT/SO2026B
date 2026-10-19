CC ?= gcc
CFLAGS ?= -Wall -Wextra -O2 -g

TARGET = benchmark

all: $(TARGET)

$(TARGET): benchmark.c
	$(CC) $(CFLAGS) -o $@ benchmark.c

clean:
	rm -f $(TARGET) bench_data.bin

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run
