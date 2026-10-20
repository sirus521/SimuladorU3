/* resultado.h — Resultados de una simulación.
 *
 * Se separa en dos niveles:
 *   - resultado_replica_t: lo queProduce UNA corrida.
 *   - resultado_t: la agregación de N réplicas (media + IC 95 %).
 */
#ifndef SIM_RESULTADO_H
#define SIM_RESULTADO_H

#include "sim/config.h"

#define HISTO_CLASES 20
#define SERIES_MAX 2000

/* Una serie temporal muestreada. */
typedef struct {
    double t;             /* hora de simulación                       */
    double cola_diag;     /* vehículos esperando diagnóstico         */
    double cola_rep;      /* vehículos esperando reparación           */
    double cola_refac;    /* vehículos esperando refacción            */
    double en_diag;       /* vehículos en diagnóstico                */
    double en_rep;        /* vehículos en reparación                 */
} serie_t;

/* Resultados de UNA réplica. */
typedef struct {
    uint32_t semilla;

    /* Métricas de tiempo de permanencia (horas) de los vehículos entregados */
    double tpr_suma;
    double tpr_cuadrado;
    double tpr_min;
    double tpr_max;
    double tpr_p90;
    int    tpr_cantidad;

    /* Conteos */
    int entregadas;
    int rechazadas;
    int presupuestos_decididos;  /* OTs que llegaron a la etapa de presupuesto */
    int bloqueadas_refaccion;   /* Vehículos que esperaron una refacción  */
    int en_sistema_al_final;    /* vehículos que no salieron antes del fin */

    /* Tiempo de ocupación (horas) */
    double ocupacion_diagnostico; /* suma sobre bahías */
    double ocupacion_reparacion;  /* suma sobre mecánicos */
    double horas_vacias;          /* horas con el taller sin ningún vehículo */

    /* Series muestreadas cada 15 min */
    serie_t series[SERIES_MAX];
    int      series_n;

    /* Histograma de TPR (20 clases) */
    int histo[HISTO_CLASES];
    double histo_ancho;      /* ancho de clase en horas */
    double histo_max;        /* cota superior de la última clase */

    /* TPR de los vehículos entregados en la primera y en la segunda mitad
     * del periodo medido. Si difieren mucho, el sistema NO está en estado
     * estacionario: la cola sigue creciendo y el TPR depende del
     * horizonte. Es el diagnóstico que evita interpretar mal un escenario
     * saturado. */
    double tpr_primera_mitad;
    double tpr_segunda_mitad;
    int    entregas_primera_mitad;
    int    entregas_segunda_mitad;

    /* TPR por tipo de avería */
    double tpr_tipo_suma[SIM_MAX_TIPOS];
    double tpr_tipo_cuadrado[SIM_MAX_TIPOS];
    int    tpr_tipo_cantidad[SIM_MAX_TIPOS];

    /* Trabajo en el sistema (WIP) promedio observado, para Little */
    double wip_diagnostico_prom;
    double wip_reparacion_prom;

    /* Longitud media de las colas, integrada sobre el horizonte */
    double cola_media_diag;
    double cola_media_rep;

    /* Permanencia media en cada etapa (horas), para comparar con M/M/c */
    double permanencia_diagnostico;
    double permanencia_reparacion;

    /* Cuántas muestras pidió el PRNG en esta réplica. El periodo del LCG es
     * 65536: si una réplica se acerca a un cuarto de ese valor, conviene
     * avisar (ver notas/09-prng-generador.md § 9). */
    uint32_t muestras_prng;
} resultado_replica_t;

/* Agregación de N réplicas. */
typedef struct {
    int    n_replicas;
    uint32_t semilla_base;

    /* Cada métrica viene con su media y su margen de IC 95 %. */
    double tpr_medio,        tpr_ic95;
    double tpr_p90_medio,    tpr_p90_ic95;
    double tpr_min_medio,    tpr_min_ic95;
    double tpr_max_medio,    tpr_max_ic95;
    double throughput_diario, throughput_ic95;
    double pct_rechazo,      pct_rechazo_ic95;
    double pct_bloqueo_refac, pct_bloqueo_refac_ic95;
    double pct_ociosidad,    pct_ociosidad_ic95;    double util_diagnostico, util_diagnostico_ic95;
    double util_reparacion,  util_reparacion_ic95;
    double cola_media_diag,  cola_media_diag_ic95;
    double cola_media_rep,   cola_media_rep_ic95;
    double wip_diagnostico,  wip_diagnostico_ic95;
    double wip_reparacion,   wip_reparacion_ic95;

    /* Permanencia media por etapa, con su IC (para la comparación M/M/c) */
    double permanencia_diagnostico, permanencia_diagnostico_ic95;
    double permanencia_reparacion,  permanencia_reparacion_ic95;

    /* TPR por tipo de avería (media e IC) */
    double tpr_tipo_medio[SIM_MAX_TIPOS];
    double tpr_tipo_ic95[SIM_MAX_TIPOS];
    int    tpr_tipo_cantidad[SIM_MAX_TIPOS];

    /* Histograma agregado de TPR */
    int    histo[HISTO_CLASES];
    double histo_ancho;
    double histo_max;

    /* Series promedio (un punto por intervalo de muestreo) */
    serie_t series[SERIES_MAX];
    int      series_n;

    /* Comparación contra la solución analítica M/M/c */
    double   mmc_lambda_diag, mmc_mu_diag;
    int      mmc_c_diag;
    double   mmc_lq_teorico, mmc_l_teorico, mmc_w_teorico;
    double   mmc_lq_simulado, mmc_l_simulado, mmc_w_simulado;
    double   mmc_error_lq, mmc_error_l, mmc_error_w;
    int      mmc_estable;

    /* Avisos (por ejemplo, réplicas que superaron un cuarto del periodo) */
    char avisos[512];

    /* 1 si la comparación contra M/M/c es exacta: exige un solo tipo de
     * avería (servicio exponencial puro) y un sistema sin bloqueo por
     * refacciones. Con varios tipos, el servicio es una mezcla de
     * exponenciales y la comparación es sólo aproximada. */
    int comparacion_exacta;

    /* 1 si el sistema está en estado estacionario: el TPR de la segunda
     * mitad del periodo medido no difiere más de un 15 % del de la
     * primera. Si es 0, el taller está saturado y el TPR crece con el
     * horizonte, así que el número publicado depende de `dias`. */
    int en_estacionario;

    /* Variación relativa entre las dos mitades (100 · |t2 - t1| / t1). */
    double deriva_tpr_pct;

    /* TPR medio en cada mitad del periodo medido. */
    double tpr_primera_mitad;
    double tpr_segunda_mitad;
} resultado_t;

/* Calcula el IC 95 % de una media muestral. */
double ic95_media(double media, double desviacion, int n);

/* Agrega N réplicas en un resultado global. */
void resultado_agregar(const resultado_replica_t *replicas, int n, const config_t *cfg,
                       resultado_t *out);

#endif /* SIM_RESULTADO_H */
