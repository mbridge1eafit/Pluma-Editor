# Notas para la certificación

Textos para **Partner Center > envío > Opciones de envío**. Los revisores de Microsoft trabajan en inglés, así que se entregan en inglés. Abajo va la traducción para ti.

## Notas para la certificación

Campo *Notes for certification*:

```
Pluma is a native Win32 (C++) Markdown editor packaged as MSIX. It needs no account, sign-in, license key or internet connection.

How to test:
1. Launch Pluma from Start. The user interface is in Spanish.
2. Type Markdown in the editor (left). The formatted preview updates on the right.
3. Menu "Archivo" (File): "Nuevo" (New), "Abrir..." (Open), "Guardar" (Save), "Exportar" (Export to HTML or PDF).
4. Menu "Ver" (View): editor only, split view, preview only (Ctrl+1 / Ctrl+2 / Ctrl+3), headings panel and file explorer.
5. Double-click any .md file in File Explorer: it opens in Pluma (file type association declared in the manifest).

Updates are delivered only through the Microsoft Store: the Store build has no self-updater and makes no network connections. Links inside documents open in the default browser.
```

## Justificación de `runFullTrust`

Campo *Why do you need the runFullTrust capability?*:

```
Pluma is a classic Win32 desktop application (C++, Win32 API, Direct2D/DirectWrite) packaged with MSIX. runFullTrust is required to run its desktop executable (EntryPoint Windows.FullTrustApplication). It runs as the interactive user, never requires elevation, and uses it only for standard desktop app behavior: opening and saving the documents the user chooses and exporting them to HTML or PDF.
```

## Traducción

**Notas para la certificación.** Pluma es un editor Markdown Win32 nativo (C++) empaquetado como MSIX. No necesita cuenta, inicio de sesión, clave de licencia ni conexión a internet.

Para probarlo:
1. Abre Pluma desde Inicio. La interfaz está en español.
2. Escribe Markdown en el editor, a la izquierda. La vista previa se actualiza a la derecha.
3. Menú *Archivo*: Nuevo, Abrir, Guardar y Exportar (a HTML o PDF).
4. Menú *Ver*: solo editor, vista dividida o solo vista previa (Ctrl+1, Ctrl+2, Ctrl+3), el panel de encabezados y el explorador de archivos.
5. Doble clic en un `.md` del Explorador de archivos: se abre en Pluma, porque la asociación está declarada en el manifiesto.

Las actualizaciones llegan solo por Microsoft Store: la versión de la Store no trae actualizador propio ni se conecta a internet. Los enlaces de los documentos se abren en el navegador predeterminado.

**Justificación de `runFullTrust`.** Pluma es una aplicación de escritorio Win32 clásica (C++, API Win32, Direct2D y DirectWrite) empaquetada con MSIX. Necesita `runFullTrust` para ejecutar su ejecutable de escritorio (EntryPoint `Windows.FullTrustApplication`). Corre como el usuario interactivo y nunca pide elevación. Solo hace lo normal en una aplicación de escritorio: abrir y guardar los documentos que el usuario elige y exportarlos a HTML o PDF.
