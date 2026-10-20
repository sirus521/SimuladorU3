/* agregado.c — Agregación de réplicas y comparación contra la teoría.
 *
 * Una réplica es una muestra de un proceso estocástico. El resultado que se
 * publica es la media entre réplicas con su intervalo de confianza al 95 %,
 * usando réplicas independientes (semillas distintas).
 */
#include "sim/resultado.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sim/analitico.h"

double ic95_media(double media, double desviacion, int n)
{
    (void)media;   /* la media no entra en el margen: sólo la dispersión */
    if (n < 2 || desviacion <= 0.0) {
        return 0.0;
    }
    /* 1.96 es el cuantil 97.5 % de la normal estándar. Con 30 réplicas el
     * error de usar z en vez de t de Student es despreciable. */
    return 1.96 * desviacion / sqrt((double)n);
}

/* Calcula media y desviación muestral de una serie de valores. */
static void media_y_desv(const double *v, int n, double *media, double *desv)
{
    double suma = 0.0, suma2 = 0.0;
    int i;
    if (n <= 0) {
        *media = 0.0;
        *desv = 0.0;
        return;
    }
    for (i = 0; i < n; i++) {
        suma += v[i];
        suma2 += v[i] * v[i];
    }
    *media = suma / (double)n;
    if (n > 1) {
        double var = (suma2 - (double)n * (*media) * (*media)) / (double)(n - 1);
        *desv = var > 0.0 ? sqrt(var) : 0.0;
    } else {
        *desv = 0.0;
    }
}

/* Agrega una métrica: media + IC. */
static void agregar_metrica(const double *valores, int n, double *media, double *ic)
{
    double desv;
    media_y_desv(valores, n, media, &desv);
    *ic = ic95_media(*media, desv, n);
}

void resultado_agregar(const resultado_replica_t *replicas, int n, const config_t *cfg,
                       resultado_t *out)
{
    double *buf;
    int i, j;
    int max_series = 0;

    memset(out, 0, sizeof(*out));
    out->n_replicas = n;
    out->semilla_base = (n > 0) ? replicas[0].semilla : 0;

    if (n <= 0) return;

    buf = (double *)malloc(sizeof(double) * (size_t)n);
    if (buf == NULL) return;

    /* --- TPR ---------------------------------------------------------- */
    for (i = 0; i < n; i++) {
        buf[i] = replicas[i].tpr_cantidad > 0
                     ? replicas[i].tpr_suma / (double)replicas[i].tpr_cantidad
                     : 0.0;
    }
    agregar_metrica(buf, n, &out->tpr_medio, &out->tpr_ic95);

    for (i = 0; i < n; i++) buf[i] = replicas[i].tpr_p90;
    agregar_metrica(buf, n, &out->tpr_p90_medio, &out->tpr_p90_ic95);

    for (i = 0; i < n; i++) buf[i] = replicas[i].tpr_min;
    agregar_metrica(buf, n, &out->tpr_min_medio, &out->tpr_min_ic95);

    for (i = 0; i < n; i++) buf[i] = replicas[i].tpr_max;
    agregar_metrica(buf, n, &out->tpr_max_medio, &out->tpr_max_ic95);

    /* --- Throughput (OT entregadas por día) ---------------------------- */
    /* El denominador son los días *medidos* (horizonte menos
     * calentamiento): si se usara el horizonte total, el calentamiento
     * contaría como días sin producción y el throughput saldría bajo. */
    {
        int dias_medidos = cfg->dias - cfg->dias_calentamiento;
        if (dias_medidos < 1) dias_medidos = cfg->dias;
        for (i = 0; i < n; i++) {
            buf[i] = (double)replicas[i].entregadas / (double)dias_medidos;
        }
        agregar_metrica(buf, n, &out->throughput_diario, &out->throughput_ic95);
    }

    /* --- Porcentajes --------------------------------------------------- */
    /* El denominador es el número de presupuestos decididos (OTs que
     * llegaron a la etapa de presupuesto), no el de entregadas: un
     * vehículo que sigue en el taller al final del horizonte también
     * cuenta, y usar entregadas+rechazadas hacía que el porcentaje
     * pasara de 100 %. */
    for (i = 0; i < n; i++) {
        int decisos = replicas[i].presupuestos_decididos;
        buf[i] = decisos > 0
                     ? 100.0 * (double)replicas[i].rechazadas / (double)decisos
                     : 0.0;
    }
    agregar_metrica(buf, n, &out->pct_rechazo, &out->pct_rechazo_ic95);

    for (i = 0; i < n; i++) {
        int decisos = replicas[i].presupuestos_decididos;
        buf[i] = decisos > 0
                     ? 100.0 * (double)replicas[i].bloqueadas_refaccion / (double)decisos
                     : 0.0;
    }
    agregar_metrica(buf, n, &out->pct_bloqueo_refac, &out->pct_bloqueo_refac_ic95);

    /* --- Ociosidad: tiempo con el taller sin ningún vehículo ------------ */
    {
        double horas = (double)(cfg->dias - cfg->dias_calentamiento) * 24.0;
        if (horas <= 0.0) horas = (double)cfg->dias * 24.0;
        for (i = 0; i < n; i++) {
            buf[i] = horas > 0.0 ? 100.0 * replicas[i].horas_vacias / horas : 0.0;
        }
        agregar_metrica(buf, n, &out->pct_ociosidad, &out->pct_ociosidad_ic95);
    }

    /* --- Utilización (sobre el horizonte medido, sin calentamiento) ------ */
    {
        double horas = (double)(cfg->dias - cfg->dias_calentamiento) * 24.0;
        if (horas <= 0.0) horas = (double)cfg->dias * 24.0;
        for (i = 0; i < n; i++) {
            buf[i] = 100.0 * replicas[i].ocupacion_diagnostico
                     / (horas * (double)cfg->bahias_diagnostico);
        }
        agregar_metrica(buf, n, &out->util_diagnostico, &out->util_diagnostico_ic95);

        for (i = 0; i < n; i++) {
            buf[i] = 100.0 * replicas[i].ocupacion_reparacion
                     / (horas * (double)cfg->mecanicos);
        }
        agregar_metrica(buf, n, &out->util_reparacion, &out->util_reparacion_ic95);
    }

    /* --- Colas y WIP ---------------------------------------------------- */
    for (i = 0; i < n; i++) buf[i] = replicas[i].cola_media_diag;
    agregar_metrica(buf, n, &out->cola_media_diag, &out->cola_media_diag_ic95);

    for (i = 0; i < n; i++) buf[i] = replicas[i].cola_media_rep;
    agregar_metrica(buf, n, &out->cola_media_rep, &out->cola_media_rep_ic95);

    for (i = 0; i < n; i++) buf[i] = replicas[i].wip_diagnostico_prom;
    agregar_metrica(buf, n, &out->wip_diagnostico, &out->wip_diagnostico_ic95);

    for (i = 0; i < n; i++) buf[i] = replicas[i].wip_reparacion_prom;
    agregar_metrica(buf, n, &out->wip_reparacion, &out->wip_reparacion_ic95);

    for (i = 0; i < n; i++) buf[i] = replicas[i].permanencia_diagnostico;
    agregar_metrica(buf, n, &out->permanencia_diagnostico, &out->permanencia_diagnostico_ic95);

    for (i = 0; i < n; i++) buf[i] = replicas[i].permanencia_reparacion;
    agregar_metrica(buf, n, &out->permanencia_reparacion, &out->permanencia_reparacion_ic95);

    /* --- TPR por tipo de avería ---------------------------------------- */
    for (j = 0; j < cfg->n_tipos; j++) {
        int m = 0;
        for (i = 0; i < n; i++) {
            if (replicas[i].tpr_tipo_cantidad[j] > 0) {
                buf[m++] = replicas[i].tpr_tipo_suma[j]
                          / (double)replicas[i].tpr_tipo_cantidad[j];
            }
        }
        out->tpr_tipo_cantidad[j] = m;
        if (m > 0) {
            agregar_metrica(buf, m, &out->tpr_tipo_medio[j], &out->tpr_tipo_ic95[j]);
        }
    }

    /* --- Histograma agregado -------------------------------------------- */
    {
        int clase;
        double ancho = 0.0;
        double maximo = 0.0;
        for (i = 0; i < n; i++) {
            if (replicas[i].histo_ancho > 0.0) ancho = replicas[i].histo_ancho;
            if (replicas[i].histo_max > maximo) maximo = replicas[i].histo_max;
        }
        out->histo_ancho = ancho;
        out->histo_max = maximo;
        for (clase = 0; clase < HISTO_CLASES; clase++) {
            int suma = 0;
            for (i = 0; i < n; i++) suma += replicas[i].histo[clase];
            out->histo[clase] = suma;
        }
    }

    /* --- Series promedio ------------------------------------------------ */
    for (i = 0; i < n; i++) {
        if (replicas[i].series_n > max_series) max_series = replicas[i].series_n;
    }
    out->series_n = max_series;
    if (max_series > 0) {
        for (i = 0; i < max_series; i++) {
            double t = 0.0, cd = 0.0, cr = 0.0, cf = 0.0, ed = 0.0, er = 0.0;
            int cuenta = 0;
            for (j = 0; j < n; j++) {
                if (i < replicas[j].series_n) {
                    t += replicas[j].series[i].t;
                    cd += replicas[j].series[i].cola_diag;
                    cr += replicas[j].series[i].cola_rep;
                    cf += replicas[j].series[i].cola_refac;
                    ed += replicas[j].series[i].en_diag;
                    er += replicas[j].series[i].en_rep;
                    cuenta++;
                }
            }
            if (cuenta > 0) {
                double k = (double)cuenta;
                out->series[i].t = t / k;
                out->series[i].cola_diag = cd / k;
                out->series[i].cola_rep = cr / k;
                out->series[i].cola_refac = cf / k;
                out->series[i].en_diag = ed / k;
                out->series[i].en_rep = er / k;
            }
        }
    }

    /* --- Deriva entre las dos mitades del periodo medido ----------------
     *
     * Si el sistema está saturado, la cola sigue creciendo y el TPR de
     * los vehículos entregados en la segunda mitad es mayor que el de la
     * primera. En ese caso el número publicado depende del horizonte y no
     * describe un régimen: hay que decirlo, no esconderlo. */
    {
        double t1 = 0.0, t2 = 0.0;
        int n1 = 0, n2 = 0;
        for (i = 0; i < n; i++) {
            if (replicas[i].entregas_primera_mitad > 0) {
                t1 += replicas[i].tpr_primera_mitad;
                n1 += replicas[i].entregas_primera_mitad;
            }
            if (replicas[i].entregas_segunda_mitad > 0) {
                t2 += replicas[i].tpr_segunda_mitad;
                n2 += replicas[i].entregas_segunda_mitad;
            }
        }
        if (n1 > 0 && n2 > 0) {
            double m1 = t1 / (double)n1;
            double m2 = t2 / (double)n2;
            out->tpr_primera_mitad = m1;
            out->tpr_segunda_mitad = m2;
            if (m1 > 0.0) {
                out->deriva_tpr_pct = 100.0 * fabs(m2 - m1) / m1;
                out->en_estacionario = out->deriva_tpr_pct <= 15.0;
            }
        } else {
            /* No hay datos suficientes: se asume estacionario. */
            out->en_estacionario = 1;
        }
    }

    /* --- Comparación contra M/M/c -------------------------------------- */
    {
        /* Tasa de llegadas media POR HORA DEL DÍA: el taller sigue
         * trabajando fuera del horario de atención (los vehículos que ya
         * entraron siguen su curso), así que el sistema opera 24 h y la
         * tasa relevante para la solución de estado estacionario es
         * llegadas_por_dia / 24.
         *
         * Nota: el modelo de llegadas NO es estacionario (se concentra en
         * 9 h), así que la comparación es una aproximación salvo en el
         * escenario de validación, que abre las 24 h. Eso se marca con
         * `comparacion_exacta`. */
        double lambda_por_hora = cfg->llegadas_por_dia / 24.0;

        /* Servicios exponenciales con media ponderada por tipo. El mixture
         * de exponenciales no es exponencial, así que la comparación es
         * exacta sólo con un solo tipo de avería. */
        double media_diag = 0.0;
        double media_rep = 0.0;
        for (i = 0; i < cfg->n_tipos; i++) {
            media_diag += cfg->peso[i] * cfg->media_diagnostico_h[i];
            media_rep += cfg->peso[i] * cfg->media_reparacion_h[i];
        }
        double mu_diag = media_diag > 0.0 ? 1.0 / media_diag : 0.0;
        /* Sólo una fracción de las OT llegan a reparación: las que aceptan
         * el presupuesto. */
        double lambda_rep = lambda_por_hora * cfg->presupuesto_aceptacion;
        double mu_rep = media_rep > 0.0 ? 1.0 / media_rep : 0.0;

        mmc_t diag = analitico_mmc(lambda_por_hora, mu_diag, cfg->bahias_diagnostico);
        mmc_t rep = analitico_mmc(lambda_rep, mu_rep, cfg->mecanicos);

        out->mmc_lambda_diag = lambda_por_hora;
        out->mmc_mu_diag = mu_diag;
        out->mmc_c_diag = cfg->bahias_diagnostico;
        out->mmc_estable = diag.estable && rep.estable;

        /* La comparación es exacta si el servicio es una exponencial pura
         * (un solo tipo) y el taller abre las 24 h (llegadas
         * estacionarias). */
        out->comparacion_exacta = (cfg->n_tipos == 1)
                                  && (cfg->hora_cierre - cfg->hora_apertura >= 24.0);

        out->mmc_lq_teorico = diag.lq;
        out->mmc_l_teorico = diag.l;
        out->mmc_w_teorico = diag.w;

        /* Simulado: la permanencia en la etapa de diagnóstico incluye la
         * espera en cola más el servicio, que es exactamente lo que mide W
         * en un M/M/c. */
        out->mmc_w_simulado = out->permanencia_diagnostico;
        out->mmc_l_simulado = out->wip_diagnostico;

        if (diag.w > 0.0) {
            out->mmc_error_w = 100.0 * (out->mmc_w_simulado - diag.w) / diag.w;
        }
        if (diag.l > 0.0) {
            out->mmc_error_l = 100.0 * (out->mmc_l_simulado - diag.l) / diag.l;
        }
        if (diag.lq > 0.0) {
            out->mmc_error_lq = 100.0 * (out->cola_media_diag - diag.lq) / diag.lq;
        }
    }

    /* --- Avisos ---------------------------------------------------------- */
    {
        size_t usado = 0;
        uint32_t max_muestras = 0;
        for (i = 0; i < n; i++) {
            if (replicas[i].muestras_prng > max_muestras) {
                max_muestras = replicas[i].muestras_prng;
            }
        }
        out->avisos[0] = '\0';
        if (max_muestras > 65536u / 4u) {
            int escrito = snprintf(out->avisos + usado, sizeof(out->avisos) - usado,
                                   "Una replica consumio %u muestras del PRNG; el periodo es 65536. "
                                   "Los resultados de esa replica podrian estar sesgados por ciclos cortos.",
                                   max_muestras);
            if (escrito > 0) usado += (size_t)escrito;
        }
    }

    free(buf);
}
