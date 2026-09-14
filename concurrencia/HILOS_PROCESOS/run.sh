#!/bin/bash

echo "Limpiando compilaciones anteriores..."
make clean

echo -e "\nCompilando el proyecto..."
make

if [ $? -eq 0 ]; then
    echo -e "\n================================================"
    echo -e "  INICIANDO MENÚ INTERACTIVO"
    echo -e "================================================"
    ./main 10 10 1
else
    echo -e "\nError en la compilación. Revisa el código."
    exit 1
fi
