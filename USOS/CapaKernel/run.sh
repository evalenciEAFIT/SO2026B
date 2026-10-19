#!/usr/bin/env bash
# ==============================================================================
# Script Maestro de Ejecución y Demostración: run.sh
# Asignatura: Sistemas Operativos (DOCENCIA 2026-B)
# Proyecto: Capa Kernel y Optimización de Rendimiento con mmap
# ==============================================================================

set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$DIR"

# Colores para salida de terminal
BOLD='\033[1m'
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # Sin color

print_header() {
    clear 2>/dev/null || true
    echo -e "${CYAN}================================================================================${NC}"
    echo -e "${BOLD}${BLUE}   SISTEMAS OPERATIVOS - DOCENCIA 2026-B : DEMOSTRACIÓN CAPA KERNEL Y MMAP     ${NC}"
    echo -e "${CYAN}================================================================================${NC}"
}

INTERACTIVE=false

pause() {
    if [ "$INTERACTIVE" = true ] && [ -t 0 ]; then
        echo ""
        echo -e "${YELLOW}Presione ENTER para continuar...${NC}"
        read -r
    fi
}

check_build() {
    echo -e "${CYAN}[*] Verificando compilación de todos los módulos y programas...${NC}"
    make all > /dev/null
    echo -e "${GREEN}[+] Todo el proyecto está compilado y listo.${NC}\n"
}

show_card() {
    local title="$1"
    local proposito="$2"
    local estrategia="$3"
    local observar="$4"

    echo -e "${CYAN}┌──────────────────────────────────────────────────────────────────────────────┐${NC}"
    echo -e "${CYAN}│${NC} ${BOLD}${BLUE}${title}${NC}"
    echo -e "${CYAN}├──────────────────────────────────────────────────────────────────────────────┤${NC}"
    echo -e "${CYAN}│${NC} ${GREEN}${BOLD}[PROPÓSITO]${NC}"
    echo -e "${CYAN}│${NC}   ${proposito}"
    echo -e "${CYAN}│${NC}"
    echo -e "${CYAN}│${NC} ${YELLOW}${BOLD}[ESTRATEGIA TÉCNICA]${NC}"
    echo -e "${CYAN}│${NC}   ${estrategia}"
    echo -e "${CYAN}│${NC}"
    echo -e "${CYAN}│${NC} ${MAGENTA}${BOLD}[QUÉ OBSERVAR EN ESTA PRUEBA]${NC}"
    echo -e "${CYAN}│${NC}   ${observar}"
    echo -e "${CYAN}└──────────────────────────────────────────────────────────────────────────────┘${NC}\n"
}

run_paso1() {
    print_header
    show_card "PASO 1: ENTRADA/SALIDA TRADICIONAL CON read() Y write()" \
        "Demostrar el cuello de botella del I/O clásico y medir la sobrecarga por cambios de contexto y doble búfer." \
        "El proceso llama repetidamente a read() en bloques de 4KB. Cada llamada provoca un Trap a Ring 0, búsqueda en Page Cache y copy_to_user() obligatorio." \
        "Fíjese en el contador de syscalls (8,192 para 32MB) y en el tiempo de CPU en modo sistema (Ring 0)."
    ./paso1_io_clasico/paso1_io_clasico
    pause
}

run_paso2() {
    print_header
    show_card "PASO 2: OPTIMIZACIÓN CON MEMORIA MAPEADA (mmap) EN USUARIO" \
        "Demostrar cómo eliminar las copias intermedias de memoria y las llamadas repetitivas al sistema usando mmap()." \
        "Se ejecuta sys_mmap una sola vez para crear el VMA. El acceso a los datos se hace por puntero directo en RAM. La MMU genera Page Faults (#PF) bajo demanda." \
        "0 llamadas a sys_read, solo 1 mmap, resolución transparente por MMU y un aumento sustancial de rendimiento (MB/s)."
    ./paso2_mmap_usuario/paso2_mmap_usuario
    pause
}

run_paso3() {
    print_header
    show_card "PASO 3: LA CAPA KERNEL TRADICIONAL: DRIVER CON copy_to_user()" \
        "Entender por qué el kernel NO puede compartir su memoria directamente sin un mecanismo especial y cómo opera dev_read()." \
        "El driver aloja memoria en kzalloc(). En dev_read(), invoca copy_to_user() para duplicar bytes por hardware/CPU hacia el espacio del proceso por seguridad (SMAP)." \
        "El proceso de usuario debe invocar la syscall read() y la CPU gasta ciclos copiando datos de un búfer a otro."
    echo -e "${YELLOW}Verificando si el módulo kmod_copy_user está cargado...${NC}"
    if ! lsmod | grep -q "kmod_copy_user"; then
        echo -e "${RED}[!] El módulo kmod_copy_user no está cargado.${NC}"
        echo -e "${CYAN}Cargando módulo con sudo...${NC}"
        sudo insmod "$DIR/paso3_driver_copy_user/kmod_copy_user.ko"
        sudo chmod 666 /dev/demo_copy_user 2>/dev/null || true
    fi
    ./paso3_driver_copy_user/test_copy_user
    pause
}

run_paso4() {
    print_header
    show_card "PASO 4: LA CAPA KERNEL OPTIMIZADA: DRIVER CON .mmap (ZERO-COPY)" \
        "Aprender a programar la operación .mmap en un controlador del kernel para lograr acceso directo a memoria física (Zero-Copy)." \
        "El driver implementa dev_mmap() y usa remap_pfn_range() para conectar las páginas físicas del kernel (PFN) directamente a la tabla de páginas (PTE) del usuario." \
        "El cliente lee y modifica los datos directamente en RAM sin invocar read()/write() ni copy_to_user(). Verificación en dmesg."
    echo -e "${YELLOW}Verificando si el módulo kmod_mmap está cargado...${NC}"
    if ! lsmod | grep -q "kmod_mmap"; then
        echo -e "${RED}[!] El módulo kmod_mmap no está cargado.${NC}"
        echo -e "${CYAN}Cargando módulo con sudo...${NC}"
        sudo insmod "$DIR/paso4_driver_mmap_kernel/kmod_mmap.ko"
        sudo chmod 666 /dev/demo_mmap 2>/dev/null || true
    fi
    ./paso4_driver_mmap_kernel/test_mmap
    echo -e "\n${CYAN}[*] Registro reciente del Kernel (dmesg):${NC}"
    sudo dmesg | tail -n 8 || true
    pause
}

run_paso5() {
    print_header
    show_card "PASO 5: SUITE DE BENCHMARKS COMPARATIVOS Y ANÁLISIS CON STRACE" \
        "Medición empírica rigurosa comparando read(4KB), read(64KB) y mmap() bajo métricas directas del kernel." \
        "Usa getrusage() para medir tiempo en usuario (Ring 3), tiempo en kernel (Ring 0) y fallos de página. strace -c analiza el porcentaje de tiempo por syscall." \
        "Observe cómo read() gasta más del 99% de su tiempo en syscalls dentro del kernel, mientras mmap() lo reduce a 0.00002s."
    ./paso5_benchmark_comparativo/run_benchmark.sh
    pause
}

run_paso6() {
    print_header
    show_card "PASO 6: PAGINACIÓN POR DEMANDA REAL EN EL KERNEL (vm_ops->fault)" \
        "Demostrar cómo los drivers avanzados asignan páginas físicas únicamente en el instante exacto en que la CPU sufre un Page Fault." \
        "En lugar de remap_pfn_range(), el driver instala vma->vm_ops. Al acceder a ptr[offset], la MMU dispara #PF y el kernel llama a demo_vma_fault()." \
        "Al acceder solo a la pág 0 y pág 5, el driver solo atiende 2 fallos. Las otras 6 páginas nunca se cargan ni consumen marcos físicos."
    echo -e "${YELLOW}Verificando si el módulo kmod_demand_paging está cargado...${NC}"
    if ! lsmod | grep -q "kmod_demand_paging"; then
        echo -e "${RED}[!] El módulo kmod_demand_paging no está cargado.${NC}"
        echo -e "${CYAN}Cargando módulo con sudo...${NC}"
        sudo insmod "$DIR/paso6_demand_paging_fault/kmod_demand_paging.ko"
        sudo chmod 666 /dev/demo_demand_fault 2>/dev/null || true
    fi
    ./paso6_demand_paging_fault/test_demand_paging
    echo -e "\n${CYAN}[*] Registro reciente del Kernel (dmesg) mostrando los Page Faults capturados:${NC}"
    sudo dmesg | tail -n 12 || true
    pause
}

run_paso7() {
    print_header
    show_card "PASO 7: IPC ULTRA-RÁPIDO ZERO-COPY CON RING BUFFER (mmap)" \
        "Aplicar mmap para resolver el problema Productor-Consumidor sin pasar por pipes, sockets ni copias en el kernel." \
        "Productor y Consumidor comparten un Ring Buffer circular en RAM con índices atómicos lock-free. Cero syscalls en el bucle principal de datos." \
        "Rendimiento descomunal de más de 10-13 millones de mensajes por segundo transferidos a velocidad pura de bus de memoria."
    ./paso7_ipc_ringbuffer/run_ipc.sh
    pause
}

run_inspector() {
    print_header
    show_card "HERRAMIENTA: INSPECCIÓN DE LA MMU Y PAGEMAP DE LINUX" \
        "Observar a bajo nivel la tabla de páginas del proceso y la traducción de direcciones virtuales a físicas." \
        "Lee /proc/self/pagemap antes y después de acceder a memoria. Muestra el bit 'Page Present' de la MMU y el Page Frame Number (PFN)." \
        "Compruebe cómo la página pasa de 'NO [0] (Paginación perezosa)' a 'SI [1] (Page Fault resuelto)' cuando la CPU ejecuta una escritura."
    ./herramientas/vma_inspector
    pause
}

load_all_modules() {
    print_header
    echo -e "${BOLD}--> CARGANDO TODOS LOS MÓDULOS DE KERNEL...${NC}"
    ./scripts/load_modules.sh
    pause
}

unload_all_modules() {
    print_header
    echo -e "${BOLD}--> DESCARGANDO TODOS LOS MÓDULOS DE KERNEL...${NC}"
    ./scripts/unload_modules.sh
    pause
}

run_all_sequential() {
    run_paso1
    run_paso2
    run_paso5
    run_paso7
    run_inspector

    echo -e "\n${YELLOW}¿Desea ejecutar también los pasos con Drivers de Kernel (Paso 3, 4 y 6)? [s/N]: ${NC}"
    read -r resp
    if [[ "$resp" =~ ^[sS]$ ]]; then
        load_all_modules
        run_paso3
        run_paso4
        run_paso6
        unload_all_modules
    fi
    echo -e "\n${GREEN}[+] ¡Demostración completa finalizada!${NC}"
}

# Si se pasa argumento por línea de comandos
case "$1" in
    --all|-a)
        check_build
        run_all_sequential
        exit 0
        ;;
    paso1|1)
        check_build
        run_paso1
        exit 0
        ;;
    paso2|2)
        check_build
        run_paso2
        exit 0
        ;;
    paso3|3)
        check_build
        run_paso3
        exit 0
        ;;
    paso4|4)
        check_build
        run_paso4
        exit 0
        ;;
    paso5|5|bench|benchmark)
        check_build
        run_paso5
        exit 0
        ;;
    paso6|6|fault)
        check_build
        run_paso6
        exit 0
        ;;
    paso7|7|ipc)
        check_build
        run_paso7
        exit 0
        ;;
    inspector|mmu)
        check_build
        run_inspector
        exit 0
        ;;
    load)
        load_all_modules
        exit 0
        ;;
    unload)
        unload_all_modules
        exit 0
        ;;
    help|--help|-h)
        echo "Uso: ./run.sh [opción]"
        echo "Opciones:"
        echo "  (sin args)   - Abre el menú interactivo"
        echo "  --all        - Ejecuta toda la suite secuencialmente"
        echo "  1..7         - Ejecuta directamente el paso indicado"
        echo "  inspector    - Ejecuta la inspección de la MMU (/proc/self/pagemap)"
        echo "  load/unload  - Carga o descarga los módulos del kernel"
        exit 0
        ;;
esac

# Menú Interactivo
check_build
INTERACTIVE=true

while true; do
    print_header
    echo -e "${BOLD}Seleccione una demostración pedagógica para ejecutar:${NC}"
    echo ""
    echo -e "  ${CYAN}[1]${NC} Paso 1: E/S Clásica con read() (Espacio de Usuario)"
    echo -e "  ${CYAN}[2]${NC} Paso 2: Memoria Mapeada con mmap() en Archivo (Espacio de Usuario)"
    echo -e "  ${CYAN}[3]${NC} Paso 3: Capa Kernel: Driver con .read y copy_to_user() [Kernel Module]"
    echo -e "  ${CYAN}[4]${NC} Paso 4: Capa Kernel: Driver con .mmap y remap_pfn_range() [Zero-Copy]"
    echo -e "  ${CYAN}[5]${NC} Paso 5: Suite de Benchmarks (read 4K vs 64K vs mmap + strace)"
    echo -e "  ${CYAN}[6]${NC} Paso 6: Paginación por Demanda Real en Kernel (vm_ops->fault)"
    echo -e "  ${CYAN}[7]${NC} Paso 7: Comunicación Inter-Procesos (IPC) Ring Buffer Zero-Copy (13M msg/s)"
    echo -e "  ${CYAN}[8]${NC} Herramienta: Inspección de Tablas de Páginas de la MMU (pagemap)"
    echo ""
    echo -e "  ${GREEN}[A]${NC} Ejecutar Demostración Completa Guiada (Todos los pasos)"
    echo -e "  ${YELLOW}[L]${NC} Cargar todos los módulos en el kernel (sudo insmod)"
    echo -e "  ${YELLOW}[U]${NC} Descargar todos los módulos del kernel (sudo rmmod)"
    echo -e "  ${RED}[0]${NC} Salir"
    echo ""
    echo -n -e "${BOLD}Ingrese su opción: ${NC}"
    read -r opt

    case "$opt" in
        1) run_paso1 ;;
        2) run_paso2 ;;
        3) run_paso3 ;;
        4) run_paso4 ;;
        5) run_paso5 ;;
        6) run_paso6 ;;
        7) run_paso7 ;;
        8) run_inspector ;;
        [aA]) run_all_sequential ;;
        [lL]) load_all_modules ;;
        [uU]) unload_all_modules ;;
        0|[qQ])
            echo -e "\n${GREEN}¡Hasta pronto!${NC}"
            break
            ;;
        *)
            echo -e "${RED}Opción inválida.${NC}"
            sleep 1
            ;;
    esac
done
