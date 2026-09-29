# Propiedades, precio y disponibilidad

Valores para las secciones **Precio y disponibilidad** y **Propiedades** del envío en Partner Center.

## Precio y disponibilidad

| Campo | Valor |
|---|---|
| Mercados | Todos los mercados (Pluma no tiene restricciones legales ni de contenido). |
| Visibilidad (primer envío) | **Público privado** (*Private audience*), con tu cuenta Microsoft en un grupo de clientes conocidos. Así pruebas la versión firmada por la Store antes de hacerla pública. |
| Visibilidad (después) | **Público**, disponible y visible en la Store. |
| Programación | Publicar en cuanto pase la certificación. |
| Precio base | **Gratis**. |
| Prueba gratuita | No. |
| Descuentos (ventas) | Ninguno. |

## Propiedades

| Campo | Valor |
|---|---|
| Categoría | **Productividad** |
| Categoría secundaria | **Herramientas para desarrolladores** |
| ¿Accede, recopila o transmite información personal? | **No**. La versión de la Store no se conecta a internet. Ver `privacy-policy.md`. |
| URL de la directiva de privacidad | `https://github.com/mbridge1eafit/Pluma-Editor/blob/master/store/privacy-policy.md` (opcional con la respuesta «No», pero conviene incluirla). |
| Sitio web | `https://github.com/mbridge1eafit/Pluma-Editor` |
| Información de contacto de soporte | `https://github.com/mbridge1eafit/Pluma-Editor/issues` |
| Modo de visualización | Ninguna casilla: no es una aplicación de realidad mixta. |

La URL de la política de privacidad solo funciona cuando `store/privacy-policy.md` esté en la rama `master` de GitHub.

### Declaraciones del producto

- Deja marcadas las que vienen marcadas por defecto: instalar en otras unidades o almacenamiento extraíble, e incluir los datos de la app en las copias de seguridad automáticas de Windows.
- No marques ninguna otra:
  - Pluma no vende nada.
  - No usa controladores ni servicios NT de terceros.
  - No se ha certificado formalmente contra las pautas de accesibilidad.

### Requisitos del sistema

| Hardware | Valor |
|---|---|
| Teclado | Mínimo (es un editor de texto). |
| Mouse | Recomendado. |
| Los demás (táctil, cámara, NFC, Bluetooth, memoria, DirectX…) | Sin marcar. |

## Opciones de envío

| Campo | Valor |
|---|---|
| Opciones de publicación | Publicar automáticamente al pasar la certificación. |
| Notas para la certificación | Ver `certification-notes.md`. |
| Funcionalidades restringidas (`runFullTrust`) | Partner Center pide justificarla. Ver `certification-notes.md`. |
