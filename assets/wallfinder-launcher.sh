#!/usr/bin/env bash
# ==============================================================================
# Lanzador seguro para WallFinder (KDE Plasma, GNOME, etc.)
# ==============================================================================
APP_DIR="/home/hex/Buscador de Fondos de Pantalla"
BIN="${APP_DIR}/build/wallpaper_finder"

cd "${APP_DIR}" || exit 1

if [ ! -x "${BIN}" ]; then
    bash "${APP_DIR}/run.sh" "$@"
else
    exec "${BIN}" "$@"
fi

