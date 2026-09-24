# Pluma — Editor Markdown nativo para Windows

Pluma es un editor de Markdown 100 % nativo para Windows diseñado para abrir en menos de 100 ms y ocupar menos de 3 MB en disco, con vista previa renderizada sin navegador embebido (sin Chromium, sin WebView2, sin .NET).

## Descargas e Instalación

Descarga la versión más reciente desde [GitHub Releases](https://github.com/mbridge1eafit/Pluma-Editor/releases). Hay dos formatos:

### Instalador (recomendado)

1. Descarga `pluma-vX.X.X-setup-x64.exe` y ejecútalo. No requiere permisos de administrador: se instala para el usuario actual en `%LOCALAPPDATA%\Programs\Pluma`.
2. En el asistente puedes marcar **«Abrir los archivos Markdown (.md, .markdown, .mdown) con Pluma de forma predeterminada»** y crear un acceso directo en el escritorio. Si otra aplicación ya abría los `.md`, Windows exige confirmarlo en Configuración: la última página del asistente ofrece abrirla.
3. Para actualizar a mano, ejecuta el instalador de la versión nueva: se instala encima de la anterior y conserva la carpeta, las opciones elegidas y tu configuración.
4. Se desinstala desde **Configuración > Aplicaciones**; el desinstalador elimina también las asociaciones de archivos.

Al no estar firmado digitalmente, SmartScreen puede mostrar una advertencia al abrir el instalador descargado: pulsa **Más información ➔ Ejecutar de todas formas**.

### Versión portable (ZIP)

1. Descarga `pluma-vX.X.X-windows-x64.zip`.
2. **Desbloquear en Windows:** clic derecho sobre el `.zip` ➔ **Propiedades** ➔ marca **"Desbloquear"** (*Unblock*) ➔ **Aceptar**, o por PowerShell: `Unblock-File .\pluma-*-windows-x64.zip`.
3. Descomprime y ejecuta `pluma.exe` (no requiere instalación).

### Verificar la integridad (SHA-256)

Compara el hash del archivo descargado con el publicado en `SHA256SUMS.txt`:
```powershell
Get-FileHash .\pluma-*-setup-x64.exe -Algorithm SHA256
```

## Actualizaciones

Pluma comprueba en segundo plano, como máximo una vez al día, si hay una versión nueva en GitHub Releases (también a demanda en `Ayuda > Buscar actualizaciones…`). Si la hay, muestra las novedades y permite elegir:

- **Actualizar ahora:** descarga el instalador, verifica su SHA-256 con el `SHA256SUMS.txt` publicado, pide guardar el documento, cierra Pluma, instala la versión nueva en modo silencioso y vuelve a abrir Pluma con el mismo documento.
- **Recordármelo más tarde** u **Omitir esta versión** (no vuelve a avisar de esa versión).

La actualización automática requiere la copia instalada con el instalador; en la versión portable el aviso abre la página de descarga. La comprobación automática se desactiva en `Configuración > Preferencias…`.

## Requisitos de compilación

- **Sistema Operativo:** Windows 10 (versión 1809 o superior) o Windows 11 (x64)
- **Compilador:** Visual Studio 2022 (MSVC 17.x) con soporte para C++20
- **Herramientas de construcción:** CMake 3.28+ y Ninja

## Instrucciones de compilación

El proyecto utiliza `CMakePresets.json` para builds reproducibles:

### Compilación en Release (optimizada, CRT estático `/MT`)
```powershell
# Configurar
cmake --preset release

# Compilar
cmake --build --preset release

# Ejecutar pruebas unitarias
ctest --preset release --output-on-failure
```

El binario resultante se generará en:
`build/release/bin/pluma.exe`

### Paquetes de distribución (ZIP e instalador)

El instalador se genera con [Inno Setup 6](https://jrsoftware.org/isinfo.php) (`winget install --id JRSoftware.InnoSetup -e`) a partir de `installer/pluma.iss`:
```powershell
.\scripts\package_release.ps1            # dist\: ZIP portable, instalador y SHA256SUMS.txt
```
La versión se define solo en `project(VERSION)` de `CMakeLists.txt` (y en `res/pluma.manifest`).

### Compilación en Debug
```powershell
cmake --preset debug
cmake --build --preset debug
ctest --preset debug --output-on-failure
```

### Compilación con AddressSanitizer (ASan)
```powershell
cmake --preset asan
cmake --build --preset asan
ctest --preset asan --output-on-failure
```

## Configuración

- **Preferencias:** `Configuración > Preferencias…` (`Ctrl+,`). Se guardan en `%APPDATA%\Pluma\pluma.ini`; si existe un `pluma.ini` junto a `pluma.exe`, se usa ese archivo (modo portable).
- **Panel de encabezados:** `Ver > Panel de encabezados` (`Ctrl+Shift+E`). Se redimensiona arrastrando su borde derecho (doble clic restablece el ancho).
- **Editor predeterminado de Markdown:** `Configuración > Establecer como editor predeterminado de Markdown…` registra Pluma para `.md`, `.markdown` y `.mdown` (sin permisos de administrador) y abre el selector de Windows para confirmar el cambio.

## Benchmarks de Rendimiento

Para ejecutar la medición automatizada del arranque en frío (NF-01):

```powershell
.\bench\startup.ps1 -ExePath "build\release\bin\pluma.exe" -FailOnHardLimit
```

## Arquitectura y Dependencias

- **Editor:** Scintilla 5.5.3 + Lexilla 5.4.3 (enlazados estáticamente, en `third_party/`).
- **Parser Markdown:** md4c 0.5.2 (CommonMark 0.31 + GFM, en `third_party/md4c`).
- **Renderizado de vista previa:** Direct2D y DirectWrite nativos.
- **Diagramas Mermaid:** motor nativo propio (`src/diagram/`) para `flowchart`/`graph`, `sequenceDiagram`, `stateDiagram` y `pie`; se dibujan en la vista previa (Direct2D), como SVG inline en HTML y como vectores en PDF. Los demás tipos se muestran como código con un aviso.
- **Acceso a Win32:** API Unicode (`W`), Per-Monitor DPI Aware v2, tema oscuro vía DWM.
- **Sin runtimes externos:** Solamente DLLs nativas del sistema operativo de Windows.
