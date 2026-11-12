#!/bin/bash
# ==============================================================================
# run_attacks.sh - Bateria de ataques controlados contra /dev/secure_mmap
#
#   sudo ./scripts/run_attacks.sh
#
# Cada prueba indica el resultado ESPERADO. Una prueba "PASA" cuando el
# resultado real coincide con el esperado (normalmente DEFENDIDO). La prueba
# T04 espera VULNERABLE a proposito: demuestra que una defensa LD_PRELOAD en
# espacio de usuario se evade con la syscall directa.
# El log completo queda en results/attacks_<fecha>.log
# ==============================================================================
source "$(dirname "$0")/common.sh"
need_root

mkdir -p "$RESULTS"
LOG="$RESULTS/attacks_$(date +%Y%m%d_%H%M%S).log"
exec > >(tee "$LOG") 2>&1

[ -f "$KO" ] && [ -x "$HERE/build/smm_attacks" ] || { echo "Primero compile con: make"; exit 1; }

# Los binarios se copian a /tmp para que "nobody" pueda ejecutarlos
# (en Ubuntu 24.04 los /home son 750).
T=/tmp/smm_test
install -d -m 755 "$T"
install -m 755 "$HERE/build/smm_attacks" "$HERE/build/guard_preload.so" "$T/"
BIN="$T/smm_attacks"
GUARD="$T/guard_preload.so"

# Archivo REAL "secreto.txt" (vigilado por el kprobe) del usuario autorizado.
for i in $(seq 1 200); do echo "SECRETO REAL linea $i: clave=EAFIT-SO-2026"; done > "$T/secreto.txt"
chown "$REAL_USER" "$T/secreto.txt"; chmod 644 "$T/secreto.txt"
echo "archivo de root solo lectura para la pregunta 8" > "$T/q8.txt"; chmod 644 "$T/q8.txt"

echo "=============================================================================="
echo " secure_mmap - bateria de ataques   $(date)"
echo " kernel: $(uname -r) | usuario autorizado: $REAL_USER (uid $REAL_UID) | atacante: nobody"
echo "=============================================================================="
load_module allow_write=0
echo reset > "$PROC"

PASS=0; FAIL=0; SUMMARY=()

# run_test ID "descripcion" esperado(0=DEFENDIDO,1=VULNERABLE) comando...
run_test() {
    local id="$1" desc="$2" expect="$3"; shift 3
    echo ""
    echo "${C_INFO}[$id] $desc${C_RST}"
    local out rc
    out="$("$@" 2>&1)"; rc=$?
    echo "$out" | sed 's/^/      /'
    if [ "$rc" -eq "$expect" ]; then
        PASS=$((PASS+1)); SUMMARY+=("${C_OK}PASA${C_RST}  $id  $desc")
    else
        FAIL=$((FAIL+1)); SUMMARY+=("${C_BAD}FALLA${C_RST} $id  $desc (exit=$rc, esperado=$expect)")
    fi
}

run_test T01 "Uso legitimo: el usuario autorizado mapea y lee" 0 \
    as_user "$BIN" legit_read 4
run_test T02 "nobody intenta mmap() de la boveda" 0 \
    as_nobody "$BIN" unauth_mmap
run_test T03 "nobody intenta read() de la boveda" 0 \
    as_nobody "$BIN" unauth_read
run_test T04 "Defensa LD_PRELOAD (usuario) sobre archivo normal: syscall directa la evade (se ESPERA VULNERABLE)" 1 \
    as_nobody env LD_PRELOAD="$GUARD" "$BIN" raw_bypass "$T/secreto.txt"
run_test T05 "Misma evasion LD_PRELOAD contra /dev/secure_mmap: el kernel la detiene" 0 \
    as_nobody env LD_PRELOAD="$GUARD" "$BIN" raw_bypass "$DEV"
run_test T06 "MAP_SHARED|PROT_WRITE con allow_write=0" 0 \
    as_user "$BIN" shared_write
run_test T07 "MAP_PRIVATE|PROT_WRITE (Copy-On-Write)" 0 \
    as_user "$BIN" private_write
run_test T08 "PROT_EXEC (inyeccion de codigo, W^X)" 0 \
    as_user "$BIN" exec_map
run_test T09 "Escalada RO -> RW con mprotect()" 0 \
    as_user "$BIN" mprotect_write
run_test T10 "Escalada RO -> RX con mprotect()" 0 \
    as_user "$BIN" mprotect_exec
run_test T11 "Estilo Dirty COW: /proc/self/mem + madvise(MADV_DONTNEED)" 0 \
    as_user "$BIN" dirtycow
run_test T12 "Otro proceso del mismo usuario lee el texto plano (process_vm_readv, /proc/pid/mem)" 0 \
    as_user "$BIN" remote_read
run_test T13 "Heredar el mapeo descifrado con fork()" 0 \
    as_user "$BIN" fork_inherit
run_test T14 "Partir (munmap/mprotect parcial) o crecer (mremap) el VMA" 0 \
    as_user "$BIN" split

echo ""; echo "${C_INFO}[T15] Manipulacion del texto cifrado (root: echo \"corrupt 3\" > $PROC)${C_RST}"
echo "corrupt 3" > "$PROC"
run_test T15 "Leer una pagina manipulada debe dar SIGBUS (integridad CRC32)" 0 \
    as_user "$BIN" integrity 3
echo "corrupt 3" > "$PROC"   # XOR de nuevo: restaura la pagina

set_param allow_write 1
run_test T16 "Carrera madvise(MADV_DONTNEED) vs escrituras en MAP_SHARED (allow_write=1)" 0 \
    as_user "$BIN" madvise_race
set_param allow_write 0

kprobe_check() {
    as_user "$BIN" map_file "$T/secreto.txt" rw || return 2
    if grep -q "ALERTA kprobe" "$PROC"; then
        echo "Bitacora del kernel:"; grep "ALERTA kprobe" "$PROC" | tail -1
        echo "RESULTADO: DEFENDIDO - el kprobe en security_mmap_file audito el mmap RW del archivo real"
        return 0
    fi
    echo "RESULTADO: VULNERABLE - el mmap del archivo real no quedo auditado"; return 1
}
run_test T17 "Auditoria de archivo REAL: mmap(RW, MAP_SHARED) de secreto.txt (kprobe)" 0 kprobe_check

run_test T18 "Pregunta 8: O_RDONLY + PROT_WRITE + MAP_PRIVATE sobre archivo normal (informativo)" 0 \
    as_user "$BIN" q8 "$T/q8.txt"

echo ""
echo "=============================================================================="
echo " RESUMEN"
echo "=============================================================================="
for l in "${SUMMARY[@]}"; do echo "  $l"; done
echo ""
echo "  Pruebas que pasan: $PASS / $((PASS+FAIL))"
echo ""
echo "=============================== $PROC ==============================="
cat "$PROC"
echo ""
echo "=============================== dmesg (ultimas lineas) ==============================="
dmesg | grep secure_mmap | tail -25
chown -R "$REAL_USER" "$RESULTS"
echo ""
echo "Log guardado en: $LOG"
[ "$FAIL" -eq 0 ]
