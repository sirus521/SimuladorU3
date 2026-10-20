/* http.c — ver http.h */
#define _POSIX_C_SOURCE 200809L

#include "http.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

const char *http_texto_estado(int codigo)
{
    switch (codigo) {
        case 200: return "OK";
        case 400: return "Bad Request";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 413: return "Payload Too Large";
        case 500: return "Internal Server Error";
        default:  return "Error";
    }
}

const char *http_tipo_mime(const char *ruta)
{
    const char *punto = strrchr(ruta, '.');
    if (punto == NULL) return "application/octet-stream";
    if (strcmp(punto, ".html") == 0 || strcmp(punto, ".htm") == 0) return "text/html; charset=utf-8";
    if (strcmp(punto, ".js") == 0 || strcmp(punto, ".mjs") == 0) return "text/javascript; charset=utf-8";
    if (strcmp(punto, ".css") == 0) return "text/css; charset=utf-8";
    if (strcmp(punto, ".json") == 0) return "application/json; charset=utf-8";
    if (strcmp(punto, ".svg") == 0) return "image/svg+xml";
    if (strcmp(punto, ".png") == 0) return "image/png";
    if (strcmp(punto, ".jpg") == 0 || strcmp(punto, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(punto, ".ico") == 0) return "image/x-icon";
    if (strcmp(punto, ".woff2") == 0) return "font/woff2";
    if (strcmp(punto, ".txt") == 0) return "text/plain; charset=utf-8";
    return "application/octet-stream";
}

/* Lee una línea terminada en \n del socket. Devuelve la longitud sin el \n,
 * o -1 si hay error o se cerró la conexión. */
static int leer_linea(int fd, char *buffer, size_t maximo)
{
    size_t largo = 0;
    while (largo + 1 < maximo) {
        char c;
        ssize_t n = recv(fd, &c, 1, 0);
        if (n <= 0) {
            if (largo == 0) return -1;
            break;
        }
        if (c == '\n') break;
        if (c != '\r') {
            buffer[largo++] = c;
        }
    }
    buffer[largo] = '\0';
    return (int)largo;
}

int http_leer_peticion(int fd, peticion_t *p)
{
    char linea[HTTP_MAX_LINEA];
    char *contenido = NULL;
    size_t longitud = 0;
    long content_length = -1;

    memset(p, 0, sizeof(*p));

    /* Línea de petición: MÉTODO RUTA VERSIÓN */
    if (leer_linea(fd, linea, sizeof(linea)) <= 0) {
        return 0;
    }
    if (sscanf(linea, "%7s %2047s %15s", p->metodo, p->ruta, p->version) < 2) {
        return -1;
    }

    /* Cabeceras. */
    for (;;) {
        int largo = leer_linea(fd, linea, sizeof(linea));
        if (largo < 0) {
            free(contenido);
            return 0;
        }
        if (largo == 0) break;   /* línea vacía: fin de cabeceras */

        if (strncasecmp(linea, "Content-Length:", 15) == 0) {
            content_length = strtol(linea + 15, NULL, 10);
            if (content_length < 0) {
                free(contenido);
                return -1;
            }
            if (content_length > HTTP_MAX_CUERPO) {
                free(contenido);
                return -2;   /* cuerpo demasiado grande */
            }
        }
    }

    /* Cuerpo. */
    if (content_length > 0) {
        size_t leidos = 0;
        contenido = (char *)malloc((size_t)content_length + 1);
        if (contenido == NULL) {
            return -1;
        }
        while (leidos < (size_t)content_length) {
            ssize_t n = recv(fd, contenido + leidos, (size_t)content_length - leidos, 0);
            if (n <= 0) {
                free(contenido);
                return 0;
            }
            leidos += (size_t)n;
        }
        contenido[leidos] = '\0';
        longitud = leidos;
    }

    p->cuerpo = contenido;
    p->longitud_cuerpo = longitud;
    return 1;
}

int http_responder(int fd, int codigo, const char *tipo_contenido,
                   const char *cuerpo, size_t longitud)
{
    char cabecera[1024];
    int n;
    size_t enviados = 0;

    if (cuerpo == NULL) {
        cuerpo = "";
        longitud = 0;
    }

    n = snprintf(cabecera, sizeof(cabecera),
                 "HTTP/1.1 %d %s\r\n"
                 "Content-Type: %s\r\n"
                 "Content-Length: %zu\r\n"
                 "Access-Control-Allow-Origin: *\r\n"
                 "Access-Control-Allow-Headers: Content-Type\r\n"
                 "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                 "Cache-Control: no-store\r\n"
                 "Connection: close\r\n"
                 "\r\n",
                 codigo, http_texto_estado(codigo), tipo_contenido, longitud);
    if (n < 0 || (size_t)n >= sizeof(cabecera)) {
        return -1;
    }
    if (send(fd, cabecera, (size_t)n, 0) < 0) {
        return -1;
    }
    while (enviados < longitud) {
        ssize_t k = send(fd, cuerpo + enviados, longitud - enviados, 0);
        if (k <= 0) {
            return -1;
        }
        enviados += (size_t)k;
    }
    return 0;
}

int http_responder_error(int fd, int codigo, const char *mensaje)
{
    char cuerpo[512];
    int n = snprintf(cuerpo, sizeof(cuerpo),
                     "{\"error\": \"%s\"}", mensaje ? mensaje : "error desconocido");
    if (n < 0) return -1;
    return http_responder(fd, codigo, "application/json; charset=utf-8", cuerpo, (size_t)n);
}

int http_servir_estatico(int fd, const char *raiz, const char *ruta)
{
    char completo[4096];
    const char *relativo = ruta;
    char *query;
    struct stat info;
    FILE *f;
    long tam;
    char *contenido;
    size_t leidos;
    int rc;

    /* Se quita la query string. */
    query = strchr((char *)ruta, '?');
    if (query != NULL) {
        static char copia[2048];
        size_t n = (size_t)(query - ruta);
        if (n >= sizeof(copia)) n = sizeof(copia) - 1;
        memcpy(copia, ruta, n);
        copia[n] = '\0';
        relativo = copia;
    }

    /* "/" sirve index.html. */
    if (strcmp(relativo, "/") == 0) {
        relativo = "/index.html";
    }

    /* Protección contra path traversal: la ruta no puede contener "..". */
    if (strstr(relativo, "..") != NULL) {
        return 0;
    }
    /* Tampoco puede contener bytes nulos ni barras invertidas. */
    if (strchr(relativo, '\\') != NULL) {
        return 0;
    }

    if (snprintf(completo, sizeof(completo), "%s%s", raiz, relativo) >= (int)sizeof(completo)) {
        return 0;
    }
    if (stat(completo, &info) != 0 || !S_ISREG(info.st_mode)) {
        return 0;
    }

    tam = (long)info.st_size;
    contenido = (char *)malloc((size_t)tam + 1);
    if (contenido == NULL) {
        return 0;
    }
    f = fopen(completo, "rb");
    if (f == NULL) {
        free(contenido);
        return 0;
    }
    leidos = fread(contenido, 1, (size_t)tam, f);
    fclose(f);

    rc = http_responder(fd, 200, http_tipo_mime(completo), contenido, leidos);
    free(contenido);
    return rc == 0 ? 1 : 0;
}
