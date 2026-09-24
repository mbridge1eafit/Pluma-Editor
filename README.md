# Pluma — Editor Markdown nativo para Windows

Pluma es un editor de Markdown 100 % nativo para Windows diseñado para abrir en menos de 100 ms y ocupar menos de 3 MB en disco, con vista previa renderizada sin navegador embebido (sin Chromium, sin WebView2, sin .NET).

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

## Benchmarks de Rendimiento

Para ejecutar la medición automatizada del arranque en frío (NF-01):

```powershell
.\bench\startup.ps1 -ExePath "build\release\bin\pluma.exe" -FailOnHardLimit
```

## Arquitectura y Dependencias

- **Editor:** Scintilla 5.5.3 + Lexilla 5.4.3 (enlazados estáticamente, en `third_party/`).
- **Parser Markdown:** md4c 0.5.2 (CommonMark 0.31 + GFM, en `third_party/md4c`).
- **Renderizado de vista previa:** Direct2D y DirectWrite nativos.
- **Acceso a Win32:** API Unicode (`W`), Per-Monitor DPI Aware v2, tema oscuro vía DWM.
- **Sin runtimes externos:** Solamente DLLs nativas del sistema operativo de Windows.
