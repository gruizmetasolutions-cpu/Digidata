#!/usr/bin/env bash
# ==============================================================================
# Script de Ejecución del Servidor de Telemetría K-2SO en Entorno Local
# ==============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
VENV_DIR="$ROOT_DIR/.venv"

cd "$ROOT_DIR"

if [ ! -d "$VENV_DIR" ]; then
    echo "⚠️  Entorno virtual no encontrado en $VENV_DIR. Creándolo..."
    uv venv "$VENV_DIR"
    "$VENV_DIR/bin/pip" install -r "$ROOT_DIR/server/requirements.txt"
fi

echo "🚀 Iniciando K-2SO Metrics Daemon en http://0.0.0.0:8000..."
echo "💡 Presiona Ctrl+C para detener el servidor."
exec "$VENV_DIR/bin/python3" "$ROOT_DIR/server/k2so_metrics_daemon.py"
