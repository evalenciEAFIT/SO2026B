CC ?= gcc
CFLAGS ?= -Wall -Wextra -O3 -g

TARGETS = producer consumer

all: $(TARGETS)

producer: producer.c ring_buffer.h
	$(CC) $(CFLAGS) -o $@ producer.c

consumer: consumer.c ring_buffer.h
	$(CC) $(CFLAGS) -o $@ consumer.c

clean:
	rm -f $(TARGETS) /tmp/demo_shm_ringbuffer.bin

run: $(TARGETS)
	./run_ipc.sh

.PHONY: all clean run
