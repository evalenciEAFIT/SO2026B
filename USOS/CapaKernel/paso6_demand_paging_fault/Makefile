obj-m += kmod_demand_paging.o

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

USER_TARGET = test_demand_paging
CC ?= gcc
CFLAGS ?= -Wall -Wextra -O2 -g

all: modules $(USER_TARGET)

modules:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

$(USER_TARGET): test_demand_paging.c
	$(CC) $(CFLAGS) -o $@ test_demand_paging.c

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f $(USER_TARGET)

load:
	sudo insmod kmod_demand_paging.ko
	sudo chmod 666 /dev/demo_demand_fault

unload:
	sudo rmmod kmod_demand_paging

test: $(USER_TARGET)
	./$(USER_TARGET)

.PHONY: all modules clean load unload test
