# Makefile para el proyecto Reproductor MP3 (Sistemas Operativos)

# Variables de compilación
CC = gcc
CFLAGS = -Wall -Wextra -O2
TARGET = reproductor_mp3
MAIN_TARGET = main
SRC = reproductor_mp3.c
MAIN_SRC = main.c

# Regla principal
all: $(TARGET) $(MAIN_TARGET)

# Regla de compilación del binario reproductor
$(TARGET): $(SRC)
	@echo " [MAKE] Compilando $(SRC)..."
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

# Regla de compilación del binario main
$(MAIN_TARGET): $(MAIN_SRC)
	@echo " [MAKE] Compilando $(MAIN_SRC)..."
	$(CC) $(CFLAGS) $(MAIN_SRC) -o $(MAIN_TARGET)

# Regla para generar un MP3 de prueba
test.mp3:
	@echo " [MAKE] Generando archivo de audio de prueba (test.mp3)..."
	@ffmpeg -f lavfi -i sine=frequency=1000:duration=3 -c:a libmp3lame test.mp3 -y -v quiet || (echo " Error: ffmpeg no instalado" && exit 1)

# Regla para ejecutar el programa (compila y genera test.mp3 si es necesario)
run: all test.mp3
	@echo " [MAKE] Ejecutando demo a través de run.sh..."
	./run.sh -demo

# Regla para limpiar los archivos generados
clean:
	@echo " [MAKE] Limpiando binarios y archivos de prueba..."
	rm -f $(TARGET) $(MAIN_TARGET) test.mp3

# Marcar reglas que no son archivos reales
.PHONY: all run clean
