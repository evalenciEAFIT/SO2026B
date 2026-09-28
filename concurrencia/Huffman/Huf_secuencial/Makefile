# ==============================================================================
# Makefile para el Compresor y Descompresor de Huffman
# Asignatura: Sistemas Operativos
# ==============================================================================

COMPILADOR = gcc
OPCIONES = -Wall -Wextra -std=c99 -O2
EJECUTABLE = huffman
FUENTES = main.c huffman.c
OBJETOS = $(FUENTES:.c=.o)

# Regla por defecto: compilar el ejecutable
all: $(EJECUTABLE)

$(EJECUTABLE): $(OBJETOS)
	$(COMPILADOR) $(OPCIONES) -o $@ $^

%.o: %.c huffman.h
	$(COMPILADOR) $(OPCIONES) -c $< -o $@

# Regla de prueba automatizada completa
test: $(EJECUTABLE)
	@echo "\n>>> 1. Creando archivo de prueba con texto en español..."
	@echo "Sistemas Operativos 2026: Algoritmo de Huffman para compresion de archivos." > prueba.txt
	@echo "Este texto contiene repeticiones para evaluar la reduccion del tamano en disco." >> prueba.txt
	@echo "\n>>> 2. Probando Compresión hacia carpeta salidas/..."
	./$(EJECUTABLE) -c prueba.txt salidas/prueba.huf
	@echo "\n>>> 3. Probando Descompresión hacia carpeta salidas/..."
	./$(EJECUTABLE) -d salidas/prueba.huf salidas/prueba_recuperada.txt
	@echo "\n>>> 4. Verificando que el archivo original y el recuperado sean identicos..."
	diff -s prueba.txt salidas/prueba_recuperada.txt
	@echo "\n>>> ¡Prueba automatizada completada con exito!"

# Limpieza de archivos objeto y temporales
clean:
	rm -rf $(OBJETOS) $(EJECUTABLE) prueba.txt salidas documento_prueba*

.PHONY: all test clean
