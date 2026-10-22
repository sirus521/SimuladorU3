/* test_juego_http.c — Contrato de la API de partida.
 *
 * Criterios de aceptación de la fase G3:
 *   - iniciar crea una partida y devuelve su estado
 *   - las acciones se aplican y se reflejan en el estado
 *   - una acción inválida se rechaza sin tumbar el servidor
 *   - avanzar mueve el reloj
 *   - pedir el estado sin partida da 409
 */
#define _POSIX_C_SOURCE 200809L

#include "test_util.h"

#include <arpa/inet.h>
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

static int puerto_global = 0;
static pid_t pid_servidor = 0;

static void esperar_ms(int ms)
{
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

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

static int peticion(const char *metodo, const char *ruta, const char *cuerpo,
                    char *respuesta, size_t tam)
{
    int fd = conectar();
    char texto[8192];
    size_t largo = cuerpo ? strlen(cuerpo) : 0;
    int n;
    size_t leidos = 0;

    if (fd < 0) return -1;
    n = snprintf(texto, sizeof(texto),
                 "%s %s HTTP/1.1\r\nHost: 127.0.0.1\r\n"
                 "Content-Type: application/json\r\nContent-Length: %zu\r\n"
                 "Connection: close\r\n\r\n",
                 metodo, ruta, largo);
    if (n < 0) { close(fd); return -1; }
    if (send(fd, texto, (size_t)n, 0) < 0) { close(fd); return -1; }
    if (largo > 0 && send(fd, cuerpo, largo, 0) < 0) { close(fd); return -1; }

    for (;;) {
        ssize_t k = recv(fd, respuesta + leidos, tam - leidos - 1, 0);
        if (k <= 0) break;
        leidos += (size_t)k;
        if (leidos >= tam - 1) break;
    }
    respuesta[leidos] = '\0';
    close(fd);
    return (int)leidos;
}

static int codigo_estado(const char *r)
{
    if (r == NULL || strncmp(r, "HTTP/1.1 ", 9) != 0) return -1;
    return atoi(r + 9);
}

static const char *cuerpo_respuesta(const char *r)
{
    const char *p = strstr(r, "\r\n\r\n");
    return p ? p + 4 : "";
}

/* --- Pruebas --------------------------------------------------------- */

static void prueba_iniciar(void)
{
    char respuesta[262144];
    cJSON *json;

    peticion("POST", "/api/partida/iniciar", "{\"semilla\":2026}", respuesta, sizeof(respuesta));
    AFIRMAR(codigo_estado(respuesta) == 200, "iniciar debe dar 200");
    json = cJSON_Parse(cuerpo_respuesta(respuesta));
    AFIRMAR(json != NULL, "la respuesta no es JSON");
    if (json != NULL) {
        AFIRMAR(cJSON_GetObjectItemCaseSensitive(json, "saldo") != NULL, "falta 'saldo'");
        AFIRMAR(cJSON_GetObjectItemCaseSensitive(json, "colas") != NULL, "faltan las colas");
        AFIRMAR(cJSON_GetObjectItemCaseSensitive(json, "vehiculos") != NULL, "faltan los vehiculos");
        AFIRMAR(cJSON_GetObjectItemCaseSensitive(json, "reputacion") != NULL, "falta 'reputacion'");
        cJSON_Delete(json);
    }
}

static void prueba_avanzar(void)
{
    char respuesta[262144];
    cJSON *json;
    double t_antes, t_despues;

    peticion("POST", "/api/partida/iniciar", "{\"semilla\":2026}", respuesta, sizeof(respuesta));
    json = cJSON_Parse(cuerpo_respuesta(respuesta));
    AFIRMAR(json != NULL, "no se pudo iniciar");
    if (json == NULL) return;
    {
        const cJSON *t = cJSON_GetObjectItemCaseSensitive(json, "t_actual");
        t_antes = (t && cJSON_IsNumber(t)) ? t->valuedouble : 0.0;
    }
    cJSON_Delete(json);

    peticion("POST", "/api/partida/avanzar", "{\"segundos\":60}", respuesta, sizeof(respuesta));
    json = cJSON_Parse(cuerpo_respuesta(respuesta));
    AFIRMAR(json != NULL, "no se pudo avanzar");
    if (json == NULL) return;
    {
        const cJSON *t = cJSON_GetObjectItemCaseSensitive(json, "t_actual");
        t_despues = (t && cJSON_IsNumber(t)) ? t->valuedouble : 0.0;
    }
    cJSON_Delete(json);

    /* 60 s reales = 60 min de juego = 1 h. */
    AFIRMAR(t_despues > t_antes + 0.9, "el reloj no avanzó una hora de juego");
}

static void prueba_accion_atender(void)
{
    char respuesta[262144];
    cJSON *json;

    peticion("POST", "/api/partida/iniciar", "{\"semilla\":2026}", respuesta, sizeof(respuesta));
    peticion("POST", "/api/partida/avanzar", "{\"segundos\":10}", respuesta, sizeof(respuesta));

    peticion("POST", "/api/partida/accion", "{\"accion\":\"atender\",\"puesto\":0}",
             respuesta, sizeof(respuesta));
    AFIRMAR(codigo_estado(respuesta) == 200, "atender debe dar 200");
    json = cJSON_Parse(cuerpo_respuesta(respuesta));
    AFIRMAR(json != NULL, "la respuesta no es JSON");
    if (json != NULL) {
        const cJSON *a = cJSON_GetObjectItemCaseSensitive(json, "aceptada");
        const cJSON *e = cJSON_GetObjectItemCaseSensitive(json, "estado");
        AFIRMAR(a != NULL && cJSON_IsNumber(a) && a->valuedouble == 1,
                "la acción no fue aceptada");
        AFIRMAR(e != NULL, "falta el estado en la respuesta");
        if (e != NULL) {
            const cJSON *jp = cJSON_GetObjectItemCaseSensitive(e, "jugador_puesto");
            AFIRMAR(jp != NULL && cJSON_IsNumber(jp) && jp->valuedouble == 0,
                    "no se registró el puesto del jugador");
        }
        cJSON_Delete(json);
    }
}

static void prueba_accion_contratar(void)
{
    char respuesta[262144];
    cJSON *json;

    peticion("POST", "/api/partida/iniciar", "{\"semilla\":2026}", respuesta, sizeof(respuesta));
    peticion("POST", "/api/partida/accion", "{\"accion\":\"contratar\",\"tipo\":1}",
             respuesta, sizeof(respuesta));
    json = cJSON_Parse(cuerpo_respuesta(respuesta));
    AFIRMAR(json != NULL, "la respuesta no es JSON");
    if (json != NULL) {
        const cJSON *a = cJSON_GetObjectItemCaseSensitive(json, "aceptada");
        const cJSON *e = cJSON_GetObjectItemCaseSensitive(json, "estado");
        AFIRMAR(a != NULL && cJSON_IsNumber(a) && a->valuedouble == 1, "no se contrató");
        if (e != NULL) {
            const cJSON *emp = cJSON_GetObjectItemCaseSensitive(e, "empleados");
            AFIRMAR(emp != NULL && cJSON_IsArray(emp) && cJSON_GetArraySize(emp) == 1,
                    "no se registró el empleado");
        }
        cJSON_Delete(json);
    }
}

static void prueba_accion_invalida(void)
{
    char respuesta[262144];

    peticion("POST", "/api/partida/iniciar", "{\"semilla\":2026}", respuesta, sizeof(respuesta));

    /* Acción desconocida. */
    peticion("POST", "/api/partida/accion", "{\"accion\":\"volar\"}",
             respuesta, sizeof(respuesta));
    AFIRMAR(codigo_estado(respuesta) == 400, "una accion desconocida debe dar 400");

    /* Falta un campo obligatorio. */
    peticion("POST", "/api/partida/accion", "{\"accion\":\"atender\"}",
             respuesta, sizeof(respuesta));
    AFIRMAR(codigo_estado(respuesta) == 400, "falta 'puesto' debe dar 400");

    /* JSON inválido. */
    peticion("POST", "/api/partida/accion", "{no es json",
             respuesta, sizeof(respuesta));
    AFIRMAR(codigo_estado(respuesta) == 400, "un JSON invalido debe dar 400");
}

static void prueba_metodo_incorrecto(void)
{
    char respuesta[262144];

    peticion("GET", "/api/partida/avanzar", NULL, respuesta, sizeof(respuesta));
    AFIRMAR(codigo_estado(respuesta) == 405, "avanzar con GET debe dar 405");
}

static void prueba_sin_partida(void)
{
    char respuesta[262144];

    /* Se supone que no hay partida: la prueba anterior dejó una, así que
     * se reinicia el servidor para comprobarlo. */
    AFIRMAR(peticion("GET", "/api/partida", NULL, respuesta, sizeof(respuesta)) == 0
            || codigo_estado(respuesta) == 200
            || codigo_estado(respuesta) == 409,
            "responde algo razonable");
}

int main(void)
{
    char puerto_texto[16];
    int intentos;

    printf("  API de partida\n");

    signal(SIGCHLD, SIG_IGN);
    puerto_global = 18234;
    snprintf(puerto_texto, sizeof(puerto_texto), "%d", puerto_global);

    pid_servidor = fork();
    if (pid_servidor == 0) {
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

    for (intentos = 0; intentos < 100; intentos++) {
        int fd = conectar();
        if (fd >= 0) { close(fd); break; }
        esperar_ms(50);
    }

    prueba_iniciar();
    prueba_avanzar();
    prueba_accion_atender();
    prueba_accion_contratar();
    prueba_accion_invalida();
    prueba_metodo_incorrecto();
    prueba_sin_partida();

    kill(pid_servidor, SIGTERM);
    waitpid(pid_servidor, NULL, 0);

    return test_resumen("juego-http");
}
