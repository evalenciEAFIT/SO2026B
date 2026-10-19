#!/usr/bin/env bash
set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$DIR"

# Colores ANSI
BOLD='\033[1m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${CYAN}=== COMPILANDO BENCHMARK ===${NC}"
make

echo ""
echo -e "${BLUE}${BOLD}=== EJECUTANDO BENCHMARK DE RENDIMIENTO (MÉTRICAS DEL KERNEL) ===${NC}"
./benchmark

echo ""
echo -e "${YELLOW}${BOLD}=== ANÁLISIS DE LLAMADAS AL SISTEMA CON STRACE ===${NC}"
echo -e "${YELLOW}Observa cómo read() satura de syscalls 'read', mientras que mmap() no:${NC}\n"

echo -e "${CYAN}--- 1. Syscalls en Paso 1 (read clásico con 32MB) ---${NC}"
if [ -f "../paso1_io_clasico/paso1_io_clasico" ]; then
    strace -c -e trace=read,mmap,munmap,openat,close ../paso1_io_clasico/paso1_io_clasico > /dev/null
else
    echo "Paso 1 no está compilado. Ejecute 'make' en la raíz."
fi

echo ""
echo -e "${CYAN}--- 2. Syscalls en Paso 2 (mmap en espacio de usuario con 32MB) ---${NC}"
if [ -f "../paso2_mmap_usuario/paso2_mmap_usuario" ]; then
    strace -c -e trace=read,mmap,munmap,openat,close ../paso2_mmap_usuario/paso2_mmap_usuario > /dev/null
else
    echo "Paso 2 no está compilado. Ejecute 'make' en la raíz."
fi

echo ""
echo -e "${GREEN}${BOLD}=== PRUEBA FINALIZADA CON ÉXITO ===${NC}"
