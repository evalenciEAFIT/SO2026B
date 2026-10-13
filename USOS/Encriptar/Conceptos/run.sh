#!/usr/bin/env bash
# ==============================================================================
# Script: run.sh
# Propósito: Compilar, ejecutar de forma interactiva o por línea de comandos
#            la suite de cifrado orientada a System Calls en Linux.
# ==============================================================================

set -e

# Directorio del script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

BIN="./file_cipher"

# Colores para la terminal
COLOR_RESET="\033[0m"
COLOR_BOLD="\033[1m"
COLOR_CYAN="\033[36m"
COLOR_GREEN="\033[32m"
COLOR_YELLOW="\033[33m"
COLOR_RED="\033[31m"
COLOR_BLUE="\033[34m"

# Función para compilar si el binario no existe o si se solicita
compilar() {
    if [[ ! -x "$BIN" ]]; then
        echo -e "${COLOR_YELLOW}[INFO] El binario $BIN no existe. Compilando con make...${COLOR_RESET}"
        make
        echo -e "${COLOR_GREEN}[OK] Compilación completada.${COLOR_RESET}\n"
    fi
}

mostrar_banner() {
    echo -e "${COLOR_CYAN}${COLOR_BOLD}"
    echo "======================================================================"
    echo "   LANZADOR DE LA SUITE DE CIFRADO DE ARCHIVOS - SISTEMAS OPERATIVOS  "
    echo "                 Llamadas al Sistema (System Calls POSIX)             "
    echo "======================================================================"
    echo -e "${COLOR_RESET}"
}

menu_interactivo() {
    compilar

    while true; do
        mostrar_banner
        echo -e "${COLOR_BOLD}Seleccione una opción:${COLOR_RESET}"
        echo "  1) Cifrado César Extendido (Byte-Shift)"
        echo "  2) Cifrado XOR Polialfabético (Stream con clave repetida)"
        echo "  3) Cifrado de Flujo RC4 (Rivest Cipher 4)"
        echo "  4) Cifrado por Transposición Columnar por Bloques"
        echo "  5) Cifrado en Memoria Virtual vía mmap(2) (Copia con Zero-Copy)"
        echo "  6) Cifrado en Memoria Virtual vía mmap(2) (In-Situ sobre el mismo archivo)"
        echo "  7) Ejecutar Suite Completa de Pruebas Automatizadas (make test)"
        echo "  8) Crear archivo de prueba rápido (sample_demo.txt)"
        echo "  9) Limpiar archivos temporales y binarios (make clean)"
        echo "  0) Salir"
        echo ""
        read -rp "Ingrese su opción [0-9]: " opcion

        case "$opcion" in
            1|2|3|4|5|6)
                # Determinar algoritmo
                local algo=""
                local es_mmap_inplace=false
                case "$opcion" in
                    1) algo="caesar" ;;
                    2) algo="xor" ;;
                    3) algo="rc4" ;;
                    4) algo="transp" ;;
                    5) algo="mmap" ;;
                    6) algo="mmap"; es_mmap_inplace=true ;;
                esac

                echo -e "\n${COLOR_CYAN}--- Configuración de Operación: $algo ---${COLOR_RESET}"
                read -rp "¿Desea Cifrar (e) o Descifrar (d)? [e/d]: " modo_in
                local modo_flag="-e"
                if [[ "$modo_in" =~ ^[dD]$ ]]; then
                    modo_flag="-d"
                fi

                read -rp "Ruta del archivo de entrada: " file_in
                if [[ ! -f "$file_in" ]]; then
                    echo -e "${COLOR_RED}[ERROR] El archivo '$file_in' no existe.${COLOR_RESET}"
                    sleep 1.5
                    continue
                fi

                local file_out=""
                if [[ "$es_mmap_inplace" == false ]]; then
                    read -rp "Ruta del archivo de salida: " file_out
                    if [[ -z "$file_out" ]]; then
                        echo -e "${COLOR_RED}[ERROR] Debe indicar un archivo de salida.${COLOR_RESET}"
                        sleep 1.5
                        continue
                    fi
                fi

                local prompt_clave="Clave de cifrado: "
                if [[ "$algo" == "caesar" ]]; then
                    prompt_clave="Desplazamiento numérico (ej. 15): "
                fi
                read -rp "$prompt_clave" key_val
                if [[ -z "$key_val" ]]; then
                    echo -e "${COLOR_RED}[ERROR] La clave no puede estar vacía.${COLOR_RESET}"
                    sleep 1.5
                    continue
                fi

                read -rp "¿Activar métricas de Benchmark (-b) y Metadatos de SO (-s)? [s/N]: " extras
                local extra_flags=""
                if [[ "$extras" =~ ^[sSyY]$ ]]; then
                    extra_flags="-b -s"
                fi

                echo -e "\n${COLOR_GREEN}[EJECUTANDO]...${COLOR_RESET}"
                if [[ "$es_mmap_inplace" == true ]]; then
                    "$BIN" -a "$algo" "$modo_flag" -i "$file_in" -k "$key_val" $extra_flags
                else
                    "$BIN" -a "$algo" "$modo_flag" -i "$file_in" -o "$file_out" -k "$key_val" $extra_flags
                fi
                echo ""
                read -rp "Presione Enter para continuar..."
                ;;

            7)
                echo -e "\n${COLOR_BLUE}[INFO] Ejecutando suite de pruebas automatizadas...${COLOR_RESET}\n"
                make test
                echo ""
                read -rp "Presione Enter para continuar..."
                ;;

            8)
                mkdir -p tests_out
                echo -e "Demostración de Cifrado y Llamadas al Sistema (Linux 2026B)\nUsuario: $(whoami)\nFecha: $(date)" > tests_out/sample_demo.txt
                echo -e "${COLOR_GREEN}[OK] Archivo creado en: tests_out/sample_demo.txt${COLOR_RESET}"
                ls -l tests_out/sample_demo.txt
                echo ""
                read -rp "Presione Enter para continuar..."
                ;;

            9)
                echo -e "\n${COLOR_YELLOW}[INFO] Limpiando artefactos...${COLOR_RESET}\n"
                make clean
                echo ""
                read -rp "Presione Enter para continuar..."
                ;;

            0)
                echo -e "\n${COLOR_GREEN}Saliendo. ¡Hasta luego!${COLOR_RESET}"
                exit 0
                ;;

            *)
                echo -e "${COLOR_RED}[AVISO] Opción inválida.${COLOR_RESET}"
                sleep 1
                ;;
        esac
    done
}

# --- Punto de entrada del script ---
if [[ $# -eq 0 ]]; then
    # Sin argumentos: lanzar menú interactivo
    menu_interactivo
elif [[ "$1" == "test" ]]; then
    make test
elif [[ "$1" == "clean" ]]; then
    make clean
elif [[ "$1" == "help" || "$1" == "-h" || "$1" == "--help" ]]; then
    compilar
    "$BIN" -h
else
    # Si se pasan parámetros directos para el ejecutable (ej. ./run.sh -a caesar -e ...)
    compilar
    "$BIN" "$@"
fi
