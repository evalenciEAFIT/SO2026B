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

rm -f /tmp/demo_shm_ringbuffer.bin

echo -e "${BLUE}${BOLD}=== INICIANDO COMUNICACIÓN INTER-PROCESOS (IPC) ZERO-COPY CON MMAP ===${NC}"
echo -e "${CYAN}Lanzando consumidor en segundo plano...${NC}"
./consumer &
CONSUMER_PID=$!

sleep 0.1

echo -e "${CYAN}Lanzando productor en primer plano...${NC}"
./producer

wait $CONSUMER_PID
rm -f /tmp/demo_shm_ringbuffer.bin
echo -e "${GREEN}${BOLD}=== PRUEBA DE IPC FINALIZADA CON ÉXITO ===${NC}"
