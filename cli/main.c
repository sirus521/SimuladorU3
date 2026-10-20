/* cli/main.c — Simulador por línea de comandos.
 *
 * Sirve para dos cosas:
 *   1. Generar los resultados del reporte de forma reproducible.
 *   2. Probar el motor sin levantar el servidor.
 *
 * Uso:
 *   simulador --escenario datos/base.json [opciones]
 *   simulador --defecto [opciones]
 *
 * Opciones:
 *   --escenario RUTA   carga un escenario JSON
 *   --defecto          usa los valores por defecto
 *   --salida RUTA      escribe el resultado en JSON (por defecto: stdout)
 *   --replicas N       sobreescribe el número de réplicas
 *   --semilla N        sobreescribe la semilla base
 *   --dias N           sobreescribe el horizonte
 *   --tabla            imprime una tabla de texto legible
 *   --help             muestra la ayuda
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cjson/cJSON.h"
#include "sim/config.h"
#include "sim/modelo.h"
#include "sim/resultado.h"

/* Serializa el resultado global a JSON. */
static cJSON *resultado_a_json(const resultado_t *r, const config_t *cfg)
{
    cJSON *raiz = cJSON_CreateObject();
    cJSON *resumen = cJSON_CreateObject();
    cJSON *validacion = cJSON_CreateObject();
    cJSON *series = cJSON_CreateArray();
    cJSON *histo = cJSON_CreateObject();
    cJSON *tipos = cJSON_CreateArray();
    cJSON *parametros = config_a_json(cfg);
    int i;

    cJSON_AddStringToObject(raiz, "escenario", cfg->nombre);
    cJSON_AddNumberToObject(raiz, "semilla_base", (double)r->semilla_base);
    cJSON_AddNumberToObject(raiz, "replicas", r->n_replicas);

    /* Métricas: cada una con su media y su margen de IC 95 %. */
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

    /* Comparación contra la solución analítica. */
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

    /* Series temporales. */
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

    /* Histograma de TPR. */
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

    /* TPR por tipo de avería. */
    for (i = 0; i < cfg->n_tipos; i++) {
        cJSON *t = cJSON_CreateObject();
        cJSON_AddNumberToObject(t, "media", r->tpr_tipo_medio[i]);
        cJSON_AddNumberToObject(t, "ic95", r->tpr_tipo_ic95[i]);
        cJSON_AddNumberToObject(t, "muestras", r->tpr_tipo_cantidad[i]);
        cJSON_AddItemToArray(tipos, t);
    }

    cJSON_AddItemToObject(raiz, "parametros", parametros);
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

static void imprimir_tabla(const resultado_t *r, const config_t *cfg)
{
    printf("\n");
    printf("  Escenario: %s   (semilla %u, %d réplicas, %d días)\n",
           cfg->nombre, r->semilla_base, r->n_replicas, cfg->dias);
    printf("  -------------------------------------------------------------------\n");
    printf("  %-30s %12s %12s\n", "Métrica", "Valor", "IC 95 %");
    printf("  -------------------------------------------------------------------\n");
    printf("  %-30s %12.3f %12.3f\n", "TPR medio (h)", r->tpr_medio, r->tpr_ic95);
    printf("  %-30s %12.3f %12.3f\n", "TPR percentil 90 (h)", r->tpr_p90_medio, r->tpr_p90_ic95);
    printf("  %-30s %12.3f %12.3f\n", "OT entregadas por día", r->throughput_diario, r->throughput_ic95);
    printf("  %-30s %12.2f %12.2f\n", "Presupuestos rechazados (%)", r->pct_rechazo, r->pct_rechazo_ic95);
    printf("  %-30s %12.2f %12.2f\n", "Bloqueo por refacción (%)", r->pct_bloqueo_refac, r->pct_bloqueo_refac_ic95);
    printf("  %-30s %12.2f %12.2f\n", "Ociosidad del taller (%)", r->pct_ociosidad, r->pct_ociosidad_ic95);
    printf("  %-30s %12.2f %12.2f\n", "Utilización diagnóstico (%)", r->util_diagnostico, r->util_diagnostico_ic95);
    printf("  %-30s %12.2f %12.2f\n", "Utilización reparación (%)", r->util_reparacion, r->util_reparacion_ic95);
    printf("  %-30s %12.3f %12.3f\n", "Cola media diagnóstico", r->cola_media_diag, r->cola_media_diag_ic95);
    printf("  %-30s %12.3f %12.3f\n", "Cola media reparación", r->cola_media_rep, r->cola_media_rep_ic95);
    printf("  -------------------------------------------------------------------\n");
    printf("  Validación contra M/M/c (etapa de diagnóstico)\n");
    printf("  %-30s %12.4f\n", "λ (llegadas/h)", r->mmc_lambda_diag);
    printf("  %-30s %12.4f\n", "μ (servicio/h)", r->mmc_mu_diag);
    printf("  %-30s %12.4f\n", "ρ (utilización)", r->mmc_lambda_diag / (r->mmc_c_diag * r->mmc_mu_diag));
    printf("  %-30s %12.4f %12.4f\n", "Lq (cola) simulado / teórico", r->cola_media_diag, r->mmc_lq_teorico);
    printf("  %-30s %12.4f %12.4f\n", "L (sistema) simulado / teórico", r->mmc_l_simulado, r->mmc_l_teorico);
    printf("  %-30s %12.4f %12.4f\n", "W (h) simulado / teórico", r->mmc_w_simulado, r->mmc_w_teorico);
    printf("  %-30s %12.2f%%\n", "Error relativo en W", r->mmc_error_w);
    printf("  %-30s %12s\n", "Comparación exacta", r->comparacion_exacta ? "si" : "no (aproximada)");
    printf("  %-30s %12s\n", "En estado estacionario", r->en_estacionario ? "si" : "NO (saturado)");
    printf("  %-30s %12.2f%%\n", "Deriva del TPR entre mitades", r->deriva_tpr_pct);
    printf("  -------------------------------------------------------------------\n");
    if (r->avisos[0] != '\0') {
        printf("  AVISO: %s\n", r->avisos);
        printf("  -------------------------------------------------------------------\n");
    }
}

static void imprimir_ayuda(void)
{
    printf("Simulador de Taller de Reparación (SOTR)\n\n");
    printf("Uso:\n");
    printf("  simulador --escenario RUTA [opciones]\n");
    printf("  simulador --defecto [opciones]\n\n");
    printf("Opciones:\n");
    printf("  --escenario RUTA   carga un escenario JSON\n");
    printf("  --defecto          usa los valores por defecto\n");
    printf("  --salida RUTA      escribe el resultado en JSON (por defecto: stdout)\n");
    printf("  --replicas N       sobreescribe el número de réplicas\n");
    printf("  --semilla N        sobreescribe la semilla base\n");
    printf("  --dias N           sobreescribe el horizonte\n");
    printf("  --tabla            imprime una tabla de texto legible\n");
    printf("  --help             muestra esta ayuda\n");
}

int main(int argc, char **argv)
{
    config_t cfg;
    char error[256];
    const char *ruta_escenario = NULL;
    const char *ruta_salida = NULL;
    int usar_defecto = 0;
    int imprimir = 0;
    int i;
    int replicas_override = 0;
    int semilla_override = 0;
    int dias_override = 0;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            imprimir_ayuda();
            return 0;
        } else if (strcmp(argv[i], "--escenario") == 0 && i + 1 < argc) {
            ruta_escenario = argv[++i];
        } else if (strcmp(argv[i], "--salida") == 0 && i + 1 < argc) {
            ruta_salida = argv[++i];
        } else if (strcmp(argv[i], "--replicas") == 0 && i + 1 < argc) {
            replicas_override = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--semilla") == 0 && i + 1 < argc) {
            semilla_override = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--dias") == 0 && i + 1 < argc) {
            dias_override = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--tabla") == 0) {
            imprimir = 1;
        } else if (strcmp(argv[i], "--defecto") == 0) {
            usar_defecto = 1;
        } else {
            fprintf(stderr, "Opción desconocida: %s\n", argv[i]);
            fprintf(stderr, "Use --help para ver las opciones.\n");
            return 2;
        }
    }

    if (ruta_escenario == NULL && !usar_defecto) {
        fprintf(stderr, "Falta --escenario o --defecto.\n");
        imprimir_ayuda();
        return 2;
    }

    if (ruta_escenario != NULL) {
        if (config_desde_json_archivo(ruta_escenario, &cfg, error, sizeof(error)) != 0) {
            fprintf(stderr, "No se pudo cargar el escenario: %s\n", error);
            return 1;
        }
    } else {
        config_valores_defecto(&cfg);
    }

    if (replicas_override > 0) cfg.replicas = replicas_override;
    if (semilla_override > 0) cfg.semilla = (uint32_t)semilla_override;
    if (dias_override > 0) cfg.dias = dias_override;

    {
        int err = config_validar(&cfg);
        if (err != 0) {
            fprintf(stderr, "Escenario inválido: %s\n", config_describir_error(err));
            return 1;
        }
    }

    /* Corre las réplicas. */
    {
        resultado_replica_t *replicas;
        resultado_t global;
        int k;

        if (cfg.replicas > 10000) {
            fprintf(stderr, "Demasiadas réplicas (máximo 10000).\n");
            return 1;
        }
        replicas = (resultado_replica_t *)calloc((size_t)cfg.replicas, sizeof(resultado_replica_t));
        if (replicas == NULL) {
            fprintf(stderr, "Sin memoria para las réplicas.\n");
            return 1;
        }
        for (k = 0; k < cfg.replicas; k++) {
            uint32_t semilla = cfg.semilla + (uint32_t)k;
            if (modelo_correr(&cfg, semilla, &replicas[k]) != 0) {
                fprintf(stderr, "Falló la réplica %d (semilla %u).\n", k, semilla);
                free(replicas);
                return 1;
            }
        }
        resultado_agregar(replicas, cfg.replicas, &cfg, &global);
        free(replicas);

        if (imprimir) {
            imprimir_tabla(&global, &cfg);
        }

        {
            cJSON *salida = resultado_a_json(&global, &cfg);
            char *texto = cJSON_Print(salida);
            if (texto == NULL) {
                fprintf(stderr, "No se pudo serializar el resultado.\n");
                cJSON_Delete(salida);
                return 1;
            }
            if (ruta_salida != NULL) {
                FILE *f = fopen(ruta_salida, "wb");
                if (f == NULL) {
                    fprintf(stderr, "No se pudo abrir '%s' para escribir.\n", ruta_salida);
                    free(texto);
                    cJSON_Delete(salida);
                    return 1;
                }
                fwrite(texto, 1, strlen(texto), f);
                fclose(f);
                fprintf(stderr, "Resultado escrito en %s\n", ruta_salida);
            } else {
                printf("%s\n", texto);
            }
            free(texto);
            cJSON_Delete(salida);
        }
    }

    return 0;
}
