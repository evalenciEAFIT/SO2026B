#!/bin/bash
# ==============================================================================
# run_bench.sh - Medicion del sobrecosto (overhead) de secure_mmap
#
#   sudo ./scripts/run_bench.sh            (PAGES=4096 REPS=10 por defecto)
#   sudo PAGES=8192 REPS=20 ./scripts/run_bench.sh
#
# Compara la misma carga sobre:
#   A) un archivo normal en tmpfs (/dev/shm): page cache, sin cifrado (linea base)
#   B) /dev/secure_mmap: control de acceso + descifrado AES-CTR + CRC por fault
#
# Herramientas: smm_bench (tiempos), /proc/secure_mmap (latencia interna de
# smm_fault), perf stat (page faults, cambios de contexto), strace -c / -T
# (numero y costo de syscalls) y ftrace function_graph (desglose de smm_fault).
# ==============================================================================
source "$(dirname "$0")/common.sh"
need_root

PAGES="${PAGES:-4096}"
REPS="${REPS:-10}"
BENCH="$HERE/build/smm_bench"
BASE=/dev/shm/smm_baseline.bin

mkdir -p "$RESULTS"
LOG="$RESULTS/bench_$(date +%Y%m%d_%H%M%S).txt"
exec > >(tee "$LOG") 2>&1

[ -f "$KO" ] && [ -x "$BENCH" ] || { echo "Primero compile con: make"; exit 1; }

echo "=============================================================================="
echo " secure_mmap - medicion de sobrecosto   $(date)"
echo " kernel $(uname -r) | CPU: $(grep -m1 'model name' /proc/cpuinfo | cut -d: -f2)"
echo " $PAGES paginas ($((PAGES*4/1024)) MiB) | $REPS repeticiones"
echo "=============================================================================="

load_module vault_pages="$PAGES" allow_write=0
head -c $((PAGES*4096)) /dev/urandom > "$BASE"
chown "$REAL_USER" "$BASE"

OLD_PARANOID="$(cat /proc/sys/kernel/perf_event_paranoid)"
sysctl -qw kernel.perf_event_paranoid=1
trap 'sysctl -qw kernel.perf_event_paranoid="$OLD_PARANOID"' EXIT

echo ""; echo "### 1. Tiempos de usuario (smm_bench)"
echo "--- A) archivo normal en tmpfs (linea base)"
as_user "$BENCH" "$BASE" "$PAGES" "$REPS"
echo reset > "$PROC"
echo "--- B) /dev/secure_mmap"
as_user "$BENCH" "$DEV" "$PAGES" "$REPS"

echo ""; echo "### 2. Latencia interna de smm_fault (medida con ktime_get_ns en el kernel)"
grep -E "page faults|promedio|maximo" "$PROC"

if command -v perf >/dev/null 2>&1; then
    EV=page-faults,minor-faults,major-faults,context-switches,task-clock
    echo ""; echo "### 3. perf stat (mmap_cold, 5 corridas)"
    echo "--- A) archivo normal"
    as_user perf stat -e "$EV" -r 5 "$BENCH" "$BASE" "$PAGES" 2 mmap_cold 2>&1 | grep -vE "^(Objetivo|modo|mmap|CSV)"
    echo "--- B) /dev/secure_mmap"
    as_user perf stat -e "$EV" -r 5 "$BENCH" "$DEV" "$PAGES" 2 mmap_cold 2>&1 | grep -vE "^(Objetivo|modo|mmap|CSV)"
else
    echo "(perf no instalado: sudo apt install linux-tools-\$(uname -r))"
fi

if command -v strace >/dev/null 2>&1; then
    echo ""; echo "### 4. strace -c: syscalls de read() vs mmap() sobre /dev/secure_mmap"
    echo "--- read_full"
    as_user strace -c -e trace=openat,read,mmap,munmap,close "$BENCH" "$DEV" "$PAGES" 2 read_full 2>&1 | grep -vE "^(Objetivo|modo|read_full|CSV)"
    echo "--- mmap_cold"
    as_user strace -c -e trace=openat,read,mmap,munmap,close "$BENCH" "$DEV" "$PAGES" 2 mmap_cold 2>&1 | grep -vE "^(Objetivo|modo|mmap_cold|CSV)"
    echo ""; echo "### 5. strace -T: costo de la llamada mmap() en si (alta del VMA)"
    echo "--- A) archivo normal"
    as_user strace -T -e trace=mmap "$BENCH" "$BASE" 16 2 mmap_cold 2>&1 | grep "PROT_READ, MAP_SHARED, 3" | tail -2
    echo "--- B) /dev/secure_mmap (incluye la verificacion de politica del modulo)"
    as_user strace -T -e trace=mmap "$BENCH" "$DEV" 16 2 mmap_cold 2>&1 | grep "PROT_READ, MAP_SHARED, 3" | tail -2
fi

TR=/sys/kernel/tracing
if [ -w "$TR/current_tracer" ] && echo smm_fault > "$TR/set_graph_function" 2>/dev/null; then
    echo ""; echo "### 6. ftrace function_graph de smm_fault (desglose de un page fault)"
    echo 2 > "$TR/max_graph_depth"
    echo function_graph > "$TR/current_tracer"
    echo > "$TR/trace"
    echo 1 > "$TR/tracing_on"
    as_user "$BENCH" "$DEV" 8 2 mmap_cold > /dev/null
    echo 0 > "$TR/tracing_on"
    grep -v "^#" "$TR/trace" | head -40
    echo nop > "$TR/current_tracer"
    echo > "$TR/set_graph_function"
    echo 0 > "$TR/max_graph_depth"
else
    echo "(ftrace no disponible para smm_fault en este kernel)"
fi

rm -f "$BASE"
chown -R "$REAL_USER" "$RESULTS"
echo ""
echo "Resultados guardados en: $LOG"
