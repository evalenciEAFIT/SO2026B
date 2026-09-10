#!/bin/bash

echo "================================================="
echo "       DESPLIEGUE EN GITHUB (BACKUP)           "
echo "================================================="

# Directorio base del repositorio git
GIT_DIR="/home/edi/DOCENCIA2026/SO2026B"
# Directorio del proyecto actual
PROYECTO_DIR="/home/edi/DOCENCIA2026/SO2026B/concurrencia/concurrencia_ludica/reproductor_MP3_proceso"

# Cambiamos al directorio del proyecto para limpiar primero
cd "$PROYECTO_DIR" || exit 1

echo "[1] Limpiando archivos binarios y temporales (make clean)..."
make clean &> /dev/null

echo "[2] Preparando archivos del proyecto para Git..."
# Cambiamos a la raíz del repositorio para ejecutar comandos Git de forma segura
cd "$GIT_DIR" || exit 1

# Asegurarse de estar en la rama main y actualizar con seguridad (rebase y autostash)
echo "[3] Sincronizando con el repositorio remoto..."
git checkout main
git pull origin main --rebase --autostash

# Agregamos estrictamente los archivos de la carpeta del proyecto
echo "[4] Añadiendo cambios del reproductor al Staging Area..."
git add "$PROYECTO_DIR/*.c"
git add "$PROYECTO_DIR/*.h" 2>/dev/null
git add "$PROYECTO_DIR/*.sh"
git add "$PROYECTO_DIR/*.md"
git add "$PROYECTO_DIR/Makefile"

# Verificamos si hay cambios reales para comitear
if git diff --cached --quiet; then
    echo "  -> No hay cambios nuevos para respaldar en este proyecto."
else
    FECHA=$(date +"%Y-%m-%d %H:%M:%S")
    MENSAJE="Respaldo automático del Reproductor MP3 Modular (Concurrencia): $FECHA"
    
    echo "[5] Creando Commit: '$MENSAJE'"
    git commit -m "$MENSAJE"
    
    echo "[6] Subiendo a GitHub (Push)..."
    git push origin main
    
    if [ $? -eq 0 ]; then
        echo "================================================="
        echo " ¡Despliegue Exitoso!"
        echo " Puedes revisar tu código en:"
        echo " https://github.com/evalenciEAFIT/SO2026B/tree/main/concurrencia/concurrencia_ludica/reproductor_MP3_proceso"
        echo "================================================="
    else
        echo " Error al subir al repositorio remoto. Verifica tu conexión o permisos."
    fi
fi
