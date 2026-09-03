# Makefile para el proyecto Reproductor MP3 (Sistemas Operativos)

# Variables de compilación
CC = gcc
CFLAGS = -Wall -Wextra -O2
TARGET = reproductor_mp3
SRC = reproductor_mp3.c

# Regla principal
all: $(TARGET)

# Regla de compilación del binario
$(TARGET): $(SRC)
	@echo " [MAKE] Compilando $(SRC)..."
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

# Regla para generar un MP3 de prueba
test.mp3:
	@echo " [MAKE] Generando archivo de audio de prueba (test.mp3)..."
	@ffmpeg -f lavfi -i sine=frequency=1000:duration=3 -c:a libmp3lame test.mp3 -y -v quiet || (echo " Error: ffmpeg no instalado" && exit 1)

# Regla para ejecutar el programa (compila y genera test.mp3 si es necesario)
run: $(TARGET) test.mp3
	@echo " [MAKE] Ejecutando reproductor..."
	./$(TARGET) test.mp3

# Regla para limpiar los archivos generados
clean:
	@echo " [MAKE] Limpiando binarios y archivos de prueba..."
	rm -f $(TARGET) test.mp3

# Marcar reglas que no son archivos reales
.PHONY: all run clean
