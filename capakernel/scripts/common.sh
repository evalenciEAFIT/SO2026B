#!/bin/bash
# common.sh - utilidades compartidas por los scripts (se incluye con "source")
set -u
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
KO="$HERE/secure_mmap.ko"
DEV=/dev/secure_mmap
PROC=/proc/secure_mmap
REAL_USER="${SUDO_USER:-$(id -un)}"
REAL_UID="$(id -u "$REAL_USER")"
RESULTS="$HERE/results"

C_OK=$'\e[32m'; C_BAD=$'\e[31m'; C_INFO=$'\e[36m'; C_RST=$'\e[0m'

need_root() {
    if [ "$(id -u)" -ne 0 ]; then
        echo "Este script requiere sudo:  sudo $0 $*"; exit 1
    fi
}

as_user()   { sudo -u "$REAL_USER" "$@"; }
as_nobody() { sudo -u nobody "$@"; }

module_loaded() { grep -q "^secure_mmap " /proc/modules; }

load_module() {  # load_module [parametros extra...]
    module_loaded && rmmod secure_mmap
    insmod "$KO" allowed_uid="$REAL_UID" "$@" || { echo "insmod fallo"; exit 1; }
    chmod 666 "$DEV"   # a proposito: la proteccion NO depende de los permisos POSIX
}

set_param() { echo "$2" > "/sys/module/secure_mmap/parameters/$1"; }
