# 📋 Dataholics DigiVice — Product Backlog & Agile Framework

> **Marco de Trabajo Ágil para el Ecosistema Soberano de Hardware, Firmware y Telemetría DigiVice.**  
> Repositorio Fork: `gruizmetasolutions-cpu/Digidata` | Upstream Base: `DEV-Dataholics/DigiVice`

---

## 🎯 1. Visión del Producto

El ecosistema **Dataholics DigiVice** fusiona terminales físicas táctiles IoT (ESP32 CYD, pantallas OLED), síntesis de audio procedural, gráficos retro LovyanGFX/LVGL 9 y daemons de observabilidad para proporcionar conciencia táctil y en tiempo real de operaciones de IA soberana (**K-2SO**, **TiRA**, el pipeline **Nemik/Andor** y **Resolve ServiceDesk**).

El objetivo ágil de este repositorio fork es acelerar el ciclo de desarrollo (*Tinkering*, pruebas, simulaciones y producción de firmware), manteniendo sincronización continua con el repositorio upstream institucional de Dataholics.

---

## 🏛️ 2. Épicas del Proyecto (Epics)

| Épica ID | Nombre de la Épica | Descripción | Área Tecnológica |
| :--- | :--- | :--- | :--- |
| **`EPIC-01`** | **Scrap-Hound ServiceDesk & CEO Badge** | Mascota de escritorio Yokai autónoma con enlace HMAC a tickets y gafete ejecutivo táctil para Alonso Ruiz (CEO). | Firmware (`cyd_resolve_pet`), LovyanGFX, mbedtls |
| **`EPIC-02`** | **Mecha OS Tactical HUD & Gundam Evolution** | HUD táctico cyberpunk con LVGL 9, métricas de clúster K-2SO y evolución por estrés del sistema. | Firmware (`cyd_digivice_v3`), LVGL 9.6, LovyanGFX |
| **`EPIC-03`** | **Pocket OLED Cyber Badge** | Terminal ultraportátil de bolsillo con pantalla SH1106 I2C y telemetría de droids. | Firmware (`oled_digivice_v1`), U8g2 / Adafruit SSD1306 |
| **`EPIC-04`** | **K-2SO Host Telemetry & Crucible Bridge** | Daemon HTTP FastAPI, máquina de estado Nemik/Andor y canal de alertas de emergencia. | Backend Python (`server/`), FastAPI, psutil |
| **`EPIC-05`** | **Simuladores Web & Studios de Prueba** | Entornos de simulación en navegador para pruebas rápidas sin requerir flashear el hardware físico. | Web (`tools/`, `viewer/`), HTML5 Canvas, JS |
| **`EPIC-06`** | **DevOps, Fork Sync & Calidad de Código** | Flujo continuo de sincronización con upstream, linters, templates de GitHub y scripts de compilación. | Git, GitHub Actions, PlatformIO |

---

## 📜 3. Historias de Usuario Priorizadas (User Stories)

### Priorización MoSCoW & Estimación en Story Points (Fibonacci: 1, 2, 3, 5, 8, 13)

| ID | Épica | Historia de Usuario | Story Points | Prioridad MoSCoW | Estado |
| :--- | :--- | :--- | :---: | :---: | :---: |
| **`US-01`** | `EPIC-06` | **Acondicionamiento del Repositorio Fork y Entorno Local** | 3 | **Must Have** | ✅ Done |
| **`US-02`** | `EPIC-04` | **Entorno de Ejecución Local para el Daemon de Telemetría K-2SO** | 3 | **Must Have** | ✅ Done |
| **`US-03`** | `EPIC-05` | **Servidor Local de Desarrollo para Simuladores Web y Web Studios** | 2 | **Must Have** | ✅ Done |
| **`US-04`** | `EPIC-01` | **Modo Captive Portal Wi-Fi con Persistencia NVS para Scrap-Hound** | 5 | **Should Have** | 📋 Backlog |
| **`US-05`** | `EPIC-02` | **Optimización de Render Diferencial y Animaciones en LVGL 9** | 5 | **Should Have** | 📋 Backlog |
| **`US-06`** | `EPIC-06` | **Automatización de Sincronización con Upstream (DEV-Dataholics/DigiVice)** | 2 | **Must Have** | ✅ Done |
| **`US-07`** | `EPIC-01` | **Telemetría Criptográfica HMAC-SHA256 con eFuse MAC de Fábrica** | 8 | **Should Have** | 📋 Backlog |
| **`US-08`** | `EPIC-03` | **Soporte de Batería y Modo Sleep para OLED Pocket Badge** | 5 | **Could Have** | 📋 Backlog |

---

## 🔍 4. Detalle de Historias de Usuario

### `US-01: Acondicionamiento del Repositorio Fork y Entorno Local`
- **Como:** Desarrollador del ecosistema DigiVice.
- **Quiero:** Que el repositorio fork cuente con remotes configurados (`origin` y `upstream`), `.gitignore` adecuado, estructura ágil y scripts de automatización.
- **Para:** Comenzar a programar inmediatamente sin fricción de configuración y sin riesgos de desincronización con la rama principal de Dataholics.
- **Criterios de Aceptación (Gherkin):**
  - **Dado** que tengo el repositorio clonado localmente.
  - **Cuando** ejecuto `git remote -v`.
  - **Entonces** debo visualizar `origin` apuntando a `gruizmetasolutions-cpu/Digidata` y `upstream` a `DEV-Dataholics/DigiVice`.
  - **Y** los archivos temporales de Python (`.venv/`) deben estar ignorados en `.gitignore`.

### `US-02: Entorno de Ejecución Local para el Daemon K-2SO`
- **Como:** Ingeniero de observabilidad y backend.
- **Quiero:** Un archivo `requirements.txt` explícito y un script para levantar `k2so_metrics_daemon.py` en un virtualenv aislado.
- **Para:** Probar la emisión de métricas JSON en el puerto 8000 antes de conectarle el hardware ESP32.
- **Criterios de Aceptación (Gherkin):**
  - **Dado** un entorno virtual Python creado.
  - **Cuando** ejecuto `make server` o `scripts/run_server.sh`.
  - **Entonces** el daemon FastAPI debe iniciar en `http://0.0.0.0:8000/metrics` respondiendo con estado 200 y JSON válido.

### `US-03: Servidor Local de Desarrollo para Simuladores Web`
- **Como:** Diseñador UI/UX y desarrollador de sprites.
- **Quiero:** Poder previsualizar y depurar los simuladores web (`scraphound_studio.html`, `digivice_simulator.html`, `conference_badge_simulator.html`) con un solo comando.
- **Para:** Ajustar los layouts táctiles y animaciones sin necesidad de flashear el ESP32 en cada cambio.
- **Criterios de Aceptación (Gherkin):**
  - **Dado** el script `scripts/serve_tools.sh`.
  - **Cuando** ejecuto `make tools`.
  - **Entonces** se inicia un servidor HTTP estático en el puerto 3000 con acceso a `tools/` y `viewer/`.

---

## 📐 5. Definición de Hecho (Definition of Done - DoD)

Toda tarea o Historia de Usuario se considera **Done** únicamente si cumple:
1. **Compilación / Sintaxis:** El código compila sin errores (firmware en PlatformIO) o pasa comprobación de sintaxis e imports (scripts Python).
2. **Pruebas en Simulador:** Cualquier cambio visual en UI o sprites es validado primero en los simuladores web correspondientes (`tools/*.html`).
3. **No Regresión de Hardware:** El consumo de DRAM/IRAM del ESP32 se mantiene dentro de los límites del microcontrolador (Free Heap > 50KB en ejecución).
4. **Convención Git:** Commits formateados según Conventional Commits (`feat:`, `fix:`, `docs:`, `chore:`).
5. **Sincronización:** La rama de trabajo está rebasada o actualizada respecto a `upstream/main`.
6. **Documentación:** Cambios de arquitectura, esquemas de payload o pines de hardware quedan reflejados en `docs/`.

---

## 🚦 6. Definición de Preparado (Definition of Ready - DoR)

Una Historia de Usuario puede entrar al Sprint si:
1. Cuenta con descripción en formato ágil estándar (*Como / Quiero / Para*).
2. Los criterios de aceptación están redactados de forma verificable (*Dado / Cuando / Entonces*).
3. Dependencias de hardware (pines, módulos, display) o endpoints de red están documentados.
4. Ha sido estimada en Story Points por el equipo.
