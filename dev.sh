#!/usr/bin/env bash
# ==============================================================================
# Dataholics DigiVice — Task Runner & CLI de Desarrollo Ágil
# ==============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

cmd="${1:-help}"

case "$cmd" in
    setup)
        echo "🚀 [1/3] Verificando entorno virtual Python..."
        if command -v uv >/dev/null 2>&1; then
            uv venv .venv
            uv pip install -r server/requirements.txt
        else
            python3 -m venv .venv
            .venv/bin/pip install -r server/requirements.txt
        fi
        echo "🔧 [2/3] Asignando permisos de ejecución a scripts..."
        chmod +x scripts/*.sh dev.sh
        echo "🔄 [3/3] Sincronizando con upstream (DEV-Dataholics/DigiVice)..."
        ./scripts/sync_upstream.sh
        echo "✅ ¡Entorno configurado y listo para trabajar!"
        ;;
    sync)
        ./scripts/sync_upstream.sh
        ;;
    server)
        ./scripts/run_server.sh
        ;;
    tools)
        ./scripts/serve_tools.sh
        ;;
    check)
        echo "🔍 Verificando integridad de módulos de servidor Python..."
        .venv/bin/python3 -c "import server.k2so_metrics_daemon, server.k2so_mission_state, server.nemik_hitm_bridge; print('✅ Todos los módulos Python cargaron sin errores.')"
        ;;
    status)
        echo "📊 Estado del Repositorio Git:"
        git status -s
        echo ""
        echo "🔗 Remotes Configurados:"
        git remote -v
        echo ""
        echo "🌿 Ramas Locales y Remotas:"
        git branch -a
        ;;
    clean)
        find . -type d -name "__pycache__" -exec rm -rf {} + 2>/dev/null || true
        find . -type f -name "*.pyc" -delete 2>/dev/null || true
        echo "🧹 Limpieza completada."
        ;;
    help|*)
        echo "🛠️ Comandos de Desarrollo Ágil (Dataholics DigiVice):"
        echo "  ./dev.sh setup    - Configura el entorno virtual (.venv), dependencias y upstream"
        echo "  ./dev.sh sync     - Sincroniza la rama main con upstream (DEV-Dataholics/DigiVice)"
        echo "  ./dev.sh server   - Inicia el daemon de telemetría K-2SO en http://localhost:8000"
        echo "  ./dev.sh tools    - Levanta servidor web local en el puerto 3000 para simuladores"
        echo "  ./dev.sh check    - Valida sintaxis e imports de scripts Python de servidor"
        echo "  ./dev.sh status   - Muestra el estado del repositorio, ramas y remotes"
        echo "  ./dev.sh clean    - Limpia caches y archivos temporales de Python"
        ;;
esac
