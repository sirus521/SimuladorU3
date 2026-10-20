#define _POSIX_C_SOURCE 200809L

/* test_http.c — Contrato de la API y del servidor HTTP.
 *
 * Criterio de aceptación de la fase F5:
 *   - cada endpoint devuelve el status y el esquema esperados
 *   - 404 en ruta inexistente
 *   - 400 en JSON inválido y en parámetros que no pasan la validación
 *   - un cuerpo grande no rompe el servidor
 *   - el path traversal no sirve archivos fuera de la raíz
 *
 * Las pruebas levantan el servidor en un puerto libre y le hablan por
 * socket, como lo haría el navegador.
 */
#include "test_util.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "cjson/cJSON.h"

/* Espera activa de N milisegundos, sin depender de usleep (que no está en
 * el estándar POSIX.1-2008). */
static void esperar_ms(int ms)
{
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

static int puerto_global = 0;
static pid_t pid_servidor = 0;

/* Conecta al servidor local. */
static int conectar(void)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in dir;
    if (fd < 0) return -1;
    memset(&dir, 0, sizeof(dir));
    dir.sin_family = AF_INET;
    dir.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    dir.sin_port = htons((uint16_t)puerto_global);
    if (connect(fd, (struct sockaddr *)&dir, sizeof(dir)) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

/* Envía una petición y lee la respuesta completa. */
static int peticion(const char *metodo, const char *ruta, const char *cuerpo,
                     char *respuesta, size_t tam_respuesta)
{
    int fd = conectar();
    char peticion_texto[8192];
    size_t largo_cuerpo = cuerpo ? strlen(cuerpo) : 0;
    int n;
    size_t leidos = 0;

    if (fd < 0) return -1;

    n = snprintf(peticion_texto, sizeof(peticion_texto),
                 "%s %s HTTP/1.1\r\n"
                 "Host: 127.0.0.1\r\n"
                 "Content-Type: application/json\r\n"
                 "Content-Length: %zu\r\n"
                 "Connection: close\r\n"
                 "\r\n",
                 metodo, ruta, largo_cuerpo);
    if (n < 0) { close(fd); return -1; }
    if (send(fd, peticion_texto, (size_t)n, 0) < 0) { close(fd); return -1; }
    if (largo_cuerpo > 0) {
        if (send(fd, cuerpo, largo_cuerpo, 0) < 0) { close(fd); return -1; }
    }

    for (;;) {
        ssize_t k = recv(fd, respuesta + leidos, tam_respuesta - leidos - 1, 0);
        if (k <= 0) break;
        leidos += (size_t)k;
        if (leidos >= tam_respuesta - 1) break;
    }
    respuesta[leidos] = '\0';
    close(fd);
    return (int)leidos;
}

/* Extrae el código de estado de la primera línea. */
static int codigo_estado(const char *respuesta)
{
    if (respuesta == NULL) return -1;
    if (strncmp(respuesta, "HTTP/1.1 ", 9) != 0) return -1;
    return atoi(respuesta + 9);
}

/* Devuelve el cuerpo de la respuesta (lo que va tras la línea en blanco). */
static const char *cuerpo_respuesta(const char *respuesta)
{
    const char *p = strstr(respuesta, "\r\n\r\n");
    return p ? p + 4 : "";
}

/* --- Pruebas --------------------------------------------------------- */

static void prueba_salud(void)
{
    char respuesta[65536];
    int n = peticion("GET", "/api/salud", NULL, respuesta, sizeof(respuesta));
    cJSON *json;

    AFIRMAR(n > 0, "no se obtuvo respuesta");
    AFIRMAR(codigo_estado(respuesta) == 200, "el estado no es 200");
    json = cJSON_Parse(cuerpo_respuesta(respuesta));
    AFIRMAR(json != NULL, "la respuesta no es JSON valido");
    if (json != NULL) {
        const cJSON *estado = cJSON_GetObjectItemCaseSensitive(json, "estado");
        AFIRMAR(estado != NULL && cJSON_IsString(estado), "falta el campo 'estado'");
        if (estado != NULL && cJSON_IsString(estado)) {
            AFIRMAR(strcmp(estado->valuestring, "ok") == 0, "el estado no es 'ok'");
        }
        cJSON_Delete(json);
    }
}

static void prueba_escenario_por_defecto(void)
{
    char respuesta[65536];
    int n = peticion("GET", "/api/escenario", NULL, respuesta, sizeof(respuesta));
    cJSON *json;

    AFIRMAR(n > 0, "no se obtuvo respuesta");
    AFIRMAR(codigo_estado(respuesta) == 200, "el estado no es 200");
    json = cJSON_Parse(cuerpo_respuesta(respuesta));
    AFIRMAR(json != NULL, "la respuesta no es JSON valido");
    if (json != NULL) {
        const cJSON *v;
        AFIRMAR(cJSON_GetObjectItemCaseSensitive(json, "tipos") != NULL, "falta 'tipos'");
        v = cJSON_GetObjectItemCaseSensitive(json, "bahias_diagnostico");
        AFIRMAR(v != NULL && cJSON_IsNumber(v), "falta 'bahias_diagnostico'");
        v = cJSON_GetObjectItemCaseSensitive(json, "dias_calentamiento");
        AFIRMAR(v != NULL && cJSON_IsNumber(v), "falta 'dias_calentamiento'");
        cJSON_Delete(json);
    }
}

static void prueba_simular(void)
{
    char respuesta[1024 * 1024];
    const char *cuerpo = "{\"llegadas_por_dia\":12,\"dias\":5,\"dias_calentamiento\":1,"
                         "\"replicas\":3,\"bahias_diagnostico\":2,\"mecanicos\":3,"
                         "\"refacciones_stock\":1000,\"refacciones_lote\":0}";
    int n = peticion("POST", "/api/simular", cuerpo, respuesta, sizeof(respuesta));
    cJSON *json;

    AFIRMAR(n > 0, "no se obtuvo respuesta");
    AFIRMAR(codigo_estado(respuesta) == 200, "el estado no es 200");
    json = cJSON_Parse(cuerpo_respuesta(respuesta));
    AFIRMAR(json != NULL, "la respuesta no es JSON valido");
    if (json != NULL) {
        const cJSON *resumen = cJSON_GetObjectItemCaseSensitive(json, "resumen");
        const cJSON *v;
        cJSON *series;
        AFIRMAR(resumen != NULL, "falta 'resumen'");
        if (resumen != NULL) {
            /* Cada métrica viene como {valor, ic95}: el intervalo va dentro
             * del objeto, no en un campo plano aparte. */
            v = cJSON_GetObjectItemCaseSensitive(resumen, "tpr_medio");
            AFIRMAR(v != NULL && cJSON_IsObject(v), "falta 'tpr_medio'");
            if (v != NULL && cJSON_IsObject(v)) {
                const cJSON *valor = cJSON_GetObjectItemCaseSensitive(v, "valor");
                const cJSON *ic = cJSON_GetObjectItemCaseSensitive(v, "ic95");
                AFIRMAR(valor != NULL && cJSON_IsNumber(valor),
                        "falta 'valor' dentro de 'tpr_medio'");
                AFIRMAR(ic != NULL && cJSON_IsNumber(ic),
                        "falta 'ic95' dentro de 'tpr_medio'");
            }
            /* El formato plano NO debe seguir existiendo: si aparece, el
             * contrato se rompió. */
            AFIRMAR(cJSON_GetObjectItemCaseSensitive(resumen, "tpr_medio_ic95") == NULL,
                    "el formato plano 'tpr_medio_ic95' no debe existir");
        }
        AFIRMAR(cJSON_GetObjectItemCaseSensitive(json, "validacion") != NULL,
                "falta 'validacion'");
        AFIRMAR(cJSON_GetObjectItemCaseSensitive(json, "histograma") != NULL,
                "falta 'histograma'");
        series = cJSON_GetObjectItemCaseSensitive(json, "series");
        AFIRMAR(series != NULL && cJSON_IsArray(series), "falta 'series'");
        if (series != NULL && cJSON_IsArray(series)) {
            AFIRMAR(cJSON_GetArraySize(series) > 0, "la serie está vacía");
        }
        cJSON_Delete(json);
    }
}

static void prueba_json_invalido(void)
{
    char respuesta[65536];
    int n = peticion("POST", "/api/simular", "{esto no es json", respuesta, sizeof(respuesta));
    AFIRMAR(n > 0, "no se obtuvo respuesta");
    AFIRMAR(codigo_estado(respuesta) == 400, "un JSON invalido debe dar 400");
}

static void prueba_parametros_invalidos(void)
{
    char respuesta[65536];
    const char *cuerpo = "{\"bahias_diagnostico\":0}";
    int n = peticion("POST", "/api/simular", cuerpo, respuesta, sizeof(respuesta));
    AFIRMAR(n > 0, "no se obtuvo respuesta");
    AFIRMAR(codigo_estado(respuesta) == 400, "parámetros invalidos deben dar 400");
    AFIRMAR(strstr(cuerpo_respuesta(respuesta), "error") != NULL,
            "la respuesta de error debe traer un mensaje");
}

static void prueba_metodo_incorrecto(void)
{
    char respuesta[65536];
    int n = peticion("GET", "/api/simular", NULL, respuesta, sizeof(respuesta));
    AFIRMAR(n > 0, "no se obtuvo respuesta");
    AFIRMAR(codigo_estado(respuesta) == 405, "un metodo incorrecto debe dar 405");
}

static void prueba_ruta_desconocida(void)
{
    char respuesta[65536];
    int n = peticion("GET", "/api/no-existe", NULL, respuesta, sizeof(respuesta));
    AFIRMAR(n > 0, "no se obtuvo respuesta");
    AFIRMAR(codigo_estado(respuesta) == 404, "una ruta desconocida debe dar 404");
}

static void prueba_cuerpo_grande(void)
{
    char respuesta[65536];
    char *cuerpo = (char *)malloc(6 * 1024 * 1024);
    int n;

    AFIRMAR(cuerpo != NULL, "sin memoria para la prueba");
    if (cuerpo == NULL) return;
    memset(cuerpo, 'a', 6 * 1024 * 1024 - 1);
    cuerpo[6 * 1024 * 1024 - 1] = '\0';

    n = peticion("POST", "/api/simular", cuerpo, respuesta, sizeof(respuesta));
    AFIRMAR(n > 0, "no se obtuvo respuesta");
    AFIRMAR(codigo_estado(respuesta) == 413,
            "un cuerpo demasiado grande debe dar 413");
    free(cuerpo);
}

static void prueba_path_traversal(void)
{
    char respuesta[65536];
    int n = peticion("GET", "/../Makefile", NULL, respuesta, sizeof(respuesta));
    AFIRMAR(n > 0, "no se obtuvo respuesta");
    /* Debe ser 404: no se sirve nada fuera de la raíz web. */
    AFIRMAR(codigo_estado(respuesta) == 404, "el path traversal debe dar 404");
    AFIRMAR(strstr(cuerpo_respuesta(respuesta), "CFLAGS") == NULL,
            "el path traversal se filtró al sistema de archivos");
}

static void prueba_escenarios_listados(void)
{
    char respuesta[65536];
    int n = peticion("GET", "/api/escenarios", NULL, respuesta, sizeof(respuesta));
    cJSON *json;
    AFIRMAR(n > 0, "no se obtuvo respuesta");
    AFIRMAR(codigo_estado(respuesta) == 200, "el estado no es 200");
    json = cJSON_Parse(cuerpo_respuesta(respuesta));
    AFIRMAR(json != NULL, "la respuesta no es JSON valido");
    if (json != NULL) {
        const cJSON *lista = cJSON_GetObjectItemCaseSensitive(json, "escenarios");
        AFIRMAR(lista != NULL && cJSON_IsArray(lista), "falta la lista de escenarios");
        cJSON_Delete(json);
    }
}

int main(void)
{
    char puerto_texto[16];

    printf("  Servidor HTTP\n");

    /* Puerto alto y fijo. La probabilidad de colisión es baja y el
     * proceso es local; si el servidor no levanta, las pruebas fallan con
     * mensajes claros. */
    signal(SIGCHLD, SIG_IGN);
    puerto_global = 18123;
    snprintf(puerto_texto, sizeof(puerto_texto), "%d", puerto_global);

    pid_servidor = fork();
    if (pid_servidor == 0) {
        /* Hijo: el servidor. */
        execl("./bin/simulador-web", "simulador-web",
              "--puerto", puerto_texto,
              "--raiz-web", "web/dist",
              (char *)NULL);
        _exit(127);
    }
    if (pid_servidor < 0) {
        printf("   no se pudo lanzar el servidor\n");
        return 1;
    }

    /* Espera a que el servidor acepte conexiones. */
    {
        int intentos;
        for (intentos = 0; intentos < 100; intentos++) {
            int fd = conectar();
            if (fd >= 0) {
                close(fd);
                break;
            }
            esperar_ms(50);
        }
    }

    prueba_salud();
    prueba_escenario_por_defecto();
    prueba_escenarios_listados();
    prueba_simular();
    prueba_json_invalido();
    prueba_parametros_invalidos();
    prueba_metodo_incorrecto();
    prueba_ruta_desconocida();
    prueba_cuerpo_grande();
    prueba_path_traversal();

    kill(pid_servidor, SIGTERM);
    waitpid(pid_servidor, NULL, 0);

    return test_resumen("http");
}
