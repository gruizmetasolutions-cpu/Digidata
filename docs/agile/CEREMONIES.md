# ⏱️ Ceremonias y Ritmo Ágil — DigiVice

> **Guía práctica para la ejecución de ceremonias en el ciclo de sprints de DigiVice.**

---

## 📅 Calendario de Sprints (Cadencia de 2 Semanas)

```
Semana 1:
┌──────────────┬──────────────┬──────────────┬──────────────┬──────────────┐
│    LUNES     │    MARTES    │  MIÉRCOLES   │    JUEVES    │   VIERNES    │
├──────────────┼──────────────┼──────────────┼──────────────┼──────────────┤
│ Sprint       │ Daily        │ Daily        │ Daily        │ Daily        │
│ Planning     │ Standup      │ Standup      │ Standup      │ Standup      │
└──────────────┴──────────────┴──────────────┴──────────────┴──────────────┘

Semana 2:
┌──────────────┬──────────────┬──────────────┬──────────────┬──────────────┐
│    LUNES     │    MARTES    │  MIÉRCOLES   │    JUEVES    │   VIERNES    │
├──────────────┼──────────────┼──────────────┼──────────────┼──────────────┤
│ Daily        │ Daily        │ Daily        │ Daily        │ Sprint Review│
│ Standup      │ Standup      │ Standup      │ Standup      │ & Retrospect.│
└──────────────┴──────────────┴──────────────┴──────────────┴──────────────┘
```

---

## 1. Sprint Planning (Planificación)
- **Duración:** 45 - 60 minutos al inicio de cada sprint.
- **Entradas:** Product Backlog priorizado (`BACKLOG.md`), capacidad del equipo, feedback de retrospectivas anteriores.
- **Objetivo:** Definir el Sprint Goal y seleccionar las Historias de Usuario que cumplen el Definition of Ready (DoR).
- **Salida:** Tablero del Sprint actualizado (`SPRINT.md`).

---

## 2. Daily Standup (Sincronización Diaria / Asíncrona)
- **Duración:** 10 - 15 minutos (o reporte asíncrono en canal de desarrollo).
- **Las 3 Preguntas Clave:**
  1. ¿Qué logré ayer para avanzar hacia el Sprint Goal?
  2. ¿Qué planeo completar hoy?
  3. ¿Existe algún bloqueo o impedimento técnico (hardware, dependencias, upstream)?
- **Registro:** Se anota brevemente en la sección `Standup Log` de `SPRINT.md`.

---

## 3. Sprint Review (Demostración de Incrementos)
- **Duración:** 30 minutos al final del sprint.
- **Participantes:** Equipo de desarrollo, PO / Stakeholders.
- **Dinámica:**
  - Demostración visual de firmwares en simuladores web (`tools/*.html`) o en hardware físico (CYD / OLED).
  - Verificación contra los criterios de aceptación de cada Historia de Usuario.
  - Validación del Definition of Done (DoD).

---

## 4. Sprint Retrospective (Mejora Continua)
- **Duración:** 30 minutos inmediatamente después de la Review.
- **Estructura (Starfish o 4 Ls):**
  - **Liked (Qué funcionó bien):** Prácticas que debemos mantener (ej: simular antes de flashear).
  - **Learned (Qué aprendimos):** Nuevos hallazgos técnicos (ej: consumo de memoria con LVGL 9).
  - **Lacked (Qué hizo falta):** Herramientas, información o pruebas faltantes.
  - **Longed For (Qué deseamos implementar):** Nuevas automatizaciones o scripts para el siguiente sprint.
