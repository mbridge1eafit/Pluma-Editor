# Registro de Decisiones de Arquitectura — Pluma

Este documento registra las decisiones técnicas tomadas durante el desarrollo de Pluma siguiendo las instrucciones operativas del PRD.

---

## [2026-09-23] Decisión 001: Versiones fijas de dependencias vendorizadas en `third_party/`

- **Contexto:** El requerimiento NF-03 y la tarea M0.2 exigen enlazar estáticamente el CRT (`/MT`), Scintilla, Lexilla y md4c, sin dependencias dinámicas fuera de las DLLs del sistema operativo de Windows.
- **Decisión:**
  - **md4c:** Versión `release-0.5.2` (última versión estable que implementa CommonMark 0.31 y extensiones GFM con callbacks SAX de cero asignaciones por nodo).
  - **Scintilla:** Versión `5.5.3` (versión madura con soporte completo para Direct2D/DirectWrite, Win32 y control de edición de alto rendimiento).
  - **Lexilla:** Versión `5.4.3` (acompañante de Scintilla 5.x que contiene el lexer Markdown `SCLEX_MARKDOWN`).
- **Alternativas descartadas:**
  - *Gestores de paquetes como vcpkg/conan:* Agregan capas innecesarias y complejidad al pipeline CI y contradicen la directiva de vendorización fija en `third_party/`.
  - *Otras librerías de parsing (cmark, pulldown-cmark):* Descartadas en el PRD debido a que md4c está optimizado para C puro, tiene menor consumo de memoria y cero asignaciones por nodo.

---

## [2026-09-23] Decisión 002: Sistema de construcción y presets

- **Contexto:** El agente y el pipeline CI de GitHub Actions deben poder compilar y probar de forma reproducible con `cmake --preset` y `ctest --preset`.
- **Decisión:** Usar CMake 3.28+ con Ninja como generador, C++20 con MSVC (`/std:c++20`, `/W4`, `/WX`, `/permissive-`), enlace estático de CRT (`/MT` en Release, `/MTd` en Debug), y soporte para AddressSanitizer (`/fsanitize=address`) en el preset `asan`.
- **Alternativas descartadas:** Generador de Visual Studio msbuild (Ninja ofrece mayor velocidad en compilaciones incrementales y compatibilidad directa entre CI y consola local).

---

## [2026-09-23] Decisión 003: Sincronización y medición de arranque (M0.5)

- **Contexto:** NF-01 y la sección 2 exigen arranque en frío ≤ 60 ms (límite duro 100 ms) medido hasta el primer frame pintado.
- **Decisión:** La ventana principal emite una señal mediante un Named Event de Windows (`Local\PlumaStartupEvent_<PID>`) o marcador ETW justo después de procesar el primer `WM_PAINT`. El script de benchmark `bench/startup.ps1` mide el tiempo transcurrido desde `CreateProcess` hasta la señal del evento, garantizando exactitud de microsegundos sin interferir en el hilo de UI.
- **Alternativas descartadas:** Medición basada en sondeo de títulos de ventana o inspección de procesos (demasiado imprecisas e introducen jitter en la medición).
