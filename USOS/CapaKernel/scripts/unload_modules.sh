#!/usr/bin/env bash
# ==============================================================================
# Script para descargar los módulos de kernel demostrativos con Salida a Color
# ==============================================================================

# Colores ANSI
BOLD='\033[1m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${BLUE}${BOLD}=== DESCARGANDO MÓDULOS DE KERNEL ===${NC}"

if lsmod | grep -q "kmod_demand_paging"; then
    echo -e "${YELLOW}[-] Descargando kmod_demand_paging...${NC}"
    sudo rmmod kmod_demand_paging
    echo -e "${GREEN}[+] kmod_demand_paging descargado.${NC}"
fi

if lsmod | grep -q "kmod_mmap"; then
    echo -e "${YELLOW}[-] Descargando kmod_mmap...${NC}"
    sudo rmmod kmod_mmap
    echo -e "${GREEN}[+] kmod_mmap descargado.${NC}"
fi

if lsmod | grep -q "kmod_copy_user"; then
    echo -e "${YELLOW}[-] Descargando kmod_copy_user...${NC}"
    sudo rmmod kmod_copy_user
    echo -e "${GREEN}[+] kmod_copy_user descargado.${NC}"
fi

echo -e "${GREEN}${BOLD}=== LIMPIEZA COMPLETADA ===${NC}"
