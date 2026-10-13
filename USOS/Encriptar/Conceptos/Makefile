# ==============================================================================
# Makefile - Suite de Cifrado de Archivos orientada a System Calls (Linux / SO)
# ==============================================================================

CC := gcc
CFLAGS := -Wall -Wextra -pedantic -std=c11 -O2 -Iinclude -D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE
LDFLAGS := 

SRC_DIR := src
INC_DIR := include
OBJ_DIR := obj
BIN := file_cipher

# Lista de archivos fuente
SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))

# Regla por defecto
all: $(BIN)

# Enlace del ejecutable final
$(BIN): $(OBJS)
	@echo "[LD] Enlazando ejecutable: $@"
	$(CC) $(OBJS) $(LDFLAGS) -o $@
	@echo ">> Compilación finalizada con éxito: ./$(BIN)"

# Compilación de objetos
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	@echo "[CC] Compilando $<"
	$(CC) $(CFLAGS) -c $< -o $@

# Creación del directorio de objetos
$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR)

# Limpieza de binarios y temporales
clean:
	@echo "[CLEAN] Eliminando archivos objeto y binarios..."
	@rm -rf $(OBJ_DIR) $(BIN) tests_out
	@echo "[CLEAN] Completado."

# Suite de pruebas automatizadas con verificación bit a bit (cmp)
test: $(BIN)
	@echo "======================================================================"
	@echo "              INICIANDO PRUEBAS AUTOMATIZADAS DE CIFRADO              "
	@echo "======================================================================"
	@mkdir -p tests_out
	@echo "Generando archivo de prueba de texto y binario..."
	@echo "Sistemas Operativos 2026B - Manejo de System Calls y Memoria Virtual en Linux!" > tests_out/sample.txt
	@head -c 65536 /dev/urandom > tests_out/sample.bin
	
	@echo -e "\n>>> 1. Probando CIFRADO CÉSAR (Byte-shift)..."
	./$(BIN) -a caesar -e -i tests_out/sample.txt -o tests_out/sample.caesar.enc -k 42
	./$(BIN) -a caesar -d -i tests_out/sample.caesar.enc -o tests_out/sample.caesar.dec -k 42
	cmp -s tests_out/sample.txt tests_out/sample.caesar.dec && echo "[OK] César texto idéntico bit a bit" || (echo "[FALLO] César texto difiere"; exit 1)

	@echo -e "\n>>> 2. Probando CIFRADO XOR (Flujo Polialfabético)..."
	./$(BIN) -a xor -e -i tests_out/sample.bin -o tests_out/sample.xor.enc -k "LinuxKernelSyscallKey2026" -b
	./$(BIN) -a xor -d -i tests_out/sample.xor.enc -o tests_out/sample.xor.dec -k "LinuxKernelSyscallKey2026"
	cmp -s tests_out/sample.bin tests_out/sample.xor.dec && echo "[OK] XOR binario idéntico bit a bit" || (echo "[FALLO] XOR binario difiere"; exit 1)

	@echo -e "\n>>> 3. Probando CIFRADO RC4 (Stream Cipher)..."
	./$(BIN) -a rc4 -e -i tests_out/sample.bin -o tests_out/sample.rc4.enc -k "SecretRC4KeyPassphrase" -b
	./$(BIN) -a rc4 -d -i tests_out/sample.rc4.enc -o tests_out/sample.rc4.dec -k "SecretRC4KeyPassphrase"
	cmp -s tests_out/sample.bin tests_out/sample.rc4.dec && echo "[OK] RC4 binario idéntico bit a bit" || (echo "[FALLO] RC4 binario difiere"; exit 1)

	@echo -e "\n>>> 4. Probando TRANSPOSICIÓN COLUMNAR (Bloques)..."
	./$(BIN) -a transp -e -i tests_out/sample.txt -o tests_out/sample.transp.enc -k "CLAVEMATRICIAL"
	./$(BIN) -a transp -d -i tests_out/sample.transp.enc -o tests_out/sample.transp.dec -k "CLAVEMATRICIAL"
	cmp -s tests_out/sample.txt tests_out/sample.transp.dec && echo "[OK] Transposición columnar idéntica bit a bit" || (echo "[FALLO] Transposición difiere"; exit 1)

	@echo -e "\n>>> 5. Probando MMAP (Memoria Virtual Zero-Copy con archivo de salida)..."
	./$(BIN) -a mmap -e -i tests_out/sample.bin -o tests_out/sample.mmap.enc -k "VirtualMemoryKey" -b
	./$(BIN) -a mmap -d -i tests_out/sample.mmap.enc -o tests_out/sample.mmap.dec -k "VirtualMemoryKey"
	cmp -s tests_out/sample.bin tests_out/sample.mmap.dec && echo "[OK] MMAP copia idéntico bit a bit" || (echo "[FALLO] MMAP copia difiere"; exit 1)

	@echo -e "\n>>> 6. Probando MMAP IN-SITU (Modificación directa sobre páginas virtuales)..."
	cp tests_out/sample.txt tests_out/sample.inplace.txt
	./$(BIN) -a mmap -e -i tests_out/sample.inplace.txt -k "InPlaceKey"
	./$(BIN) -a mmap -d -i tests_out/sample.inplace.txt -k "InPlaceKey"
	cmp -s tests_out/sample.txt tests_out/sample.inplace.txt && echo "[OK] MMAP in-situ idéntico bit a bit" || (echo "[FALLO] MMAP in-situ difiere"; exit 1)

	@echo -e "\n======================================================================"
	@echo "         ¡TODAS LAS PRUEBAS DE CIFRADO Y SYSTEM CALLS PASARON!         "
	@echo "======================================================================"

help:
	@echo "Opciones disponibles:"
	@echo "  make          : Compilar la aplicación"
	@echo "  make test     : Compilar y ejecutar pruebas de integridad criptográfica"
	@echo "  make clean    : Limpiar binarios y archivos temporales de prueba"
	@echo "  make help     : Mostrar esta ayuda"

.PHONY: all clean test help
