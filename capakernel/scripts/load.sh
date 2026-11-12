#!/bin/bash
# load.sh - carga secure_mmap con allowed_uid = usuario que invoco sudo
#   sudo ./scripts/load.sh [allow_write=1] [vault_pages=4096] [verbose=1] [watch_name=archivo]
source "$(dirname "$0")/common.sh"
need_root
[ -f "$KO" ] || { echo "Primero compile con: make"; exit 1; }
load_module "$@"
echo "secure_mmap cargado: $DEV (allowed_uid=$REAL_UID -> $REAL_USER) $*"
echo "Estado: cat $PROC   |   Bitacora: sudo dmesg | grep secure_mmap"
