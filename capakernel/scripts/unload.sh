#!/bin/bash
# unload.sh - descarga secure_mmap
source "$(dirname "$0")/common.sh"
need_root
if module_loaded; then rmmod secure_mmap && echo "secure_mmap descargado"; else echo "secure_mmap no estaba cargado"; fi
