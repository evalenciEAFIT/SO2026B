obj-m += kmod_mmap.o

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

USER_TARGET = test_mmap
CC ?= gcc
CFLAGS ?= -Wall -Wextra -O2 -g

all: modules $(USER_TARGET)

modules:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

$(USER_TARGET): test_mmap.c
	$(CC) $(CFLAGS) -o $@ test_mmap.c

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f $(USER_TARGET)

load:
	sudo insmod kmod_mmap.ko
	sudo chmod 666 /dev/demo_mmap

unload:
	sudo rmmod kmod_mmap

test: $(USER_TARGET)
	./$(USER_TARGET)

.PHONY: all modules clean load unload test
