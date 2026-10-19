#!/usr/bin/env bash
# ==============================================================================
# Script para cargar los módulos de kernel demostrativos con Salida a Color
# ==============================================================================
set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# Colores ANSI
BOLD='\033[1m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${BLUE}${BOLD}=== CARGANDO MÓDULOS DE KERNEL DOCENTES ===${NC}"

# Compilar los módulos
echo -e "${CYAN}[*] Compilando módulo Paso 3 (kmod_copy_user)...${NC}"
make -C "$DIR/paso3_driver_copy_user" modules > /dev/null

echo -e "${CYAN}[*] Compilando módulo Paso 4 (kmod_mmap)...${NC}"
make -C "$DIR/paso4_driver_mmap_kernel" modules > /dev/null

echo -e "${CYAN}[*] Compilando módulo Paso 6 (kmod_demand_paging)...${NC}"
make -C "$DIR/paso6_demand_paging_fault" modules > /dev/null

# 1. Cargar Paso 3 (copy_to_user)
if lsmod | grep -q "kmod_copy_user"; then
    echo -e "${YELLOW}[i] kmod_copy_user ya estaba cargado.${NC}"
else
    echo -e "${GREEN}[+] Cargando kmod_copy_user.ko...${NC}"
    sudo insmod "$DIR/paso3_driver_copy_user/kmod_copy_user.ko"
    sudo chmod 666 /dev/demo_copy_user 2>/dev/null || true
fi

# 2. Cargar Paso 4 (mmap con remap_pfn_range)
if lsmod | grep -q "kmod_mmap"; then
    echo -e "${YELLOW}[i] kmod_mmap ya estaba cargado.${NC}"
else
    echo -e "${GREEN}[+] Cargando kmod_mmap.ko...${NC}"
    sudo insmod "$DIR/paso4_driver_mmap_kernel/kmod_mmap.ko"
    sudo chmod 666 /dev/demo_mmap 2>/dev/null || true
fi

# 3. Cargar Paso 6 (demand paging con vm_ops->fault)
if lsmod | grep -q "kmod_demand_paging"; then
    echo -e "${YELLOW}[i] kmod_demand_paging ya estaba cargado.${NC}"
else
    echo -e "${GREEN}[+] Cargando kmod_demand_paging.ko...${NC}"
    sudo insmod "$DIR/paso6_demand_paging_fault/kmod_demand_paging.ko"
    sudo chmod 666 /dev/demo_demand_fault 2>/dev/null || true
fi

echo ""
echo -e "${BLUE}${BOLD}=== ESTADO DE DISPOSITIVOS REGISTRADOS ===${NC}"
ls -l /dev/demo_copy_user /dev/demo_mmap /dev/demo_demand_fault 2>/dev/null || true
echo ""
echo -e "${GREEN}${BOLD}¡Módulos cargados exitosamente! Puede ejecutar las pruebas:${NC}"
echo -e "  - ${CYAN}./paso3_driver_copy_user/test_copy_user${NC}"
echo -e "  - ${CYAN}./paso4_driver_mmap_kernel/test_mmap${NC}"
echo -e "  - ${CYAN}./paso6_demand_paging_fault/test_demand_paging${NC}"
