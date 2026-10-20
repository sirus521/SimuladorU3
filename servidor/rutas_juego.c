/* rutas_juego.c — API de partida. Ver rutas_juego.h.
 *
 * El servidor es **autoritativo**: el cliente no simula nada, sólo envía
 * acciones y dibuja el estado que le devuelve el servidor. Así la partida es
 * reproducible (misma semilla + mismas acciones = misma partida) y el
 * cliente no puede hacer trampas.
 */
#define _POSIX_C_SOURCE 200809L

#include "rutas_juego.h"
#include "http.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "cjson/cJSON.h"
#include "sim/config.h"
#include "sim/juego.h"

/* ------------------------------------------------------------------ */
/* La partida activa                                                   */
/* ------------------------------------------------------------------ */

static juego_t partida;
static int partida_activa = 0;
static struct timeval ultimo_tick;

/* Ahora mismo, en segundos (reloj monotónico). */
static double ahora(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec / 1e6;
}

/* ------------------------------------------------------------------ */
/* Utilidades de respuesta                                             */
/* ------------------------------------------------------------------ */

static int responder_json(int fd, cJSON *obj, int codigo)
{
    char *texto = cJSON_Print(obj);
    int rc;
    if (texto == NULL) {
        cJSON_Delete(obj);
        return http_responder_error(fd, 500, "no se pudo serializar la respuesta");
    }
    rc = http_responder(fd, codigo, "application/json; charset=utf-8", texto, strlen(texto));
    free(texto);
    cJSON_Delete(obj);
    return rc;
}

/* ------------------------------------------------------------------ */
/* Endpoints                                                            */
/* ------------------------------------------------------------------ */

/* POST /api/partida/iniciar
 * Cuerpo opcional: {"semilla": N, "llegadas_por_dia": N, ...} */
static int endpoint_iniciar(int fd, const char *cuerpo, size_t longitud)
{
    config_t cfg;
    char error[256];
    cJSON *entrada = NULL;
    uint32_t semilla = 2026;

    config_valores_defecto(&cfg);

    if (cuerpo != NULL && longitud > 0) {
        entrada = cJSON_ParseWithLength(cuerpo, longitud);
        if (entrada == NULL) {
            return http_responder_error(fd, 400, "el cuerpo no es JSON valido");
        }
        if (config_desde_json(entrada, &cfg, error, sizeof(error)) != 0) {
            cJSON_Delete(entrada);
            return http_responder_error(fd, 400, error);
        }
        {
            const cJSON *s = cJSON_GetObjectItemCaseSensitive(entrada, "semilla");
            if (s != NULL && cJSON_IsNumber(s)) {
                semilla = (uint32_t)s->valuedouble;
            }
        }
        cJSON_Delete(entrada);
    }

    {
        int err = config_validar(&cfg);
        if (err != 0) {
            return http_responder_error(fd, 400, config_describir_error(err));
        }
    }

    if (juego_iniciar(&partida, &cfg, semilla) != 0) {
        return http_responder_error(fd, 500, "no se pudo iniciar la partida");
    }
    partida_activa = 1;
    gettimeofday(&ultimo_tick, NULL);

    return responder_json(fd, juego_a_json(&partida), 200);
}

/* POST /api/partida/accion
 * Cuerpo: {"accion": "atender"|"cotizar"|"pedir_lote"|"contratar", ...} */
static int endpoint_accion(int fd, const char *cuerpo, size_t longitud)
{
    cJSON *entrada;
    const cJSON *accion;
    int aceptada = 0;

    if (!partida_activa) {
        return http_responder_error(fd, 409, "no hay ninguna partida en curso");
    }
    if (cuerpo == NULL || longitud == 0) {
        return http_responder_error(fd, 400, "falta el cuerpo de la peticion");
    }
    entrada = cJSON_ParseWithLength(cuerpo, longitud);
    if (entrada == NULL) {
        return http_responder_error(fd, 400, "el cuerpo no es JSON valido");
    }
    accion = cJSON_GetObjectItemCaseSensitive(entrada, "accion");
    if (accion == NULL || !cJSON_IsString(accion)) {
        cJSON_Delete(entrada);
        return http_responder_error(fd, 400, "falta el campo 'accion'");
    }

    if (strcmp(accion->valuestring, "atender") == 0) {
        const cJSON *p = cJSON_GetObjectItemCaseSensitive(entrada, "puesto");
        if (p == NULL || !cJSON_IsNumber(p)) {
            cJSON_Delete(entrada);
            return http_responder_error(fd, 400, "falta el campo 'puesto'");
        }
        aceptada = juego_accion_atender(&partida, (int)p->valuedouble);
    } else if (strcmp(accion->valuestring, "cotizar") == 0) {
        const cJSON *p = cJSON_GetObjectItemCaseSensitive(entrada, "precio");
        if (p == NULL || !cJSON_IsNumber(p)) {
            cJSON_Delete(entrada);
            return http_responder_error(fd, 400, "falta el campo 'precio'");
        }
        aceptada = juego_accion_cotizar(&partida, (int)p->valuedouble);
    } else if (strcmp(accion->valuestring, "pedir_lote") == 0) {
        aceptada = juego_accion_pedir_lote(&partida);
    } else if (strcmp(accion->valuestring, "contratar") == 0) {
        const cJSON *p = cJSON_GetObjectItemCaseSensitive(entrada, "tipo");
        if (p == NULL || !cJSON_IsNumber(p)) {
            cJSON_Delete(entrada);
            return http_responder_error(fd, 400, "falta el campo 'tipo'");
        }
        aceptada = juego_accion_contratar(&partida, (int)p->valuedouble);
    } else {
        cJSON_Delete(entrada);
        return http_responder_error(fd, 400, "accion desconocida");
    }

    cJSON_Delete(entrada);

    {
        cJSON *obj = cJSON_CreateObject();
        cJSON_AddNumberToObject(obj, "aceptada", aceptada);
        cJSON_AddItemToObject(obj, "estado", juego_a_json(&partida));
        return responder_json(fd, obj, 200);
    }
}

/* POST /api/partida/avanzar
 * Cuerpo opcional: {"segundos": N}. Si no viene, usa el tiempo real
 * transcurrido desde la última llamada (así el juego corre a ritmo real). */
static int endpoint_avanzar(int fd, const char *cuerpo, size_t longitud)
{
    cJSON *entrada = NULL;
    double segundos = -1.0;

    if (!partida_activa) {
        return http_responder_error(fd, 409, "no hay ninguna partida en curso");
    }

    if (cuerpo != NULL && longitud > 0) {
        entrada = cJSON_ParseWithLength(cuerpo, longitud);
        if (entrada == NULL) {
            return http_responder_error(fd, 400, "el cuerpo no es JSON valido");
        }
        {
            const cJSON *s = cJSON_GetObjectItemCaseSensitive(entrada, "segundos");
            if (s != NULL && cJSON_IsNumber(s)) {
                segundos = s->valuedouble;
            }
        }
        cJSON_Delete(entrada);
    }

    if (segundos < 0.0) {
        /* Ritmo real: el tiempo transcurrido desde la última llamada.
         * Aquí sí hay tope de seguridad, porque si el cliente estuvo un
         * rato sin llamar (pestaña en segundo plano) no queremos un salto
         * de varios minutos de juego de golpe. */
        double ahora_s = ahora();
        segundos = ahora_s - (double)ultimo_tick.tv_sec
                    - (double)ultimo_tick.tv_usec / 1e6;
        gettimeofday(&ultimo_tick, NULL);
        if (segundos < 0.0) segundos = 0.0;
        if (segundos > 2.0) segundos = 2.0;
    } else if (segundos < 0.0) {
        segundos = 0.0;
    }
    /* Si el cliente pide el tiempo explícitamente, se le da exacto: el
     * tope de seguridad es para el ritmo implícito, no para el explícito. */

    /* Escala: 1 segundo real = 1 minuto de juego. La velocidad la aplica
     * el cliente multiplicando los segundos que pide. */
    juego_avanzar(&partida, segundos / 60.0);

    return responder_json(fd, juego_a_json(&partida), 200);
}

/* GET /api/partida */
static int endpoint_estado(int fd)
{
    if (!partida_activa) {
        return http_responder_error(fd, 409, "no hay ninguna partida en curso");
    }
    return responder_json(fd, juego_a_json(&partida), 200);
}

/* ------------------------------------------------------------------ */
/* Dispatcher                                                          */
/* ------------------------------------------------------------------ */

int rutas_juego_atender(int fd, const char *metodo, const char *ruta,
                        const char *cuerpo, size_t longitud_cuerpo)
{
    if (strcmp(ruta, "/api/partida") == 0) {
        if (strcmp(metodo, "GET") != 0) {
            return http_responder_error(fd, 405, "solo se admite GET");
        }
        return endpoint_estado(fd);
    }
    if (strcmp(ruta, "/api/partida/iniciar") == 0) {
        if (strcmp(metodo, "POST") != 0) {
            return http_responder_error(fd, 405, "solo se admite POST");
        }
        return endpoint_iniciar(fd, cuerpo, longitud_cuerpo);
    }
    if (strcmp(ruta, "/api/partida/accion") == 0) {
        if (strcmp(metodo, "POST") != 0) {
            return http_responder_error(fd, 405, "solo se admite POST");
        }
        return endpoint_accion(fd, cuerpo, longitud_cuerpo);
    }
    if (strcmp(ruta, "/api/partida/avanzar") == 0) {
        if (strcmp(metodo, "POST") != 0) {
            return http_responder_error(fd, 405, "solo se admite POST");
        }
        return endpoint_avanzar(fd, cuerpo, longitud_cuerpo);
    }
    return 0;
}
