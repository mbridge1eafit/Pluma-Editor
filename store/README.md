# Publicar Pluma en Microsoft Store

Todo lo necesario para publicar Pluma en Microsoft Store: textos, imágenes y pasos. La Store firma el paquete MSIX con un certificado de Microsoft, así que Windows (SmartScreen y el Control inteligente de aplicaciones) no bloquea la versión de la Store. El motivo está en la Decisión 017 de `docs/DECISIONS.md`.

## Contenido de esta carpeta

| Archivo | Para qué sirve |
|---|---|
| `listing/es.md` | Textos de la ficha: nombre, descripción, características, leyendas de las capturas. |
| `images/screenshots/*.png` | Cinco capturas de 1920 × 1080 para la ficha. |
| `images/app-tile-icon-300x300.png` | Icono 1:1 de la ficha (muy recomendado). |
| `images/super-hero-art-1920x1080.png` | Imagen principal 16:9 de la ficha (recomendada, sin texto ni interfaz). |
| `properties.md` | Precio, disponibilidad, categoría, URL de soporte y privacidad, declaraciones y requisitos. |
| `age-rating.md` | Respuestas del cuestionario de clasificación por edad (IARC). |
| `certification-notes.md` | Notas para los revisores y justificación de `runFullTrust`. |
| `privacy-policy.md` | Política de privacidad. Su URL pública en GitHub va en Partner Center. |
| `screenshots/documentos-de-ejemplo/` | Documentos usados en las capturas, para repetirlas. |

El paquete lo genera `scripts/package_msix.ps1` a partir de `packaging/msix/` (manifiesto e identidad).

## Datos de la app en la Store

| Dato | Valor |
|---|---|
| Nombre reservado | Pluma Markdown Editor |
| Store ID | `9NJF1JRF4CXG` |
| Enlace (funciona cuando la app está publicada) | <https://apps.microsoft.com/detail/9NJF1JRF4CXG> |
| Enlace de protocolo | `ms-windows-store://pdp/?productid=9NJF1JRF4CXG` |
| Package Family Name | `mBridge.PlumaMarkdownEditor_r27zwack9q2bm` |
| AUMID de Pluma empaquetado | `mBridge.PlumaMarkdownEditor_r27zwack9q2bm!Pluma` |

## Primera publicación

### 1. Cuenta de desarrollador (gratis)

1. Entra en <https://storedeveloper.microsoft.com> con tu cuenta Microsoft personal.
2. Regístrate como **desarrollador individual**: no tiene costo ni pide tarjeta.
3. Completa la verificación de identidad que te pida Partner Center.

### 2. Reservar el nombre

En Partner Center: **Aplicaciones y juegos > Nueva aplicación**. El nombre reservado es **Pluma Markdown Editor**. Si cambias el nombre, actualízalo en `displayName` de `packaging/msix/identity.json` y en `listing/es.md`: el nombre del paquete y el de la reserva deben coincidir.

### 3. Copiar la identidad del paquete

En **Pluma Markdown Editor > Administración de productos > Identidad del producto** copia estos valores a `packaging/msix/identity.json` (ya están copiados):

| Partner Center | `identity.json` |
|---|---|
| `Package/Identity/Name` | `identityName` |
| `Package/Identity/Publisher` (empieza por `CN=`) | `publisher` |
| `Package/Properties/PublisherDisplayName` | `publisherDisplayName` |
| Nombre reservado | `displayName` |

Haz commit de ese archivo. No son secretos: cualquiera los ve en el paquete publicado.

### 4. Publicar la política de privacidad

Haz push de `store/privacy-policy.md` a `master`. Su URL, `https://github.com/mbridge1eafit/Pluma-Editor/blob/master/store/privacy-policy.md`, va en *Propiedades*.

### 5. Generar el paquete

Desde **Developer PowerShell for VS**, en la carpeta del proyecto:

```powershell
cmake --preset release
cmake --build --preset release
.\scripts\package_msix.ps1 -RequireStoreIdentity
```

El resultado es `dist\pluma-vX.Y.Z-x64.msix`. También lo genera el workflow **Release** de GitHub en cada tag: queda en los artefactos de la ejecución, como `pluma-vX.Y.Z-msix`. El paquete va sin firmar a propósito, porque la Store lo firma al publicarlo.

`-RequireStoreIdentity` hace fallar el script si `identity.json` sigue con valores `PENDIENTE`. Partner Center también rechaza un paquete cuya identidad no coincide con la de la aplicación.

### 6. Crear el envío

En **Pluma > Envío 1**, completa cada sección:

| Sección | Qué poner |
|---|---|
| Precio y disponibilidad | `properties.md` > *Precio y disponibilidad*. En el primer envío conviene elegir *solo vínculo directo*, para probar. |
| Propiedades | `properties.md` > *Propiedades*. |
| Clasificaciones por edad | `age-rating.md`. |
| Paquetes | Arrastra `pluma-vX.Y.Z-x64.msix`. Partner Center valida el manifiesto y detecta el idioma (español). |
| Listados de la Store > Español | `listing/es.md`, las capturas y las imágenes de `images/`. |
| Opciones de envío | `certification-notes.md`: notas para los revisores y justificación de `runFullTrust`. |

Pulsa **Enviar a la Store**. La certificación suele tardar desde unas horas hasta tres días hábiles. Si la rechazan, el informe explica el motivo.

### 7. Probar y hacer pública

1. Con la visibilidad *solo vínculo directo*, instala Pluma desde el enlace de la Store (*Identidad del producto > URL*) en este portátil. Comprueba que el Control inteligente de aplicaciones ya no lo bloquea, que los `.md` se abren con doble clic y que no aparece *Ayuda > Buscar actualizaciones*.
2. Crea un envío nuevo cambiando la visibilidad a **Público**.

## Cada versión nueva

Con Claude Code, la skill `/msstore-publish` hace estos pasos: descarga y valida el MSIX del release, prepara el envío en Partner Center desde Chrome y te pide confirmación antes de enviarlo a certificación. También sirve para cambiar la ficha, hacer visible la app o consultar el estado de la certificación. A mano:

1. Publica el release de GitHub como siempre (skill `github-release`).
2. Descarga el artefacto `pluma-vX.Y.Z-msix` del workflow **Release**, o genéralo con `package_msix.ps1 -RequireStoreIdentity`.
3. En Partner Center, **Actualizar** el envío:
   - Reemplaza el paquete.
   - Escribe *Novedades de esta versión* en `listing/es.md` (resumen de `dist/RELEASE_NOTES.md`, sin URL).
   - Si cambió la interfaz, actualiza las capturas.
4. Envía. Windows actualiza Pluma solo en los equipos que lo instalaron desde la Store.

La versión del MSIX sale de `project(VERSION)`, con el cuarto número en 0 como exige la Store, y cada envío debe tener una versión mayor que el anterior.

## Diferencias de la versión de la Store

- **Actualizaciones:** las hace la Store. Pluma empaquetado oculta *Ayuda > Buscar actualizaciones* y la opción de Preferencias, y no consulta GitHub.
- **Editor predeterminado:** las extensiones `.md`, `.markdown` y `.mdown` están declaradas en el manifiesto. *Establecer como editor predeterminado* solo abre la página de Pluma en *Configuración > Aplicaciones predeterminadas*.
- **Carpeta de instalación:** la gestiona Windows (`WindowsApps`) y es de solo lectura, así que no hay modo portable.
- **Configuración:** Pluma sigue escribiendo `%APPDATA%\Pluma\pluma.ini`, pero según la versión de Windows puede quedar en la carpeta privada del paquete (`%LOCALAPPDATA%\Packages\<paquete>\LocalCache`). Por eso la versión de la Store y la del instalador pueden no compartir preferencias.
- **Otros canales:** el ZIP y el instalador de GitHub siguen existiendo sin firmar, para quien no use la Store.

## Repetir las capturas

Las capturas actuales se tomaron con Pluma 0.6.1: el Control inteligente de aplicaciones de este equipo bloquea las compilaciones locales sin firmar. Con la 0.7 de la Store instalada:

1. Abre los documentos de `screenshots/documentos-de-ejemplo/Proyecto Aurora/`.
2. Pon la ventana a 1920 × 1080 píxeles (el tamaño mínimo es 1366 × 768).
3. Captura en tema claro y oscuro: vista dividida con el explorador, un diagrama de secuencia en tema oscuro, la tabla y el gráfico de `Presupuesto.md`, el explorador en árbol con el panel de encabezados, y solo vista previa.
