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

---

## [2026-09-23] Decisión 004: Integración directa de Scintilla y Lexilla (M1.2, M1.3)

- **Contexto:** M1.2 exige evitar el costo de `SendMessage` en el editor, y M1.3 requiere resaltado Markdown nativo en modo claro y oscuro.
- **Decisión:** Usar `SCI_GETDIRECTFUNCTION` y `SCI_GETDIRECTPOINTER` para obtener un puntero a función directa en C (`sptr_t (*)(sptr_t, unsigned int, uptr_t, sptr_t)`), eliminando la sobrecarga del subsistema de mensajes de Windows en operaciones de edición. El lexer Markdown se instancia directamente mediante `lmMarkdown.Create()` enlazado estáticamente sin registro dinámico ni dependencias de runtime.
- **Alternativas descartadas:** Carga dinámica vía `LoadLibrary("Lexilla.dll")` (violaría la regla de un solo ejecutable portable y el límite estricto de dependencias).

---

## [2026-09-23] Decisión 005: Preservación de codificación y guardado atómico (M1.1, F-02, F-03, NF-08)

- **Contexto:** F-03 exige round-trip byte a byte sin alterar codificación (UTF-8 con/sin BOM, UTF-16 LE/BE) ni saltos de línea (CRLF, LF). NF-08 exige guardado atómico para prevenir pérdidas de datos ante cortes imprevistos.
- **Decisión:** `DocumentIO` analiza los primeros bytes para BOM (3 bytes para UTF-8 BOM, 2 bytes para UTF-16 LE/BE) y preserva el formato original al guardar. Si el archivo no tiene BOM, se trata como UTF-8 directo. Para archivos mayores a 1 MB se usa `CreateFileMappingW` + `MapViewOfFile`. El guardado atómico escribe primero a un archivo temporal (`.tmp_<timestamp>_<pid>`) en el mismo directorio, descarga búferes a disco (`FlushFileBuffers`), y ejecuta `ReplaceFileW` (con fallback a `MoveFileExW` con `MOVEFILE_REPLACE_EXISTING`).
- **Alternativas descartadas:** Sobrescritura directa de archivos in-place (riesgo de corrupción de datos si el proceso o el sistema se interrumpe a mitad de escritura).
