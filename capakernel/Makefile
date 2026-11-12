# ==============================================================================
# Parcial 3 - Sistemas Operativos 2026-2 (EAFIT)
# secure_mmap: seguridad de archivos mapeados con mmap() en la capa kernel
#
#   make            -> modulo secure_mmap.ko + herramientas de prueba (build/)
#   make check      -> compila desde cero y FALLA si aparece cualquier warning
#   make load       -> carga el modulo (allowed_uid = tu UID)
#   make unload     -> descarga el modulo
#   make attacks    -> bateria de ataques contra secure_mmap  (sudo)
#   make baseline   -> mismos ataques contra el driver del Paso 6 (sin defensa)
#   make bench      -> mediciones de sobrecosto con perf/strace/ftrace (sudo)
#   make clean
# ==============================================================================

SHELL := /bin/bash

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

# Usar el MISMO compilador con el que se construyo el kernel. En Ubuntu el
# kernel se compila con "x86_64-linux-gnu-gcc-13" y si usamos "gcc" Kbuild
# imprime "warning: the compiler differs from the one used to build the
# kernel" aunque sea la misma version. El nombre se lee de la .config.
KCC    ?= $(shell sed -n 's/^CONFIG_CC_VERSION_TEXT="\([^ ]*\).*/\1/p' $(KDIR)/.config 2>/dev/null)
KCC_OK := $(if $(KCC),$(shell command -v $(KCC) 2>/dev/null))
KMAKE  := $(MAKE) -C $(KDIR) M=$(PWD) $(if $(KCC_OK),CC=$(KCC))

UCC     ?= gcc
UCFLAGS := -Wall -Wextra -Werror -O2 -g

TOOLS := build/smm_attacks build/smm_bench build/guard_preload.so

.PHONY: all module tools check load unload attacks baseline bench clean help

all: module tools

module:
	$(KMAKE) modules

tools: $(TOOLS)

build:
	mkdir -p build

build/smm_attacks: tests/smm_attacks.c | build
	$(UCC) $(UCFLAGS) -pthread -o $@ $<

build/smm_bench: tests/smm_bench.c | build
	$(UCC) $(UCFLAGS) -o $@ $<

build/guard_preload.so: tests/guard_preload.c | build
	$(UCC) $(UCFLAGS) -fPIC -shared -o $@ $< -ldl

# Compilacion limpia verificable: guarda el log y falla si hay "warning".
check: clean
	@mkdir -p build
	@set -o pipefail; $(MAKE) --no-print-directory all 2>&1 | tee build/build.log; \
	if [ $$? -ne 0 ]; then echo ""; echo "[X] La compilacion FALLO (ver build/build.log)"; exit 1; fi; \
	if grep -qi "warning" build/build.log; then \
		echo ""; echo "[X] La compilacion produjo advertencias (ver build/build.log)"; exit 1; \
	else \
		echo ""; echo "[OK] Compilacion limpia: 0 advertencias (modulo + herramientas)"; \
	fi

load: module
	sudo ./scripts/load.sh

unload:
	sudo ./scripts/unload.sh

attacks: all
	sudo ./scripts/run_attacks.sh

baseline: tools
	sudo ./scripts/run_baseline.sh

bench: all
	sudo ./scripts/run_bench.sh

clean:
	$(KMAKE) clean
	rm -rf build

help:
	@sed -n '2,14p' Makefile
