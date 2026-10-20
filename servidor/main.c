/* servidor/main.c — Servidor HTTP del simulador.
 *
 * Un solo proceso sirve la API y la interfaz web compilada.
 *
 *   simulador-web [--puerto N] [--raiz-web RUTA]
 */
#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "http.h"
#include "rutas.h"

typedef struct {
    int    fd;
    char   raiz_web[1024];
} conexion_t;

/* Un hilo por conexión. Como el servidor es local y las conexiones son
 * cortas, no hace falta un pool. */
static void *atender_conexion(void *arg)
{
    conexion_t *c = (conexion_t *)arg;
    peticion_t p;
    int rc;

    rc = http_leer_peticion(c->fd, &p);
    if (rc == 1) {
        if (strcmp(p.metodo, "OPTIONS") == 0) {
            /* Preflight de CORS. */
            http_responder(c->fd, 200, "text/plain", "", 0);
        } else if (rutas_atender(c->fd, c->raiz_web, p.metodo, p.ruta,
                                 p.cuerpo, p.longitud_cuerpo)) {
            /* La API ya respondió. */
        } else if (!http_servir_estatico(c->fd, c->raiz_web, p.ruta)) {
            /* No era una API ni un archivo: hay que responder 404. Sin esto
             * el cliente se quedaría esperando una respuesta que nunca
             * llega. */
            http_responder_error(c->fd, 404, "recurso no encontrado");
        }
        free(p.cuerpo);
    } else if (rc == -2) {
        http_responder_error(c->fd, 413, "el cuerpo es demasiado grande");
    }

    shutdown(c->fd, SHUT_RDWR);
    close(c->fd);
    free(c);
    return NULL;
}

int main(int argc, char **argv)
{
    int puerto = 8080;
    char raiz_web[1024];
    int fd;
    struct sockaddr_in direccion;
    int opt = 1;
    int i;

    snprintf(raiz_web, sizeof(raiz_web), "web/dist");

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--puerto") == 0 && i + 1 < argc) {
            puerto = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--raiz-web") == 0 && i + 1 < argc) {
            snprintf(raiz_web, sizeof(raiz_web), "%s", argv[++i]);
        } else if (strcmp(argv[i], "--help") == 0) {
            printf("Uso: simulador-web [--puerto N] [--raiz-web RUTA]\n");
            return 0;
        } else {
            fprintf(stderr, "Opción desconocida: %s\n", argv[i]);
            return 2;
        }
    }

    if (puerto < 1 || puerto > 65535) {
        fprintf(stderr, "Puerto inválido: %d\n", puerto);
        return 2;
    }

    /* Se ignoran SIGPIPE: si el cliente se desconecta a media respuesta, el
     * servidor no debe morir. */
    signal(SIGPIPE, SIG_IGN);

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return 1;
    }
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&direccion, 0, sizeof(direccion));
    direccion.sin_family = AF_INET;
    direccion.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    direccion.sin_port = htons((uint16_t)puerto);

    if (bind(fd, (struct sockaddr *)&direccion, sizeof(direccion)) < 0) {
        fprintf(stderr, "No se pudo hacer bind en el puerto %d: %s\n",
                puerto, strerror(errno));
        close(fd);
        return 1;
    }
    if (listen(fd, 32) < 0) {
        perror("listen");
        close(fd);
        return 1;
    }

    printf("Simulador de Taller de Reparación\n");
    printf("  API:     http://127.0.0.1:%d/api/salud\n", puerto);
    printf("  Interfaz http://127.0.0.1:%d/\n", puerto);
    printf("  Raíz web: %s\n", raiz_web);
    printf("  Ctrl+C para detener.\n");
    fflush(stdout);

    for (;;) {
        int cliente = accept(fd, NULL, NULL);
        conexion_t *c;
        pthread_t hilo;

        if (cliente < 0) {
            if (errno == EINTR) continue;
            perror("accept");
            break;
        }

        c = (conexion_t *)calloc(1, sizeof(conexion_t));
        if (c == NULL) {
            close(cliente);
            continue;
        }
        c->fd = cliente;
        snprintf(c->raiz_web, sizeof(c->raiz_web), "%s", raiz_web);

        if (pthread_create(&hilo, NULL, atender_conexion, c) != 0) {
            close(cliente);
            free(c);
            continue;
        }
        pthread_detach(hilo);
    }

    close(fd);
    return 0;
}
