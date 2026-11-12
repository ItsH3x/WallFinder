#!/usr/bin/env bash
# ==============================================================================
# Script de Ejecución y Compilación Automática para 'Buscador de Fondos de Pantalla'
# ==============================================================================

set -e

# Determinar el directorio raíz del proyecto
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build"
BINARY="${BUILD_DIR}/wallpaper_finder"

# Verificar si el binario existe y tiene permisos de ejecución
if [ ! -x "${BINARY}" ]; then
    echo "============================================================"
    echo " [!] Binario '${BINARY}' no encontrado o no ejecutable."
    echo " [*] Iniciando compilación automática del proyecto..."
    echo "============================================================"

    # Verificar que cmake esté disponible
    if ! command -v cmake >/dev/null 2>&1; then
        echo " [ERROR] 'cmake' no está instalado o no se encuentra en el PATH." >&2
        echo " Instale las dependencias necesarias antes de continuar." >&2
        exit 1
    fi

    mkdir -p "${BUILD_DIR}"
    cmake -B "${BUILD_DIR}" -S "${PROJECT_ROOT}" -DCMAKE_BUILD_TYPE=Release
    
    # Detectar cantidad de núcleos de CPU para compilación en paralelo
    NPROCS=$(nproc 2>/dev/null || getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)
    cmake --build "${BUILD_DIR}" --target wallpaper_finder -j"${NPROCS}"

    if [ ! -x "${BINARY}" ]; then
        echo " [ERROR] La compilación finalizó pero no se generó el binario ejecutable." >&2
        exit 1
    fi

    echo "============================================================"
    echo " [OK] Compilación exitosa. Iniciando aplicación..."
    echo "============================================================"
fi

# Ejecutar el binario pasando todos los argumentos recibidos
exec "${BINARY}" "$@"
