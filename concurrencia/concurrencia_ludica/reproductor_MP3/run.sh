#!/bin/bash

# Script de ejecución mejorado y acoplado al Makefile

# Función de ayuda del script
mostrar_ayuda_script() {
    echo -e "\033[1;32mUso de run.sh:\033[0m"
    echo -e "  ./run.sh [Opciones]"
    echo -e ""
    echo -e "\033[1;36mOpciones:\033[0m"
    echo -e "  -c, --cancion <archivo>   Ruta al archivo MP3 a reproducir."
    echo -e "  -demo, --demo             Inicia el programa en modo demostración con un archivo de prueba."
    echo -e "  -h, --ayuda               Muestra este menú de ayuda y termina."
    echo -e ""
    echo -e "Si no se especifica ninguna canción, el script intentará compilar/usar 'test.mp3'."
    exit 0
}

# Inicializar variable de archivo
MP3_FILE=""
DEMO_MODE=0

# Procesar argumentos del bash (soporta corto y largo)
while [[ "$#" -gt 0 ]]; do
    case $1 in
        -h|--ayuda) mostrar_ayuda_script ;;
        -c|--cancion) MP3_FILE="$2"; shift ;;
        -demo|--demo) DEMO_MODE=1 ;;
        *) MP3_FILE="$1" ;; # Posicional fallback
    esac
    shift
done

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
echo "[2] Verificando dependencias de decodificación..."
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
if [ "$DEMO_MODE" -eq 1 ] || [ -z "$MP3_FILE" ]; then
    echo "[3] Modo demostración activado. Preparando archivo de prueba (test.mp3)..."
    make test.mp3
    MP3_FILE="test.mp3"
else
    echo "[3] Usando archivo del usuario: $MP3_FILE"
    if [ ! -f "$MP3_FILE" ]; then
        echo -e "\033[1;31m Error: El archivo '$MP3_FILE' no existe.\033[0m"
        exit 1
    fi
fi

# 4. Ejecutar el reproductor a través del proceso main en C
echo "[4] Ejecutando el programa Orquestador (main.c)..."
echo "    Presiona Ctrl+C para salir prematuramente."
echo "-------------------------------------------------"
./main -c "$MP3_FILE"
echo "-------------------------------------------------"

echo " Script finalizado con éxito."
