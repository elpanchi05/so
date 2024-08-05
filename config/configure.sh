#!/bin/bash

# Verificar los parámetros
if [ "$#" -lt 2 ]; then
  echo "Uso: $0 <CLAVE> <VALOR> [<ARCHIVO_O_DIRECTORIO>]"
  exit 1
fi

CLAVE=${1:?}
VALOR=${2:?}
ARCHIVO_O_DIRECTORIO=${3:-.}  # Si no se proporciona, usar el directorio actual

# Imprimir mensaje de inicio
#echo -e "\nModificando archivos de configuración en '$ARCHIVO_O_DIRECTORIO'...\n"

# Buscar y reemplazar en los archivos de configuración
grep -Rl "$CLAVE" "$ARCHIVO_O_DIRECTORIO" \
  | grep -E '\.config$|\.cfg$' \
  | tee >(xargs -n1 sed -i "s|^\($CLAVE\s*=\).*|\1$VALOR|")

# Imprimir mensaje de finalización
#echo -e "\nLos archivos de configuración han sido modificados correctamente en '$ARCHIVO_O_DIRECTORIO'.\n"

