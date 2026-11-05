#!/usr/bin/env bash
# ==============================================================================
# Script de Sincronización Automática con Upstream (DEV-Dataholics/DigiVice)
# ==============================================================================
set -euo pipefail

UPSTREAM_URL="https://github.com/DEV-Dataholics/DigiVice.git"
UPSTREAM_REMOTE="upstream"
DEFAULT_BRANCH="main"

echo "🔍 [1/4] Verificando configuración de remotes..."
if ! git remote get-url "$UPSTREAM_REMOTE" >/dev/null 2>&1; then
    echo "⚠️  Remote '$UPSTREAM_REMOTE' no encontrado. Añadiéndolo ($UPSTREAM_URL)..."
    git remote add "$UPSTREAM_REMOTE" "$UPSTREAM_URL"
fi

echo "📡 [2/4] Obteniendo cambios de $UPSTREAM_REMOTE..."
git fetch "$UPSTREAM_REMOTE"

CURRENT_BRANCH=$(git rev-parse --abbrev-ref HEAD)
echo "🌿 [3/4] Rama actual: $CURRENT_BRANCH"

if [ "$CURRENT_BRANCH" != "$DEFAULT_BRANCH" ]; then
    echo "ℹ️  Cambiando a '$DEFAULT_BRANCH' para sincronizar con upstream..."
    git checkout "$DEFAULT_BRANCH"
fi

echo "🔄 [4/4] Sincronizando '$DEFAULT_BRANCH' con '$UPSTREAM_REMOTE/$DEFAULT_BRANCH'..."
if git merge "$UPSTREAM_REMOTE/$DEFAULT_BRANCH" --ff-only; then
    echo "✅ Sincronización exitosa con fast-forward."
    echo "🚀 Publicando cambios en tu fork (origin/$DEFAULT_BRANCH)..."
    git push origin "$DEFAULT_BRANCH" || echo "⚠️  No se pudo hacer push automático a origin. Realízalo manualmente si es necesario."
else
    echo "❌ Conflicto detectado: la rama local ha divergido. Se requiere resolver manualmente."
    exit 1
fi

echo "🎉 ¡Tu fork está al día con DEV-Dataholics/DigiVice!"
