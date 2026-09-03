#!/bin/bash

# Script de ejecución mejorado y acoplado al Makefile

echo "================================================="
echo "     INICIANDO REPRODUCTOR MP3 (SYSCALLS)  "
echo "================================================="

# 1. Compilar usando el Makefile
echo "[1] Delegando compilación a Make..."
make all
if [ $? -ne 0 ]; then
    echo " Error fatal: Falló la compilación."
    exit 1
fi

# 2. Comprobar dependencias de software (Decodificadores)
echo "[2]  Verificando dependencias de decodificación..."
if command -v mpg123 &> /dev/null; then
    echo "     mpg123 encontrado."
elif command -v ffplay &> /dev/null; then
    echo "     ffplay encontrado."
else
    echo "     ADVERTENCIA CRÍTICA: Ni 'mpg123' ni 'ffplay' están instalados."
    echo "    El código en C fallará en la llamada a execve()."
    echo "    Solución: sudo apt install mpg123 ffmpeg"
fi

# 3. Determinar el archivo MP3 a usar
MP3_FILE="test.mp3"

# Si el usuario pasa un argumento, usamos su archivo
if [ $# -eq 1 ]; then
    MP3_FILE="$1"
    echo "[3]  Usando archivo de usuario: $MP3_FILE"
    if [ ! -f "$MP3_FILE" ]; then
        echo " Error: El archivo '$MP3_FILE' no existe."
        exit 1
    fi
else
    # Si no pasan argumento, usamos Make para generar test.mp3
    echo "[3]  Usando archivo de prueba predeterminado..."
    make test.mp3
fi

# 4. Ejecutar el reproductor en C
echo "[4]  Ejecutando el programa (Presiona Ctrl+C para salir prematuramente)..."
echo "-------------------------------------------------"
./reproductor_mp3 "$MP3_FILE"
echo "-------------------------------------------------"

echo " Script finalizado con éxito."
