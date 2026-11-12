#!/bin/bash
# ==============================================================================
# run_baseline.sh - Los MISMOS ataques contra el driver del Paso 6 del profesor
#                   (kmod_demand_paging: mmap + .fault SIN medidas de seguridad)
#
#   sudo ./scripts/run_baseline.sh [ruta/al/kmod_demand_paging.ko]
#
# Sirve como grupo de control: muestra que los ataques SI funcionan contra un
# driver mmap ingenuo, y por lo tanto que es secure_mmap quien los detiene.
# ==============================================================================
source "$(dirname "$0")/common.sh"
need_root

USER_HOME="$(getent passwd "$REAL_USER" | cut -d: -f6)"
P6_DIR="$USER_HOME/SO2026B/USOS/CapaKernel/paso6_demand_paging_fault"
P6_KO="${1:-$P6_DIR/kmod_demand_paging.ko}"
P6_DEV=/dev/demo_demand_fault

if [ ! -f "$P6_KO" ]; then
    echo "No encuentro $P6_KO"
    echo "Clone el repo del profesor en ~/SO2026B y compile el paso 6:"
    echo "  cd ~ && git clone https://github.com/evalenciEAFIT/SO2026B.git"
    echo "  make -C ~/SO2026B/USOS/CapaKernel/paso6_demand_paging_fault"
    exit 1
fi

mkdir -p "$RESULTS"
LOG="$RESULTS/baseline_$(date +%Y%m%d_%H%M%S).log"
exec > >(tee "$LOG") 2>&1

T=/tmp/smm_test
install -d -m 755 "$T"
install -m 755 "$HERE/build/smm_attacks" "$T/"
BIN="$T/smm_attacks"

grep -q "^kmod_demand_paging " /proc/modules && rmmod kmod_demand_paging
insmod "$P6_KO" || exit 1
chmod 666 "$P6_DEV"
export SMM_DEV="$P6_DEV"

echo "=============================================================================="
echo " LINEA BASE (sin defensa): ataques contra $P6_DEV   $(date)"
echo "=============================================================================="
VULN=0; TOTAL=0; SUMMARY=()
run_attack() {
    local id="$1" desc="$2"; shift 2
    echo ""; echo "${C_INFO}[$id] $desc${C_RST}"
    local out rc
    out="$("$@" 2>&1)"; rc=$?
    echo "$out" | sed 's/^/      /'
    TOTAL=$((TOTAL+1))
    if [ "$rc" -eq 1 ]; then VULN=$((VULN+1)); SUMMARY+=("${C_BAD}ATAQUE EXITOSO${C_RST}   $id $desc")
    elif [ "$rc" -eq 0 ]; then SUMMARY+=("${C_OK}bloqueado${C_RST}        $id $desc")
    else SUMMARY+=("no concluyente   $id $desc"); fi
}

run_attack B02 "nobody mmap()"                    as_nobody env SMM_DEV="$P6_DEV" "$BIN" unauth_mmap
run_attack B07 "MAP_PRIVATE|PROT_WRITE (COW)"     as_user env SMM_DEV="$P6_DEV" "$BIN" private_write
run_attack B08 "PROT_EXEC"                        as_user env SMM_DEV="$P6_DEV" "$BIN" exec_map
run_attack B09 "mprotect RO -> RW"                as_user env SMM_DEV="$P6_DEV" "$BIN" mprotect_write
run_attack B10 "mprotect RO -> RX"                as_user env SMM_DEV="$P6_DEV" "$BIN" mprotect_exec
run_attack B11 "Dirty COW (/proc/self/mem)"       as_user env SMM_DEV="$P6_DEV" "$BIN" dirtycow
run_attack B12 "process_vm_readv desde otro proceso" as_user env SMM_DEV="$P6_DEV" "$BIN" remote_read
run_attack B13 "herencia por fork()"              as_user env SMM_DEV="$P6_DEV" "$BIN" fork_inherit
run_attack B14 "partir/crecer el VMA"             as_user env SMM_DEV="$P6_DEV" "$BIN" split

rmmod kmod_demand_paging
echo ""
echo "=============================================================================="
for l in "${SUMMARY[@]}"; do echo "  $l"; done
echo ""
echo "  Ataques exitosos SIN secure_mmap: $VULN / $TOTAL"
echo "  (compare con: sudo ./scripts/run_attacks.sh)"
chown -R "$REAL_USER" "$RESULTS"
echo "Log guardado en: $LOG"
