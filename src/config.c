/* config.c — ver config.h */
#include "sim/config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "cjson/cJSON.h"

void config_valores_defecto(config_t *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    snprintf(cfg->nombre, SIM_NOMBRE_LEN, "base");

    cfg->semilla = 2026;
    cfg->dias = 30;
    cfg->dias_calentamiento = 10;
    cfg->replicas = 30;

    /* Demanda: 18 vehículos por día, concentrados en las 9 horas de
     * atención. Con 2 bahías y 3 mecánicos el sistema es estable pero
     * con cola visible en reparación: la reparación es el cuello de
     * botella, que es justo lo que el simulador debe mostrar. */
    cfg->llegadas_por_dia = 18.0;
    cfg->bahias_diagnostico = 2;
    cfg->mecanicos = 3;

    /* Refacciones: el consumo es 20 veh/día × 0.85 aceptación × 1.2
     * piezas/trabajo ≈ 20 piezas/día. Con un lote de 40 cada 48 h se
     * cubren 20/día, así que el almacén llega a tener tension sin
     * colapsarse. */
    cfg->refacciones_stock = 40;
    cfg->refacciones_lote = 40;
    cfg->refacciones_periodo_h = 48.0;

    cfg->presupuesto_aceptacion = 0.85;
    cfg->presupuesto_decision_h = 0.5;

    cfg->hora_apertura = 9.0;
    cfg->hora_cierre = 18.0;

    cfg->n_tipos = 3;
    cfg->peso[0] = 0.40;  cfg->media_diagnostico_h[0] = 0.5;
    cfg->media_reparacion_h[0] = 2.0;  cfg->refacciones[0] = 1;
    cfg->peso[1] = 0.40;  cfg->media_diagnostico_h[1] = 0.8;
    cfg->media_reparacion_h[1] = 3.5;  cfg->refacciones[1] = 1;
    cfg->peso[2] = 0.20;  cfg->media_diagnostico_h[2] = 1.2;
    cfg->media_reparacion_h[2] = 6.0;  cfg->refacciones[2] = 2;
}

int config_validar(const config_t *cfg)
{
    double suma = 0.0;
    int i;

    if (cfg->llegadas_por_dia <= 0.0) return 1;
    if (cfg->bahias_diagnostico < 1) return 2;
    if (cfg->mecanicos < 1) return 3;
    if (cfg->refacciones_stock < 0) return 4;
    if (cfg->presupuesto_aceptacion < 0.0 || cfg->presupuesto_aceptacion > 1.0) return 5;
    if (cfg->dias < 1) return 6;
    if (cfg->dias_calentamiento < 0) return 15;
    if (cfg->dias_calentamiento >= cfg->dias) return 16;
    if (cfg->replicas < 1) return 7;
    if (cfg->n_tipos < 1 || cfg->n_tipos > SIM_MAX_TIPOS) return 9;
    if (cfg->hora_cierre <= cfg->hora_apertura) return 10;
    if (cfg->refacciones_lote < 0) return 11;

    for (i = 0; i < cfg->n_tipos; i++) {
        suma += cfg->peso[i];
        if (cfg->peso[i] < 0.0) return 8;
        if (cfg->media_diagnostico_h[i] <= 0.0) return 12;
        if (cfg->media_reparacion_h[i] <= 0.0) return 13;
        if (cfg->refacciones[i] < 0) return 14;
    }
    /* Tolerancia por punto flotante: los pesos no siempre cuadran exacto. */
    if (fabs(suma - 1.0) > 1e-6) return 8;

    return 0;
}

const char *config_describir_error(int codigo)
{
    switch (codigo) {
        case 0:  return "sin error";
        case 1:  return "llegadas_por_dia debe ser mayor que cero";
        case 2:  return "bahias_diagnostico debe ser al menos 1";
        case 3:  return "mecanicos debe ser al menos 1";
        case 4:  return "refacciones_stock no puede ser negativo";
        case 5:  return "presupuesto_aceptacion debe estar entre 0 y 1";
        case 6:  return "dias debe ser al menos 1";
        case 7:  return "replicas debe ser al menos 1";
        case 8:  return "los pesos de los tipos de averia deben sumar 1";
        case 9:  return "n_tipos debe estar entre 1 y 4";
        case 10: return "hora_cierre debe ser mayor que hora_apertura";
        case 11: return "refacciones_lote no puede ser negativo";
        case 12: return "media_diagnostico_h debe ser mayor que cero";
        case 13: return "media_reparacion_h debe ser mayor que cero";
        case 14: return "refacciones por tipo no puede ser negativo";
        case 15: return "dias_calentamiento no puede ser negativo";
        case 16: return "dias_calentamiento debe ser menor que dias";
        default: return "error desconocido";
    }
}

/* --- Lectura de JSON -------------------------------------------------- */

static int leer_double(const cJSON *obj, const char *clave, double *destino)
{
    const cJSON *e = cJSON_GetObjectItemCaseSensitive(obj, clave);
    if (e == NULL || !cJSON_IsNumber(e)) {
        return 0;
    }
    *destino = e->valuedouble;
    return 1;
}

static int leer_int(const cJSON *obj, const char *clave, int *destino)
{
    const cJSON *e = cJSON_GetObjectItemCaseSensitive(obj, clave);
    if (e == NULL || !cJSON_IsNumber(e)) {
        return 0;
    }
    *destino = (int)e->valuedouble;
    return 1;
}

static int leer_texto(const cJSON *obj, const char *clave, char *destino, size_t tam)
{
    const cJSON *e = cJSON_GetObjectItemCaseSensitive(obj, clave);
    if (e == NULL || !cJSON_IsString(e)) {
        return 0;
    }
    snprintf(destino, tam, "%s", e->valuestring);
    return 1;
}

int config_desde_json(const cJSON *obj, config_t *cfg, char *error, size_t tam_error)
{
    int i;
    const cJSON *tipos;

    if (obj == NULL || !cJSON_IsObject(obj)) {
        snprintf(error, tam_error, "el escenario no es un objeto JSON");
        return 1;
    }

    /* Se empieza de los valores por defecto: los campos ausentes conservan
     * su valor, y así un JSON parcial sigue siendo válido. */
    config_valores_defecto(cfg);

    leer_texto(obj, "nombre", cfg->nombre, SIM_NOMBRE_LEN);

    {
        const cJSON *e = cJSON_GetObjectItemCaseSensitive(obj, "semilla");
        if (e && cJSON_IsNumber(e)) {
            cfg->semilla = (uint32_t)e->valuedouble;
        }
    }
    leer_int(obj, "dias", &cfg->dias);
    leer_int(obj, "dias_calentamiento", &cfg->dias_calentamiento);
    leer_int(obj, "replicas", &cfg->replicas);
    leer_int(obj, "bahias_diagnostico", &cfg->bahias_diagnostico);
    leer_int(obj, "mecanicos", &cfg->mecanicos);
    leer_int(obj, "refacciones_stock", &cfg->refacciones_stock);
    leer_int(obj, "refacciones_lote", &cfg->refacciones_lote);
    leer_double(obj, "llegadas_por_dia", &cfg->llegadas_por_dia);
    leer_double(obj, "refacciones_periodo_h", &cfg->refacciones_periodo_h);
    leer_double(obj, "presupuesto_aceptacion", &cfg->presupuesto_aceptacion);
    leer_double(obj, "presupuesto_decision_h", &cfg->presupuesto_decision_h);
    leer_double(obj, "hora_apertura", &cfg->hora_apertura);
    leer_double(obj, "hora_cierre", &cfg->hora_cierre);

    tipos = cJSON_GetObjectItemCaseSensitive(obj, "tipos");
    if (tipos != NULL && cJSON_IsArray(tipos)) {
        int n = cJSON_GetArraySize(tipos);
        if (n < 1 || n > SIM_MAX_TIPOS) {
            snprintf(error, tam_error, "tipos debe tener entre 1 y %d elementos", SIM_MAX_TIPOS);
            return 1;
        }
        cfg->n_tipos = n;
        for (i = 0; i < n; i++) {
            const cJSON *t = cJSON_GetArrayItem(tipos, i);
            if (!cJSON_IsObject(t)) {
                snprintf(error, tam_error, "el tipo %d no es un objeto", i);
                return 1;
            }
            if (!leer_double(t, "peso", &cfg->peso[i])) {
                snprintf(error, tam_error, "falta 'peso' en el tipo %d", i);
                return 1;
            }
            if (!leer_double(t, "media_diagnostico_h", &cfg->media_diagnostico_h[i])) {
                snprintf(error, tam_error, "falta 'media_diagnostico_h' en el tipo %d", i);
                return 1;
            }
            if (!leer_double(t, "media_reparacion_h", &cfg->media_reparacion_h[i])) {
                snprintf(error, tam_error, "falta 'media_reparacion_h' en el tipo %d", i);
                return 1;
            }
            leer_int(t, "refacciones", &cfg->refacciones[i]);
        }
    }

    {
        int err = config_validar(cfg);
        if (err != 0) {
            snprintf(error, tam_error, "%s", config_describir_error(err));
            return 1;
        }
    }
    return 0;
}

int config_desde_json_archivo(const char *ruta, config_t *cfg, char *error, size_t tam_error)
{
    FILE *f = fopen(ruta, "rb");
    char *contenido;
    long tam;
    size_t leidos;
    cJSON *raiz;
    int rc;

    if (f == NULL) {
        snprintf(error, tam_error, "no se pudo abrir el archivo '%s'", ruta);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    tam = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (tam < 0) {
        fclose(f);
        snprintf(error, tam_error, "no se pudo medir el archivo '%s'", ruta);
        return 1;
    }
    contenido = (char *)malloc((size_t)tam + 1);
    if (contenido == NULL) {
        fclose(f);
        snprintf(error, tam_error, "sin memoria para leer '%s'", ruta);
        return 1;
    }
    leidos = fread(contenido, 1, (size_t)tam, f);
    fclose(f);
    contenido[leidos] = '\0';

    raiz = cJSON_Parse(contenido);
    free(contenido);
    if (raiz == NULL) {
        snprintf(error, tam_error, "el archivo '%s' no contiene JSON valido", ruta);
        return 1;
    }
    rc = config_desde_json(raiz, cfg, error, tam_error);
    cJSON_Delete(raiz);
    return rc;
}

struct cJSON *config_a_json(const config_t *cfg)
{
    cJSON *raiz = cJSON_CreateObject();
    cJSON *tipos = cJSON_CreateArray();
    int i;

    cJSON_AddStringToObject(raiz, "nombre", cfg->nombre);
    cJSON_AddNumberToObject(raiz, "semilla", (double)cfg->semilla);
    cJSON_AddNumberToObject(raiz, "dias", cfg->dias);
    cJSON_AddNumberToObject(raiz, "dias_calentamiento", cfg->dias_calentamiento);
    cJSON_AddNumberToObject(raiz, "replicas", cfg->replicas);
    cJSON_AddNumberToObject(raiz, "llegadas_por_dia", cfg->llegadas_por_dia);
    cJSON_AddNumberToObject(raiz, "bahias_diagnostico", cfg->bahias_diagnostico);
    cJSON_AddNumberToObject(raiz, "mecanicos", cfg->mecanicos);
    cJSON_AddNumberToObject(raiz, "refacciones_stock", cfg->refacciones_stock);
    cJSON_AddNumberToObject(raiz, "refacciones_lote", cfg->refacciones_lote);
    cJSON_AddNumberToObject(raiz, "refacciones_periodo_h", cfg->refacciones_periodo_h);
    cJSON_AddNumberToObject(raiz, "presupuesto_aceptacion", cfg->presupuesto_aceptacion);
    cJSON_AddNumberToObject(raiz, "presupuesto_decision_h", cfg->presupuesto_decision_h);
    cJSON_AddNumberToObject(raiz, "hora_apertura", cfg->hora_apertura);
    cJSON_AddNumberToObject(raiz, "hora_cierre", cfg->hora_cierre);

    for (i = 0; i < cfg->n_tipos; i++) {
        cJSON *t = cJSON_CreateObject();
        cJSON_AddNumberToObject(t, "peso", cfg->peso[i]);
        cJSON_AddNumberToObject(t, "media_diagnostico_h", cfg->media_diagnostico_h[i]);
        cJSON_AddNumberToObject(t, "media_reparacion_h", cfg->media_reparacion_h[i]);
        cJSON_AddNumberToObject(t, "refacciones", cfg->refacciones[i]);
        cJSON_AddItemToArray(tipos, t);
    }
    cJSON_AddItemToObject(raiz, "tipos", tipos);

    return raiz;
}
