# 🔀 Git Workflow & Fork Guide — DigiVice Ecosystem

> **Guía oficial de versionado, ramas y sincronización para el fork `gruizmetasolutions-cpu/Digidata`.**

---

## 🏛️ 1. Topología del Fork

Trabajamos con un modelo de fork sincronizado con el repositorio upstream oficial de la organización:

```
[DEV-Dataholics/DigiVice] (upstream)
         │
         ▼  (git fetch upstream)
[Local Workspace: /home/antigravity/gruizmetasolutions-cpu/Digidata]
         │
         ▼  (git push origin)
[gruizmetasolutions-cpu/Digidata] (origin)
```

### Configuración de Remotes:
- **`origin`**: Tu fork personal (`https://github.com/gruizmetasolutions-cpu/Digidata.git`).
- **`upstream`**: El repositorio principal oficial (`https://github.com/DEV-Dataholics/DigiVice.git`).

Verificación en terminal:
```bash
git remote -v
```

---

## 🔄 2. Procedimiento de Sincronización con Upstream

Para mantener tu fork actualizado con los últimos cambios de Dataholics sin perder trabajo local:

### Método 1: Vía Script Automatizado (Recomendado)
```bash
make sync
# O directamente:
./scripts/sync_upstream.sh
```

### Método 2: Manual
```bash
# 1. Asegurar que no hay cambios sucios en el árbol de trabajo
git status

# 2. Descargar cambios de upstream
git fetch upstream

# 3. Cambiar a main e incorporar upstream con fast-forward
git checkout main
git merge upstream/main --ff-only

# 4. Publicar la actualización a tu fork en GitHub
git push origin main
```

---

## 🌿 3. Estrategia de Ramas (Branching Model)

1. **`main`**: Es un reflejo exacto y limpio de `upstream/main`. **Nunca hagas commits directos sobre `main`**.
2. **`feat/<nombre-de-historia>`**: Para el desarrollo de nuevas funcionalidades (ej: `feat/captive-portal-wifi`, `feat/scraphound-sprites`).
3. **`fix/<descripcion-del-bug>`**: Para corrección de errores (ej: `fix/audio-gpio-leak`, `fix/ntp-timeout`).
4. **`docs/<tema>`**: Para adiciones o mejoras documentales.
5. **`chore/<mantenimiento>`**: Para dependencias, scripts o configuración CI/CD.

### Creando una nueva rama de trabajo:
```bash
git checkout main
git pull upstream main
git checkout -b feat/mi-nueva-funcionalidad
```

---

## 💬 4. Convención de Commits (Conventional Commits)

Utilizamos el estándar Conventional Commits para mantener un historial legible y permitir changelogs automáticos:

```
<tipo>(<alcance>): <descripción concisa en imperativo o presente>

[cuerpo opcional explicando el porqué o detalles técnicos]

[referencias a issues: Fixes #12, Closes #4]
```

### Tipos Permitidos:
- **`feat`**: Nueva funcionalidad (ej: `feat(pet): add barking sound on poke`).
- **`fix`**: Corrección de bug (ej: `fix(hud): resolve flicker in differential clock render`).
- **`docs`**: Cambios en documentación (ej: `docs: update hardware wiring table for ST7789`).
- **`style`**: Formato de código, espacios, punto y comas (sin cambios en lógica).
- **`refactor`**: Refactorización de código sin cambiar comportamiento exterior.
- **`perf`**: Mejoras de rendimiento o reducción de uso de RAM.
- **`test`**: Pruebas unitarias o simuladores.
- **`chore`**: Tareas de configuración, dependencias o scripts auxiliares.

### Alcances Recomendados (`<alcance>`):
- `pet` (`firmware/cyd_resolve_pet/`)
- `hud` (`firmware/cyd_digivice_v3/`)
- `oled` (`firmware/oled_digivice_v1/`)
- `server` (`server/`)
- `tools` (`tools/`, `viewer/`)
- `wifi` (Módulo Wi-Fi / Portal cautivo)
- `crypto` (HMAC / eFuse / tokens)
- `agile` (Backlog, sprints, templates)

---

## 🚀 5. Flujo de Pull Request (PR)

1. Haz push de tu rama a tu fork:
   ```bash
   git push -u origin feat/mi-nueva-funcionalidad
   ```
2. Crea el Pull Request mediante `gh` o la interfaz web:
   ```bash
   gh pr create --title "feat(pet): mi nueva funcionalidad" --body "Descripción detallada del cambio..."
   ```
3. Verifica que la plantilla `.github/PULL_REQUEST_TEMPLATE.md` esté completa.
4. Tras la revisión y aprobación, mezcla mediante **Squash and Merge** o **Rebase** para conservar la linealidad del historial.
