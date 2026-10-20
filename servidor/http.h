/* http.h — Servidor HTTP/1.1 mínimo sobre sockets POSIX.
 *
 * Se implementa a mano porque en esta máquina no se puede instalar nada
 * del sistema (sudo pide password), y `libmicrohttpd` no está disponible.
 * Ver ADR-003 en notas/06-bitacora-y-decisiones.md.
 *
 * Alcance deliberado: sólo lo que necesita este proyecto.
 *   - GET  y POST
 *   - cuerpo de la petición leído por Content-Length, con tope de tamaño
 *   - un hilo por conexión (pthreads)
 *   - serveo de archivos estáticos con protección contra path traversal
 */
#ifndef SIM_HTTP_H
#define SIM_HTTP_H

#include <stddef.h>

#define HTTP_MAX_CABECERA   8192
#define HTTP_MAX_CUERPO     (4 * 1024 * 1024)
#define HTTP_MAX_LINEA      8192

typedef struct {
    char metodo[8];
    char ruta[2048];
    char version[16];
    char *cuerpo;            /* NULL si no hay cuerpo; el llamador lo libera */
    size_t longitud_cuerpo;
} peticion_t;

/* Lee y parsea una petición de un socket ya aceptado.
 * Devuelve 1 si se leyó bien, 0 si la conexión se cerró, -1 si hay error. */
int http_leer_peticion(int fd, peticion_t *p);

/* Envía una respuesta. `cuerpo` puede ser NULL si no hay cuerpo. */
int http_responder(int fd, int codigo, const char *tipo_contenido,
                   const char *cuerpo, size_t longitud);

/* Envía una respuesta de error en JSON con un mensaje. */
int http_responder_error(int fd, int codigo, const char *mensaje);

/* Sirve un archivo estático desde `raiz`. Devuelve 1 si lo sirvió. */
int http_servir_estatico(int fd, const char *raiz, const char *ruta);

/* Traduce el nombre de un código HTTP a su texto. */
const char *http_texto_estado(int codigo);

/* El tipo MIME según la extensión del archivo. */
const char *http_tipo_mime(const char *ruta);

#endif /* SIM_HTTP_H */
