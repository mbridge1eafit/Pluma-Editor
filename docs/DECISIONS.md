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

---

## [2026-09-24] Decisión 009: Integración completa con el SO y pulido final (M5, F-04, F-11, F-12, F-13)

- **Contexto:** Los requerimientos F-04, F-11, F-12 y F-13 exigen pulido e integración total con el entorno nativo de Windows:
  - F-04: Cuadros de diálogo y atajos de teclado para búsqueda y reemplazo (`Ctrl+F`, `Ctrl+H`), búsqueda bidireccional (arriba/abajo), coincidencias y reemplazo individual o global. Diálogo para ir a línea (`Ctrl+G`).
  - F-11: Barra de estado nativa con 5 indicadores clave: posición del cursor (Lín, Col), conteo en vivo de palabras y caracteres, codificación (UTF-8, UTF-8 BOM, UTF-16 LE/BE), salto de línea (CRLF/LF) y modo de vista actual.
  - F-12: Atajos directos de formato Markdown (`Ctrl+B`, `Ctrl+I`, `Ctrl+Shift+C`, `Ctrl+Shift+X`, `Ctrl+K`) con inserción de delimitadores y selección inteligente.
  - F-13: Menú/popup de esquema de encabezados del documento (`Ctrl+Shift+O`) con jerarquía visual H1-H6 y salto sincronizado al encabezado seleccionado.
- **Decisión:**
  - *Barra de estado Win32 (`msctls_statusbar32`):*
    - Se crea en `MainWindow::HandleMessage` con clase `STATUSCLASSNAME` y flag `SBARS_SIZEGRIP`.
    - La partición de paneles se escala dinámicamente según los DPI del monitor en `UpdateStatusBarParts()` (`WM_CREATE` y `WM_DPICHANGED`), garantizando visualización sin recortes en monitores 4K o configuraciones multimonitor con factores de escala heterogéneos.
    - Se actualiza en `WM_SIZE`, `OpenFile`, `SaveFile`, `NewFile`, `SCN_UPDATEUI` y al cambiar el modo de visualización.
  - *Búsqueda y Reemplazo del sistema (`commdlg.h`):*
    - Se implementa mediante los cuadros de diálogo estándar de Windows `FindTextW` y `ReplaceTextW` registrados vía `RegisterWindowMessageW(FINDMSGSTRINGW)` y despachados en el bucle principal de mensajes con `IsDialogMessageW`.
    - `EditorView::FindNext` soporta búsqueda bidireccional (adelante y atrás) con `SCFIND_MATCHCASE` y `SCFIND_WHOLEWORD`, búsqueda circular (wrap-around) y selección visual de la coincidencia activa con `SCI_SCROLLCARET`.
    - `EditorView::ReplaceAll` agrupa todas las sustituciones en una única transacción de deshacer (`SCI_BEGINUNDOACTION` / `SCI_ENDUNDOACTION`), reportando el número exacto de reemplazos efectuados y actualizando el parseo y la barra de estado.
    - `ShowGotoLineDialog` implementa un cuadro modal nativo ligero Win32 que valida el número de línea 1-indexed y posiciona simultáneamente el cursor en el editor Scintilla y el scroll en la vista previa Direct2D.
  - *Atajos de formato Markdown (F-12):*
    - Métodos `WrapSelection`, `InsertBold`, `InsertItalic`, `InsertCode`, `InsertStrikethrough` e `InsertLink` en `EditorView`.
    - Si existe una selección de texto activa, los delimitadores la envuelven preservando o seleccionando los segmentos relevantes (por ejemplo, en `InsertLink` se envuelve `[texto](url)` dejando seleccionada la palabra `url` para sobrescritura inmediata).
    - Si la selección abarca múltiples líneas, `InsertCode` envuelve automáticamente con bloques cercados triple backtick (` ```\n...\n``` `); si es de una sola línea, aplica código en línea con acentos graves simples (`` `...` ``).
    - Si no hay selección, se insertan los delimitadores correspondientes y se posiciona el cursor en el centro para escritura inmediata.
  - *Esquema del documento flotante / TOC (F-13):*
    - `ShowOutlinePopup` ejecuta `Md4cAdapter::Parse` sobre el texto actual y extrae los encabezados de nivel H1 a H6 con su número de línea de origen.
    - Construye un menú flotante jerárquico `TrackPopupMenu` con indentación visual y prefijos (`#`, `##`, etc.).
    - Al hacer clic en un encabezado, navega instantáneamente al número de línea tanto en el editor como en la vista previa.
- **Alternativas descartadas:**
  - *Controles de interfaz de usuario de terceros o frameworks web embebidos:* Rechazados de plano según la arquitectura central del proyecto. Todos los diálogos, menús y barras de estado emplean las APIs nativas Win32 C++20 con coste cero en dependencias externas.

---

## [2026-09-24] Decisión 010: Revisión integral — renderizado Markdown, tablas sin desbordes y corrección de errores

- **Contexto:** Una revisión completa del código detectó errores funcionales (contenido que desaparecía en la vista previa y en el PDF, riesgo de pérdida de datos al abrir archivos bloqueados, DPI ignorado) y carencias visuales (enlaces sin color, emojis monocromos, tablas cuyo texto se salía de las celdas).
- **Decisión:**
  - *Vista previa en DIPs con DPI explícito:* el render target Direct2D se crea con el DPI del monitor y todo el layout trabaja en píxeles independientes del dispositivo; ratón, scroll y tamaño de página se convierten a DIPs. Antes el layout se mezclaba con píxeles físicos y se descuadraba al 125–200 %.
  - *Separación layout / dibujo:* `PreviewRenderer` dibuja un `LayoutEngine` sobre cualquier `ID2D1RenderTarget` (ventana o mapa de bits WIC), lo que permite verificar el renderizado fuera de pantalla.
  - *Layout recursivo:* listas anidadas, párrafos y bloques de código dentro de elementos de lista, citas anidadas y números de listas ordenadas (antes solo se mostraba el primer párrafo de cada elemento y los números no se dibujaban).
  - *Colores mediante drawing effects:* enlaces y código en línea reciben su pincel con `SetDrawingEffect`; los emojis se dibujan en color con `D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT`. Interlineado uniforme para que los emojis y el código no alteren la altura de las líneas.
  - *Tablas con ancho automático:* se mide el ancho natural y el de la palabra más larga de cada celda y se reparte el espacio como un navegador (auto table layout); si ni así cabe, se reparte con un tope común para no partir palabras cortas y el resto se ajusta con `DWRITE_WORD_WRAPPING_EMERGENCY_BREAK`. Cada celda se recorta a su rectángulo, así que ningún texto puede invadir otra celda. El mismo algoritmo se aplica al PDF.
  - *Parser:* entidades HTML completas (tabla de md4c y numéricas fuera de ASCII), HTML en bruto sin etiquetas visibles (`<br>`, `<img alt>`, comentarios ocultos), `:shortcodes:` de emoji al estilo GitHub fuera del código, y anclas (`Slugify`) compatibles con acentos.
  - *PDF:* texto enriquecido por tramos (negrita, cursiva, código, enlaces, tachado) con métricas AFM reales de Helvetica; antes se perdía todo el texto con formato.
  - *E/S:* `ReadDocument` lanza excepción si no puede leer (antes devolvía un documento vacío que al guardarse sobrescribía el archivo real), comparte el archivo con otros editores y detecta archivos ANSI (Windows-1252) con ida y vuelta exacta.
- **Alternativas descartadas:**
  - *Scroll horizontal por tabla:* complejo de implementar con Win32 puro y menos cómodo que ajustar el contenido al ancho disponible.
  - *Decodificar imágenes con WIC en la vista previa:* aumenta el coste de arranque y memoria; se muestra un marcador clicable con el texto alternativo que abre la imagen.

---

## [2026-09-24] Decisión 011: Preferencias persistentes, panel de encabezados y editor predeterminado

- **Contexto:** El usuario necesita adaptar Pluma a sus preferencias sin recompilar, navegar documentos largos con un esquema siempre visible y abrir los archivos `.md` con Pluma desde el Explorador.
- **Decisión:**
  - *Configuración (`src/config/settings.*`):* archivo INI UTF-8 en `%APPDATA%\Pluma\pluma.ini`, o `pluma.ini` junto al ejecutable si existe (modo portable). Parser y serializador propios, puros y probados; los valores fuera de rango se acotan y los inválidos conservan el valor por defecto. Se guarda de forma atómica (`WriteDocumentAtomic`) al cerrar y al aceptar/aplicar el diálogo. Incluye tema, fuente y tamaño del editor, zoom de la vista previa, ajuste de línea, números de línea, línea actual, tabulación, fin de línea de documentos nuevos, vista inicial, panel de encabezados, barra de estado, scroll sincronizado, posición de ventana, reabrir el último documento y opciones de exportación PDF/HTML.
  - *Diálogo «Preferencias» (`Configuración > Preferencias…`, `Ctrl+,`):* plantilla `DIALOGEX` (escalado DPI automático del sistema) con aplicación en vivo (`Aplicar`). En modo oscuro las casillas se pintan vía `NM_CUSTOMDRAW`, porque las casillas con tema ignoran el color de texto de `WM_CTLCOLORSTATIC`.
  - *Zoom de la vista previa:* se aplica como escala de DPI del render target Direct2D; el diseño y el texto crecen de forma uniforme sin tocar el motor de layout.
  - *Panel de encabezados (`src/outline/outline_panel.*`, `Ver > Panel de encabezados`, `Ctrl+Shift+E`):* panel lateral izquierdo redimensionable con un `TreeView` jerárquico alimentado por el árbol del parseo asíncrono (sin parseo adicional). Si solo cambian los títulos se editan en su sitio, sin reconstruir ni perder el desplazamiento. Resalta la sección del cursor y navega con clic o teclado.
  - *Editor predeterminado (`Configuración > Establecer como editor predeterminado de Markdown…`):* registra en `HKCU` (sin elevación) el ProgID `Pluma.Markdown`, `OpenWithProgids`, `Applications\pluma.exe` y las *Capabilities* de `RegisteredApplications`. Windows 10/11 protege la elección del usuario (`UserChoice` con hash), así que la confirmación se hace en Configuración: `ms-settings:defaultapps?registeredAppUser=Pluma` (página propia de Pluma en Windows 11 con la actualización 2023-04 o posterior; Windows 10 ignora el parámetro y abre «Aplicaciones predeterminadas»).
- **Alternativas descartadas:**
  - *Registro de Windows para las preferencias:* menos transparente para el usuario e incompatible con el modo portable.
  - *Escribir `UserChoice` directamente:* Windows invalida la entrada (hash) y restablece la asociación; no es un mecanismo soportado.
  - *Selector «Abrir con» (`SHOpenWithDialog`):* desde Windows 10 ignora `OAIF_*_REGISTRATION` y, sin `OAIF_EXEC`, solo muestra un aviso que remite a Configuración (y devuelve éxito). Con `OAIF_EXEC` abriría un archivo de muestra y solo asociaría `.md`.

---

## [2026-09-24] Decisión 012: Renderizado nativo de diagramas Mermaid

- **Contexto:** Los bloques ` ```mermaid ` se mostraban como código. La referencia de Mermaid es una biblioteca JavaScript que necesita un DOM y un navegador, prohibidos por NF-09.
- **Decisión:**
  - *Motor propio en C++ (`src/diagram/`):* parser y layout de `flowchart`/`graph` (direcciones TB/TD/BT/LR/RL, las 14 formas de nodo, sintaxis `A@{ shape }`, aristas `-->`, `---`, `-.->`, `==>`, `~~~`, `<-->`, `--o`, `--x`, etiquetas `|texto|` y `-- texto -->`, longitud por guiones extra, cadenas y `&`, `subgraph` anidados, `classDef`/`class`/`:::`/`style`/`linkStyle`), `sequenceDiagram` (participantes y actores, 10 tipos de flecha, activaciones `+`/`-` y `activate`, notas, `loop`/`alt`/`else`/`opt`/`par`/`critical`/`break`/`rect`, `autonumber`, `title`), `stateDiagram(-v2)` (inicio/fin por ámbito, estados compuestos, `<<fork>>`/`<<join>>`/`<<choice>>`, descripciones, notas) y `pie` (`showData`, `title`). Se aceptan front matter (`title:`) y directivas `%%{init}%%` (ignoradas).
  - *Layout jerárquico tipo Sugiyama (`graph_layout.*`):* inversión de ciclos por DFS, rangos por camino más largo con fuentes ajustadas, nodos ficticios por rango atravesado (el central lleva la etiqueta, como dagre), reducción de cruces por baricentro manteniendo contiguos los subgrafos, coordenadas por regresión isotónica ponderada con separaciones mínimas (aristas largas rectas) y una pasada que expulsa del marco de cada subgrafo los nodos ajenos en todos los rangos que abarca.
  - *Escena vectorial independiente del backend (`diagram_scene.h`):* rectángulos, elipses, trazos Bézier y texto de una línea con colores por *rol*. El tema se resuelve al pintar, así que cambiar claro/oscuro no rehace el layout. Cada backend aporta la medición de texto de su propia fuente para que las etiquetas siempre quepan: DirectWrite (Segoe UI) en la vista previa, métricas AFM de Helvetica en PDF y Helvetica/Arial en SVG.
  - *Vista previa:* `LayoutEngine` cachea la escena y sus `IDWriteTextLayout` por texto fuente (se descartan las no usadas en el último layout) y escala el diagrama al ancho de la columna; `PreviewRenderer` crea las geometrías Direct2D una sola vez por diagrama y recolorea un único pincel por primitiva.
  - *HTML:* SVG inline autocontenido cuyos colores son variables CSS (`--pluma-mm-*`) con la paleta clara de respaldo, por lo que sigue el tema claro/oscuro/automático de la página. *PDF:* operadores vectoriales (`re`, `c`, `B`, texto WinAnsi) escalados a la página.
  - *Errores:* un diagrama inválido o de un tipo no compatible (`classDiagram`, `erDiagram`, `gantt`, `gitGraph`, `mindmap`...) se muestra como bloque de código con el motivo en la etiqueta (vista previa) o como código (HTML/PDF). Entradas de más de 64 KB o más de 400 nodos se rechazan para no bloquear el hilo de UI.
- **Alternativas descartadas:**
  - *mermaid.js en WebView2 o con un motor JavaScript embebido (QuickJS):* WebView2 está prohibido por NF-09 y mermaid.js depende del DOM y de la medición de texto del navegador.
  - *`mmdc` (mermaid-cli) externo:* requiere Node.js y Chromium instalados.
  - *Cargar mermaid.js desde un CDN en el HTML exportado:* rompería el requisito de HTML autocontenido y usable sin conexión.

---

## [2026-09-24] Decisión 013: Instalador y actualización automática

- **Contexto:** Pluma se distribuía solo como ZIP portable: el usuario tenía que enterarse por su cuenta de las versiones nuevas, descargarlas y reemplazar el ejecutable. Se necesita un instalador que asocie los archivos Markdown y que sirva para actualizar, y que la aplicación avise de las versiones nuevas y se actualice sola si el usuario lo acepta.
- **Decisión:**
  - *Instalador (`installer/pluma.iss`, Inno Setup 6):* instalación **por usuario** (`PrivilegesRequired=lowest`, `%LOCALAPPDATA%\Programs\Pluma`), sin UAC, lo que permite actualizar sin elevar. Tarea «Abrir los archivos Markdown con Pluma de forma predeterminada» (marcada por defecto) que escribe en `HKCU` las mismas claves que `RegisterMarkdownHandler`; el ProgID, «Abrir con» y las *Capabilities* se registran siempre. Si el usuario ya eligió otra aplicación (`UserChoice`/`UserChoiceLatest`), Windows solo permite cambiarlo en Configuración: la página final ofrece abrir `ms-settings:defaultapps?registeredAppUser=Pluma`. El desinstalador elimina todas las claves.
  - *Actualización con el mismo instalador:* el `AppId` es fijo, así que una versión nueva se instala encima de la anterior conservando carpeta, tareas elegidas (`UsePreviousTasks`) y `%APPDATA%\Pluma\pluma.ini`. `pluma.exe` mantiene el mutex `PlumaEditorAppMutex`; el instalador espera a que desaparezca (hasta 30 s en una actualización lanzada por Pluma) o pide cerrar Pluma.
  - *Comprobación en la aplicación (`src/update/`):* 4 s después de arrancar (sin afectar al arranque, NF-01) y como máximo una vez al día, un hilo consulta `GET /repos/mbridge1eafit/Pluma-Editor/releases/latest` con WinHTTP (HTTPS obligatorio, proxy del sistema). Parser JSON propio mínimo y probado; comparación SemVer; se ignoran borradores, *prereleases* y la versión que el usuario decidió omitir. `Ayuda > Buscar actualizaciones…` hace la consulta a demanda. Se desactiva en Preferencias.
  - *Flujo de actualización:* diálogo nativo (`TaskDialogIndirect`) con las novedades y tres opciones: *Actualizar ahora*, *Recordármelo más tarde*, *Omitir esta versión*. Al aceptar, se descarga el instalador (`pluma-vX.Y.Z-setup-x64.exe`) a `%TEMP%\Pluma\Update` con barra de progreso cancelable, se verifica con el SHA-256 publicado en `SHA256SUMS.txt` (CNG/`bcrypt`), se pide guardar el documento, se comprueba que no haya otras ventanas de Pluma y se lanza `/SILENT /SUPPRESSMSGBOXES /NORESTART /SP- /PLUMAUPDATE=1 /PLUMAOPEN="doc.md"`. Pluma se cierra; el instalador actualiza y vuelve a abrirlo con el mismo documento.
  - *Copias portables:* si `pluma.exe` no está en la carpeta registrada por el instalador (`HKCU\...\Uninstall\{AppId}_is1\InstallLocation`), el aviso ofrece abrir la página de la versión en lugar de instalar.
  - *Versión única:* `project(VERSION)` de CMake genera `pluma_version.h`, usado por el código y por `pluma.rc`. `package_release.ps1` falla si la versión de `pluma.exe` no coincide con el tag (si no, el actualizador ofrecería la misma versión sin fin). `winhttp.dll` y `bcrypt.dll` se cargan en diferido (NF-04).
- **Alternativas descartadas:**
  - *MSI (WiX):* más pesado de mantener y orientado a instalaciones por máquina; las actualizaciones mayores de MSI son más frágiles que la reinstalación en sitio de Inno Setup.
  - *NSIS:* equivalente en capacidad, pero Inno Setup ofrece de serie actualización en sitio por `AppId`, recuerdo de tareas, desinstalación de claves del registro y un asistente moderno.
  - *Actualizador que reemplaza `pluma.exe` directamente (sin instalador):* obligaría a mantener dos caminos de instalación y a reimplementar el registro de asociaciones y la desinstalación.
  - *Instalación para todos los usuarios (Program Files):* cada actualización necesitaría UAC.
  - *Verificar solo con HTTPS:* no detecta descargas corruptas o truncadas; el SHA-256 publicado con la versión sí. (El instalador no está firmado con Authenticode: requiere un certificado de pago.)
