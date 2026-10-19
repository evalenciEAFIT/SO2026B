obj-m += kmod_copy_user.o

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

USER_TARGET = test_copy_user
CC ?= gcc
CFLAGS ?= -Wall -Wextra -O2 -g

all: modules $(USER_TARGET)

modules:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

$(USER_TARGET): test_copy_user.c
	$(CC) $(CFLAGS) -o $@ test_copy_user.c

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f $(USER_TARGET)

load:
	sudo insmod kmod_copy_user.ko
	sudo chmod 666 /dev/demo_copy_user

unload:
	sudo rmmod kmod_copy_user

test: $(USER_TARGET)
	./$(USER_TARGET)

.PHONY: all modules clean load unload test
