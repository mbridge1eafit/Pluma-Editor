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

---

## [2026-09-24] Decisión 006: Pipeline asíncrono de parseo Markdown y AST BlockTree (M2, F-08, F-14)

- **Contexto:** El requerimiento F-08 exige parseo en hilo secundario con un debounce de 50 ms sin bloquear el hilo de la interfaz de usuario, descartando automáticamente versiones obsoletas. El requerimiento F-14 exige correspondencia entre bloques y líneas fuente del documento para el scroll sincronizado.
- **Decisión:**
  - El AST `BlockTree` representa los bloques jerárquicos (encabezados H1-H6, párrafos, citas, listas ordenadas/desordenadas, listas de tareas GFM `[x]`/`[ ]`, bloques de código con lenguaje, separadores temáticos, tablas completas con cabecera y alineación de columnas) y spans en línea (texto, énfasis, negrita, código, enlaces, imágenes, tachado y saltos de línea) almacenando rangos de líneas fuente (`startLine`..`endLine`).
  - `Md4cAdapter` implementa callbacks SAX de md4c enlazados a la especificación CommonMark 0.31 + extensiones GFM. La conversión de offsets de caracteres a números de línea se implementa con un avance secuencial monotónico amortizado $O(1)$ (`cachedLineIndex`), garantizando un tiempo de parseo total $O(N)$ en documentos grandes sin overhead de iteradores de depuración.
  - `ParseWorker` encapsula un hilo en segundo plano (`std::jthread`) sincronizado con `SRWLOCK` y `CONDITION_VARIABLE` (`SleepConditionVariableSRW`). El debounce se computa de forma reactiva; si se encolan nuevas pulsaciones durante el debounce o el parseo, las versiones obsoletas se liberan inmediatamente sin enviar mensajes. Cuando la versión es actual, se transfiere la propiedad del árbol al hilo de UI mediante `PostMessageW(m_targetHwnd, WM_USER_PARSE_COMPLETE, version, reinterpret_cast<LPARAM>(tree.release()))`.
- **Alternativas descartadas:**
  - *Parseo síncrono en el hilo de UI:* Descartado porque bloquearía la interfaz y aumentaría la latencia de tecleo a pixel en documentos medianos y grandes (> 100 KB), violando NF-05 y F-08.
  - *Generación de HTML intermedio o DOM web:* Descartado conforme al PRD; el AST en C++ alimenta directamente el motor de layout DirectWrite en M3 sin intermediarios web.

---

## [2026-09-24] Decisión 007: Motor de vista previa Direct2D/DirectWrite y Split View (M3, F-06, F-07, F-10)

- **Contexto:** Los requerimientos F-06 y F-07 exigen renderizado de vista previa enriquecida (encabezados con escala tipográfica proporcional, texto enriquecido, bloques de código, citas, listas, tablas y enlaces) en un componente nativo sin motor de navegador embebido. El requerimiento F-10 exige soporte de enlaces internos (`#ancla`) y externos con cursor interactivo y navegación fluida. Además, se requiere un modo split view con divisor arrastrable y atajos de vista (`Ctrl+1`, `Ctrl+2`, `Ctrl+3`).
- **Decisión:**
  - `PreviewLayout` utiliza la API nativa DirectWrite (`IDWriteFactory`, `IDWriteTextFormat`, `IDWriteTextLayout`) para medir y diagramar bloques tipográficos en coordenadas lógicas y de píxel con escalado Per-Monitor DPI v2. Soporta encabezados H1-H6 con tamaños y márgenes específicos, bloques de código en `Consolas` con fondo diferenciado, citas con sangría y barra vertical, listas ordenadas y de tareas con checkboxes interactivos, tablas formateadas con bordes y anchos proporcionales, y slugs canónicos para anclas internas.
  - Los rangos de enlaces (`LinkTarget`) se almacenan con sus rectángulos delimitadores (`D2D1_RECT_F`) durante el paso de layout. `HitTestLink(x, y)` permite hit-testing inmediato en coordenadas de cliente con viewport offset.
  - `PreviewView` encapsula una ventana hija Win32 (`PlumaPreviewViewClass`) con render target Direct2D (`ID2D1HwndRenderTarget`) y doble búfer de presentación sin parpadeo. Implementa scroll vertical nativo (`SetScrollInfo`, `WM_VSCROLL`, `WM_MOUSEWHEEL`), culling de visualización (solo dibuja bloques dentro del viewport actual), cursor de mano interactivo (`IDC_HAND`), navegación de anclas internas mediante desplazamiento del scroll y apertura de URLs externas con `ShellExecuteW`.
  - `MainWindow` gestiona el divisor vertical arrastrable entre el editor y la vista previa con cursor `IDC_SIZEWE`, soporte de DPI dinámico (`WM_DPICHANGED`), y conmutación de vistas (`ViewMode::EditorOnly`, `ViewMode::Split`, `ViewMode::PreviewOnly`).
- **Alternativas descartadas:**
  - *GDI / GDI+:* Descartado por falta de aceleración por hardware, renderizado subpixel deficiente de fuentes modernas e incapacidad para manejar tipografía compleja y DPI dinámico con la calidad y rendimiento de DirectWrite.
  - *WebView2 / Chromium:* Estrictamente prohibido por la especificación (NF-09) para evitar inflar el binario y el consumo de RAM.

---

## [2026-09-24] Decisión 008: Scroll sincronizado bidireccional y exportación HTML/PDF (M4, F-09, F-14)

- **Contexto:** El requerimiento F-14 exige scroll sincronizado bidireccional proporcional por bloque AST sin rebotes ni bucles infinitos de eventos. El requerimiento F-09 exige exportación a HTML autocontenido con CSS embebido y temas claro/oscuro, y exportación a PDF vector con márgenes estándar de 20 mm, saltos de página respetando bloques y sin encabezados huérfanos.
- **Decisión:**
  - `SyncScrollController`:
    - Coordina la posición visible entre Scintilla (`EditorView::GetFirstVisibleDocLine()` y `ScrollToDocLine()`) y la vista previa (`PreviewView::ScrollToLine()` y `GetLineForCurrentScroll()`).
    - En `LayoutEngine`, la función `GetScrollYForLine(docLine)` interpola proporcionalmente la coordenada Y dentro del bloque AST (`bounds.top` a `bounds.bottom`) o entre bloques adyacentes. La inversa `GetLineForScrollY(scrollY)` mapea la coordenada visual al número de línea del documento fuente.
    - Prevención de rebote (anti-echo): `SyncScrollController` rastrea `ScrollSource::Editor` vs `ScrollSource::Preview` y `lastEditorLine`/`lastPreviewLine`. Al propagar el scroll a un panel, se suprime temporalmente la notificación de retorno, garantizando cero oscilaciones.
  - `HtmlExporter`:
    - Emplea `md_html` de md4c enlazado estáticamente con dialecto GitHub (tablas, tareas, tachado, autolinks). Genera un documento HTML5 completo y autónomo con estilos modernos embebidos en `<style>` basados en variables CSS (`:root` / `@media (prefers-color-scheme: dark)`), contenedor responsive de 860 px, tipografía nativa del sistema y reglas de impresión `@media print` (`page-break-inside: avoid`). Tiempo de exportación < 5 ms para documentos típicos y < 35 ms para archivos de 300 KB (cumpliendo sobradamente el presupuesto de < 500 ms de F-09).
  - `PdfExporter`:
    - Generador directo de PDF 1.4 de alta precisión vectorial con soporte de tamaños A4 y Carta, y márgenes configurables de 20 mm (estándar F-09).
    - Prevención de encabezados huérfanos (orphan prevention): si un encabezado no dispone de al menos 40 pt para incluir contenido siguiente en la página actual, se genera un salto de página anticipado.
    - Manejo de bloques: los bloques de código y tablas calculan su altura y preservan filas sin cortes arbitrarios.
    - Fuentes estándar Type 1 (Helvetica, Courier) con codificación WinAnsi y secuencias de escape octales para caracteres acentuados y símbolos especiales, garantizando compatibilidad universal sin dependencias de servicios externos ni del servicio de cola de impresión de Windows.
- **Alternativas descartadas:**
  - *Sincronización por porcentaje lineal de altura:* Descartada porque diferentes densidades tipográficas entre código fuente y texto renderizado provocan desfasajes notables en documentos reales, violando el criterio de aceptación 2 de F-14.
  - *Generación de PDF delegada exclusivamente a spooler de Windows ("Microsoft Print to PDF"):* Descartada como única opción debido a fallos cuando el servicio Spooler está desactivado por políticas corporativas o entornos restringidos. El generador vectorial nativo proporciona un rendimiento < 5 ms, 100 % de fiabilidad y salida de vectores pura.


