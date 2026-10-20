/* rutas.c — Rutas de la API. Ver rutas.h.
 *
 * Rutas:
 *   GET  /api/salud        -> {"estado":"ok", ...}
 *   GET  /api/escenarios   -> lista los escenarios disponibles en datos/
 *   GET  /api/escenario    -> escenario por defecto (parámetros de entrada)
 *   POST /api/simular      -> corre la simulación y devuelve los resultados
 *   GET  /api/analitico    -> solución M/M/c para un escenario dado
 *
 * El cuerpo de POST /api/simular acepta los mismos campos que
 * config_a_json() produce. Los campos ausentes conservan su valor por
 * defecto, así que un cuerpo vacío devuelve el escenario base.
 */
#define _POSIX_C_SOURCE 200809L

#include "rutas.h"
#include "http.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "cjson/cJSON.h"
#include "sim/config.h"
#include "sim/modelo.h"
#include "sim/resultado.h"

/* Construye el JSON de resultados. Misma forma que usa el CLI. */
static cJSON *resultado_a_json(const resultado_t *r, const config_t *cfg)
{
    cJSON *raiz = cJSON_CreateObject();
    cJSON *resumen = cJSON_CreateObject();
    cJSON *validacion = cJSON_CreateObject();
    cJSON *series = cJSON_CreateArray();
    cJSON *histo = cJSON_CreateObject();
    cJSON *tipos = cJSON_CreateArray();
    int i;

/* Cada métrica se emite como un objeto {valor, ic95} en lugar de dos
 * campos planos ("tpr_medio" y "tpr_medio_ic95"). Así es imposible leer un
 * valor sin ver su intervalo al lado, y el contrato de la API queda
 * auto-descriptivo. */
#define METRICA(objeto, nombre, media, ic)                                   \
    do {                                                                      \
        cJSON *m = cJSON_CreateObject();                                     \
        cJSON_AddNumberToObject(m, "valor", media);                          \
        cJSON_AddNumberToObject(m, "ic95", ic);                              \
        cJSON_AddItemToObject(objeto, nombre, m);                             \
    } while (0)

    METRICA(resumen, "tpr_medio", r->tpr_medio, r->tpr_ic95);
    METRICA(resumen, "tpr_p90", r->tpr_p90_medio, r->tpr_p90_ic95);
    METRICA(resumen, "tpr_min", r->tpr_min_medio, r->tpr_min_ic95);
    METRICA(resumen, "tpr_max", r->tpr_max_medio, r->tpr_max_ic95);
    METRICA(resumen, "throughput_diario", r->throughput_diario, r->throughput_ic95);
    METRICA(resumen, "pct_rechazo", r->pct_rechazo, r->pct_rechazo_ic95);
    METRICA(resumen, "pct_bloqueo_refaccion", r->pct_bloqueo_refac, r->pct_bloqueo_refac_ic95);
    METRICA(resumen, "pct_ociosidad", r->pct_ociosidad, r->pct_ociosidad_ic95);
    METRICA(resumen, "util_diagnostico", r->util_diagnostico, r->util_diagnostico_ic95);
    METRICA(resumen, "util_reparacion", r->util_reparacion, r->util_reparacion_ic95);
    METRICA(resumen, "cola_media_diagnostico", r->cola_media_diag, r->cola_media_diag_ic95);
    METRICA(resumen, "cola_media_reparacion", r->cola_media_rep, r->cola_media_rep_ic95);
    METRICA(resumen, "wip_diagnostico", r->wip_diagnostico, r->wip_diagnostico_ic95);
    METRICA(resumen, "wip_reparacion", r->wip_reparacion, r->wip_reparacion_ic95);
    METRICA(resumen, "permanencia_diagnostico", r->permanencia_diagnostico, r->permanencia_diagnostico_ic95);
    METRICA(resumen, "permanencia_reparacion", r->permanencia_reparacion, r->permanencia_reparacion_ic95);

#undef METRICA

    cJSON_AddNumberToObject(validacion, "lambda_diagnostico", r->mmc_lambda_diag);
    cJSON_AddNumberToObject(validacion, "mu_diagnostico", r->mmc_mu_diag);
    cJSON_AddNumberToObject(validacion, "servidores", r->mmc_c_diag);
    cJSON_AddNumberToObject(validacion, "lq_teorico", r->mmc_lq_teorico);
    cJSON_AddNumberToObject(validacion, "l_teorico", r->mmc_l_teorico);
    cJSON_AddNumberToObject(validacion, "w_teorico", r->mmc_w_teorico);
    cJSON_AddNumberToObject(validacion, "lq_simulado", r->cola_media_diag);
    cJSON_AddNumberToObject(validacion, "l_simulado", r->mmc_l_simulado);
    cJSON_AddNumberToObject(validacion, "w_simulado", r->mmc_w_simulado);
    cJSON_AddNumberToObject(validacion, "error_lq_pct", r->mmc_error_lq);
    cJSON_AddNumberToObject(validacion, "error_l_pct", r->mmc_error_l);
    cJSON_AddNumberToObject(validacion, "error_w_pct", r->mmc_error_w);
    cJSON_AddNumberToObject(validacion, "estable", r->mmc_estable);
    cJSON_AddNumberToObject(validacion, "comparacion_exacta", r->comparacion_exacta);
    cJSON_AddNumberToObject(validacion, "en_estacionario", r->en_estacionario);
    cJSON_AddNumberToObject(validacion, "deriva_tpr_pct", r->deriva_tpr_pct);
    cJSON_AddNumberToObject(validacion, "tpr_primera_mitad", r->tpr_primera_mitad);
    cJSON_AddNumberToObject(validacion, "tpr_segunda_mitad", r->tpr_segunda_mitad);

    for (i = 0; i < r->series_n; i++) {
        cJSON *p = cJSON_CreateObject();
        cJSON_AddNumberToObject(p, "t", r->series[i].t);
        cJSON_AddNumberToObject(p, "cola_diagnostico", r->series[i].cola_diag);
        cJSON_AddNumberToObject(p, "cola_reparacion", r->series[i].cola_rep);
        cJSON_AddNumberToObject(p, "cola_refacciones", r->series[i].cola_refac);
        cJSON_AddNumberToObject(p, "en_diagnostico", r->series[i].en_diag);
        cJSON_AddNumberToObject(p, "en_reparacion", r->series[i].en_rep);
        cJSON_AddItemToArray(series, p);
    }

    {
        cJSON *clases = cJSON_CreateArray();
        cJSON *conteos = cJSON_CreateArray();
        for (i = 0; i < HISTO_CLASES; i++) {
            cJSON_AddItemToArray(clases, cJSON_CreateNumber((double)i * r->histo_ancho));
            cJSON_AddItemToArray(conteos, cJSON_CreateNumber((double)r->histo[i]));
        }
        cJSON_AddItemToObject(histo, "clases", clases);
        cJSON_AddItemToObject(histo, "conteos", conteos);
        cJSON_AddNumberToObject(histo, "ancho", r->histo_ancho);
        cJSON_AddNumberToObject(histo, "maximo", r->histo_max);
    }

    for (i = 0; i < cfg->n_tipos; i++) {
        cJSON *t = cJSON_CreateObject();
        cJSON_AddNumberToObject(t, "media", r->tpr_tipo_medio[i]);
        cJSON_AddNumberToObject(t, "ic95", r->tpr_tipo_ic95[i]);
        cJSON_AddNumberToObject(t, "muestras", r->tpr_tipo_cantidad[i]);
        cJSON_AddItemToArray(tipos, t);
    }

    cJSON_AddStringToObject(raiz, "escenario", cfg->nombre);
    cJSON_AddNumberToObject(raiz, "semilla_base", (double)r->semilla_base);
    cJSON_AddNumberToObject(raiz, "replicas", r->n_replicas);
    cJSON_AddItemToObject(raiz, "parametros", config_a_json(cfg));
    cJSON_AddItemToObject(raiz, "resumen", resumen);
    cJSON_AddItemToObject(raiz, "validacion", validacion);
    cJSON_AddItemToObject(raiz, "series", series);
    cJSON_AddItemToObject(raiz, "histograma", histo);
    cJSON_AddItemToObject(raiz, "por_tipo", tipos);
    if (r->avisos[0] != '\0') {
        cJSON_AddStringToObject(raiz, "avisos", r->avisos);
    }
    return raiz;
}

/* Responde con un objeto JSON ya construido. */
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

/* Corre la simulación con la configuración dada y responde. */
static int endpoint_simular(int fd, const char *cuerpo, size_t longitud)
{
    config_t cfg;
    char error[256];
    cJSON *entrada = NULL;
    resultado_replica_t *replicas;
    resultado_t global;
    int k;

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
        cJSON_Delete(entrada);
    }

    {
        int err = config_validar(&cfg);
        if (err != 0) {
            return http_responder_error(fd, 400, config_describir_error(err));
        }
    }

    if (cfg.replicas > 2000) {
        return http_responder_error(fd, 400, "demasiadas replicas (maximo 2000)");
    }
    if (cfg.dias > 3650) {
        return http_responder_error(fd, 400, "el horizonte es demasiado largo (maximo 3650 dias)");
    }

    replicas = (resultado_replica_t *)calloc((size_t)cfg.replicas, sizeof(resultado_replica_t));
    if (replicas == NULL) {
        return http_responder_error(fd, 500, "sin memoria para las replicas");
    }
    for (k = 0; k < cfg.replicas; k++) {
        if (modelo_correr(&cfg, cfg.semilla + (uint32_t)k, &replicas[k]) != 0) {
            free(replicas);
            return http_responder_error(fd, 500, "fallo al correr la simulacion");
        }
    }
    resultado_agregar(replicas, cfg.replicas, &cfg, &global);
    free(replicas);

    return responder_json(fd, resultado_a_json(&global, &cfg), 200);
}

/* Lista los archivos .json de un directorio. */
static int endpoint_escenarios(int fd, const char *ruta_datos)
{
    cJSON *raiz = cJSON_CreateObject();
    cJSON *lista = cJSON_CreateArray();
    DIR *d = opendir(ruta_datos);
    struct dirent *entrada;

    if (d != NULL) {
        while ((entrada = readdir(d)) != NULL) {
            size_t largo = strlen(entrada->d_name);
            if (largo > 5 && strcmp(entrada->d_name + largo - 5, ".json") == 0) {
                cJSON *item = cJSON_CreateObject();
                char nombre[256];
                snprintf(nombre, sizeof(nombre), "%s", entrada->d_name);
                cJSON_AddStringToObject(item, "archivo", nombre);
                cJSON_AddItemToArray(lista, item);
            }
        }
        closedir(d);
    }
    cJSON_AddItemToObject(raiz, "escenarios", lista);
    return responder_json(fd, raiz, 200);
}

int rutas_atender(int fd, const char *raiz_web, const char *metodo,
                  const char *ruta, const char *cuerpo, size_t longitud_cuerpo)
{
    char ruta_datos[1024];

    /* Se deriva el directorio de datos a partir del directorio de la
     * interfaz, para que el servidor funcione desde cualquier lugar. */
    snprintf(ruta_datos, sizeof(ruta_datos), "%s/../datos", raiz_web ? raiz_web : ".");

    if (strcmp(ruta, "/api/salud") == 0) {
        cJSON *obj = cJSON_CreateObject();
        cJSON_AddStringToObject(obj, "estado", "ok");
        cJSON_AddStringToObject(obj, "servicio", "simulador-taller-reparacion");
        cJSON_AddStringToObject(obj, "version", "1.0.0");
        return responder_json(fd, obj, 200);
    }

    if (strcmp(ruta, "/api/escenarios") == 0) {
        if (strcmp(metodo, "GET") != 0) {
            return http_responder_error(fd, 405, "solo se admite GET");
        }
        return endpoint_escenarios(fd, ruta_datos);
    }

    if (strcmp(ruta, "/api/escenario") == 0) {
        config_t cfg;
        cJSON *obj;
        if (strcmp(metodo, "GET") != 0) {
            return http_responder_error(fd, 405, "solo se admite GET");
        }
        config_valores_defecto(&cfg);
        obj = config_a_json(&cfg);
        return responder_json(fd, obj, 200);
    }

    if (strcmp(ruta, "/api/simular") == 0) {
        if (strcmp(metodo, "POST") != 0) {
            return http_responder_error(fd, 405, "solo se admite POST");
        }
        return endpoint_simular(fd, cuerpo, longitud_cuerpo);
    }

    if (strncmp(ruta, "/api/", 5) == 0) {
        return http_responder_error(fd, 404, "ruta de API desconocida");
    }

    /* Rutas de la API prioritarias sobre los archivos estáticos. */
    return 0;
}
