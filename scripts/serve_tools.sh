#!/usr/bin/env bash
# ==============================================================================
# Servidor HTTP Local para Simuladores Web y Web Studios de DigiVice
# ==============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
PORT="${PORT:-3000}"

cd "$ROOT_DIR"

echo "🎨 Dataholics DigiVice — Servidor de Herramientas y Simuladores Web"
echo "─────────────────────────────────────────────────────────────────"
echo "🌐 Servidor escuchando en: http://localhost:$PORT"
echo ""
echo "📱 Enlaces a Simuladores Disponibles:"
echo "   🐾 Scrap-Hound 16-Bit Studio:          http://localhost:$PORT/tools/scraphound_studio.html"
echo "   🤖 Cyberpunk LVGL 9 Mecha HUD:         http://localhost:$PORT/tools/digivice_simulator.html"
echo "   🪪 Executive Conference Badge (Alonso): http://localhost:$PORT/tools/conference_badge_simulator.html"
echo "   👁️ DigiVice Master Viewer Studio:      http://localhost:$PORT/viewer/index.html"
echo "─────────────────────────────────────────────────────────────────"
echo "💡 Presiona Ctrl+C para detener."

exec python3 -m http.server "$PORT"
