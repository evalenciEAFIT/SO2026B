# ==============================================================================
# Makefile Principal del Proyecto Docente: CapaKernel y mmap
# Curso: Sistemas Operativos (DOCENCIA 2026-B)
# ==============================================================================

SUBDIRS = paso1_io_clasico \
          paso2_mmap_usuario \
          paso3_driver_copy_user \
          paso4_driver_mmap_kernel \
          paso5_benchmark_comparativo \
          paso6_demand_paging_fault \
          paso7_ipc_ringbuffer \
          herramientas

.PHONY: all clean run run-paso1 run-paso2 run-benchmark run-ipc run-inspector load-modules unload-modules help

all:
	@echo "=========================================================="
	@echo " Compilando todos los modulos y programas del proyecto... "
	@echo "=========================================================="
	@for dir in $(SUBDIRS); do \
		echo "--> Compilando en $$dir..."; \
		$(MAKE) -C $$dir all || exit 1; \
	done
	@echo ""
	@echo "[+] ¡Compilacion completada con exito!"
	@echo "    - Ejecute el menu maestro con: ./run.sh"
	@echo "    - Pruebas usuario : make run-paso1 | make run-paso2 | make run-benchmark"
	@echo "    - IPC Zero-Copy   : make run-ipc"
	@echo "    - Inspector MMU   : make run-inspector"
	@echo "    - Drivers Kernel  : sudo ./scripts/load_modules.sh"

run:
	./run.sh

clean:
	@for dir in $(SUBDIRS); do \
		echo "--> Limpiando en $$dir..."; \
		$(MAKE) -C $$dir clean; \
	done
	@echo "[+] Todo limpio."

run-paso1:
	$(MAKE) -C paso1_io_clasico run

run-paso2:
	$(MAKE) -C paso2_mmap_usuario run

run-benchmark:
	./paso5_benchmark_comparativo/run_benchmark.sh

run-ipc:
	$(MAKE) -C paso7_ipc_ringbuffer run

run-inspector:
	$(MAKE) -C herramientas run

load-modules:
	./scripts/load_modules.sh

unload-modules:
	./scripts/unload_modules.sh

help:
	@echo "Opciones disponibles:"
	@echo "  make all           - Compila todos los pasos (programas y modulos de kernel)"
	@echo "  make clean         - Elimina todos los ejecutables, binarios y .ko"
	@echo "  make run-paso1     - Ejecuta la prueba de E/S clasica con read()"
	@echo "  make run-paso2     - Ejecuta la prueba con mmap() en espacio de usuario"
	@echo "  make run-benchmark - Ejecuta la comparativa completa con metricas y strace"
	@echo "  make run-ipc       - Ejecuta la comunicacion Inter-Procesos Zero-Copy (13M msg/s)"
	@echo "  make run-inspector - Inspecciona las tablas de paginas y fallos con pagemap"
	@echo "  make load-modules  - Compila y carga los drivers kmod_copy_user, kmod_mmap y kmod_demand_paging"
	@echo "  make unload-modules- Descarga los modulos del kernel"
