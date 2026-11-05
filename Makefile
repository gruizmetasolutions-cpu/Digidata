# ==============================================================================
# Dataholics DigiVice — Makefile de Automatización y Desarrollo Ágil
# ==============================================================================

.PHONY: help setup sync server tools check status clean

help:
	@echo "🛠️ Comandos de Desarrollo Disponibles (Dataholics DigiVice):"
	@echo "  make setup    - Configura el entorno virtual (.venv), dependencias y upstream"
	@echo "  make sync     - Sincroniza la rama main con upstream (DEV-Dataholics/DigiVice)"
	@echo "  make server   - Inicia el daemon de telemetría K-2SO en http://localhost:8000"
	@echo "  make tools    - Levanta servidor web local en el puerto 3000 para simuladores"
	@echo "  make check    - Valida sintaxis e imports de scripts Python de servidor"
	@echo "  make status   - Muestra el estado del repositorio, ramas y remotes"
	@echo "  make clean    - Limpia caches y archivos temporales de Python"

setup:
	@echo "🚀 Configurando entorno de desarrollo..."
	@which uv >/dev/null 2>&1 && uv venv .venv || python3 -m venv .venv
	@.venv/bin/pip install -r server/requirements.txt
	@chmod +x scripts/*.sh
	@./scripts/sync_upstream.sh
	@echo "✅ Entorno configurado correctamente."

sync:
	@./scripts/sync_upstream.sh

server:
	@./scripts/run_server.sh

tools:
	@./scripts/serve_tools.sh

check:
	@echo "🔍 Verificando módulos de servidor..."
	@.venv/bin/python3 -c "import server.k2so_metrics_daemon, server.k2so_mission_state, server.nemik_hitm_bridge; print('✅ Todos los módulos Python cargaron sin errores.')"

status:
	@echo "📊 Estado del Repositorio Git:"
	@git status -s
	@echo ""
	@echo "🔗 Remotes Configurados:"
	@git remote -v
	@echo ""
	@echo "🌿 Ramas Locales y Remotas:"
	@git branch -a

clean:
	@find . -type d -name "__pycache__" -exec rm -rf {} +
	@find . -type f -name "*.pyc" -delete
	@echo "🧹 Limpieza completada."
