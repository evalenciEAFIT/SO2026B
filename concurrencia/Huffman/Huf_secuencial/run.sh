#!/usr/bin/env bash
# ==============================================================================
# Script de Ejecución y Demostración Automatizada del Compresor de Huffman
# Asignatura: Sistemas Operativos (4to Semestre)
# ==============================================================================

set -e

# Definición de Colores ANSI
VERDE="\033[1;32m"
AZUL="\033[1;34m"
CIAN="\033[1;36m"
AMARILLO="\033[1;33m"
ROJO="\033[1;31m"
BLANCO="\033[1;37m"
NEGRITA="\033[1m"
ATENUADO="\033[2m"
RESET="\033[0m"

imprimir_separador() {
    echo -e "${CIAN}────────────────────────────────────────────────────────────────────────${RESET}"
}

echo -e "\n${CIAN}${NEGRITA}╔══════════════════════════════════════════════════════════════════════╗${RESET}"
echo -e "${CIAN}${NEGRITA}║       DEMOSTRACIÓN INTERACTIVA: COMPRESOR HUFFMAN EN LENGUAJE C      ║${RESET}"
echo -e "${CIAN}${ATENUADO}║              Docencia de Sistemas Operativos - Concurrencia          ║${RESET}"
echo -e "${CIAN}${NEGRITA}╚══════════════════════════════════════════════════════════════════════╝${RESET}\n"

# 1. Compilación
echo -e "${AMARILLO}${NEGRITA}[ETAPA 1] Compilación del Proyecto con GCC...${RESET}"
make clean > /dev/null 2>&1 || true
make
echo -e "${VERDE}✔ Compilación exitosa del ejecutable './huffman'.${RESET}\n"

# 2. Generación del archivo de prueba
ARCHIVO_ORIGEN="documento_prueba.txt"
DIR_SALIDAS="salidas"
ARCHIVO_COMPRIMIDO="${DIR_SALIDAS}/documento_prueba.huf"
ARCHIVO_RESTAURADO="${DIR_SALIDAS}/documento_restaurado.txt"
ARCHIVO_REGISTRO="${DIR_SALIDAS}/registro_salidas.txt"

echo -e "${AMARILLO}${NEGRITA}[ETAPA 2] Generando archivo de prueba con texto didáctico...${RESET}"
cat << 'EOF' > "$ARCHIVO_ORIGEN"
================================================================================
SISTEMAS OPERATIVOS: FUNDAMENTOS DE CONCURRENCIA, ARCHIVOS Y MEMORIA VIRTUAL
Asignatura: Sistemas Operativos (4to Semestre)
================================================================================

Un sistema operativo es el software fundamental que administra los recursos de
hardware y proporciona una interfaz abstracta para las aplicaciones de usuario.

Conceptos Clave de la Asignatura:
1. GESTIÓN DE PROCESOS E HILOS:
   - Planificación de CPU (Shortest Job First, Round Robin, Prioridades).
   - Cambio de contexto y estructuras de control de procesos (PCB).
   - Estados de un proceso: Listo, Ejecutando, Bloqueado y Terminado.

2. CONCURRENCIA Y SINCRONIZACIÓN:
   - Condiciones de carrera y exclusión mutua.
   - Mecanismos de sincronización: Semáforos, Mutexes, Variables de Condición.
   - Prevención y detección de bloqueos mutuos (Deadlocks).

3. JERARQUÍA DE MEMORIA Y MEMORIA VIRTUAL:
   - Espacio de direcciones virtuales: Segmento de Código, Datos, Heap y Stack.
   - Paginación y segmentación, tablas de páginas multinivel y TLB.
   - Asignación dinámica con malloc() y liberación con free() para evitar fugas.

4. SISTEMAS DE ARCHIVOS Y E/S:
   - Descriptores de archivo, tablas de archivos abiertos y llamadas al sistema.
   - Bloques en disco, inodos y asignación continua, enlazada e indexada.
   - Técnicas de compresión como el Algoritmo de Huffman para optimizar espacio.

La codificación de Huffman es un algoritmo codicioso (greedy) que asigna cadenas
de bits más cortas a los caracteres más frecuentes, optimizando el ancho de banda
y el espacio de almacenamiento secundario.
================================================================================
EOF

TAM_ORIGINAL=$(wc -c < "$ARCHIVO_ORIGEN")
echo -e "${VERDE}✔ Archivo '${ARCHIVO_ORIGEN}' creado (${TAM_ORIGINAL} bytes).${RESET}\n"

# 3. Compresión
echo -e "${AMARILLO}${NEGRITA}[ETAPA 3] Ejecutando Compresión hacia carpeta '${DIR_SALIDAS}/'...${RESET}"
echo -e "${ATENUADO}Comando: ./huffman -c ${ARCHIVO_ORIGEN} ${ARCHIVO_COMPRIMIDO}${RESET}"
./huffman -c "$ARCHIVO_ORIGEN" "$ARCHIVO_COMPRIMIDO"

# 4. Descompresión
echo -e "${AMARILLO}${NEGRITA}[ETAPA 4] Ejecutando Descompresión hacia carpeta '${DIR_SALIDAS}/'...${RESET}"
echo -e "${ATENUADO}Comando: ./huffman -d ${ARCHIVO_COMPRIMIDO} ${ARCHIVO_RESTAURADO}${RESET}"
./huffman -d "$ARCHIVO_COMPRIMIDO" "$ARCHIVO_RESTAURADO"

# 5. Verificación de Integridad
imprimir_separador
echo -e "${AMARILLO}${NEGRITA}[ETAPA 5] Verificando Integridad de Datos (Byte a Byte con 'diff')...${RESET}"
if diff -s "$ARCHIVO_ORIGEN" "$ARCHIVO_RESTAURADO" > /dev/null; then
    echo -e "${VERDE}${NEGRITA}✔ ¡ÉXITO TOTAL! El archivo original y el archivo recuperado son 100% idénticos.${RESET}"
else
    echo -e "${ROJO}${NEGRITA}✘ ERROR: Existen diferencias entre el archivo original y el restaurado.${RESET}"
    exit 1
fi
imprimir_separador

# 6. Comparación de tamaños en disco
TAM_COMPRIMIDO=$(wc -c < "$ARCHIVO_COMPRIMIDO")
AHORRO=$(awk "BEGIN {printf \"%.2f\", (1.0 - ($TAM_COMPRIMIDO / $TAM_ORIGINAL)) * 100}")

echo -e "\n${BLANCO}${NEGRITA}📊 COMPARATIVA DE ALMACENAMIENTO:${RESET}"
echo -e "  • Tamaño Original:     ${BLANCO}${TAM_ORIGINAL}${RESET} bytes"
echo -e "  • Tamaño Comprimido:   ${CIAN}${TAM_COMPRIMIDO}${RESET} bytes"
echo -e "  • Reducción Lograda:   ${VERDE}${AHORRO}%${RESET} de ahorro en almacenamiento secundario"

echo -e "\n${BLANCO}${NEGRITA}Archivos organizados en la carpeta de salida ('${DIR_SALIDAS}/'):${RESET}"
ls -lh "$DIR_SALIDAS"

# 7. Visualización del archivo de registro / bitácora de salidas
echo -e "\n${AMARILLO}${NEGRITA}[ETAPA 6] Contenido del archivo de bitácora generado ('${ARCHIVO_REGISTRO}'):${RESET}"
imprimir_separador
cat "$ARCHIVO_REGISTRO"
imprimir_separador

echo -e "\n${VERDE}${NEGRITA}💡 Tip para los estudiantes:${RESET}"
echo -e "   Puedes comprimir cualquier archivo guardándolo en la carpeta de salidas:"
echo -e "   ${BLANCO}./huffman -c <tu_archivo> salidas/<nombre>.huf${RESET}"
echo -e "   Y luego recuperarlo con:"
echo -e "   ${BLANCO}./huffman -d salidas/<nombre>.huf salidas/<recuperado>.txt${RESET}\n"
