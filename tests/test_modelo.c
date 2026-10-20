/* test_modelo.c — Motor de eventos discretos.
 *
 * Criterios de aceptación de la fase F3:
 *   - una corrida de 1 día con traza manual esperada coincide exactamente
 *   - la simulación es reproducible: misma semilla ⇒ mismos números
 *   - las invariantes del modelo se cumplen (conservación de recursos,
 *     todo vehículo que sale tiene TPR positivo, etc.)
 */
#include "test_util.h"
#include "sim/config.h"
#include "sim/modelo.h"

#include <string.h>

/* --- Un escenario mínimo y controlable ---------------------------------
 *
 * Horario de 24 h para que todas las horas sean de atención, y un solo tipo
 * de avería, para que el modelo sea M/M/c exacto y se pueda validar contra
 * la fórmula.
 */
static void escenario_minimo(config_t *cfg)
{
    config_valores_defecto(cfg);
    snprintf(cfg->nombre, SIM_NOMBRE_LEN, "minimo");
    cfg->dias = 1;
    cfg->replicas = 1;
    cfg->llegadas_por_dia = 24.0;
    cfg->bahias_diagnostico = 2;
    cfg->mecanicos = 3;
    cfg->refacciones_stock = 100;     /* sin bloqueo por refacciones */
    cfg->refacciones_lote = 0;
    cfg->refacciones_periodo_h = 48.0;
    cfg->presupuesto_aceptacion = 0.85;
    cfg->presupuesto_decision_h = 0.5;
    cfg->hora_apertura = 0.0;
    cfg->hora_cierre = 24.0;
    cfg->n_tipos = 1;
    cfg->peso[0] = 1.0;
    cfg->media_diagnostico_h[0] = 0.5;
    cfg->media_reparacion_h[0] = 2.0;
    cfg->refacciones[0] = 1;
}

/* --- Reproducibilidad -------------------------------------------------- */
static void prueba_reproducible(void)
{
    config_t cfg;
    resultado_replica_t a, b;

    escenario_minimo(&cfg);
    AFIRMAR(modelo_correr(&cfg, 2026, &a) == 0, "la primera corrida falló");
    AFIRMAR(modelo_correr(&cfg, 2026, &b) == 0, "la segunda corrida falló");

    AFIRMAR_CERCA(a.tpr_suma, b.tpr_suma, 0.0, "el TPR total no es reproducible");
    AFIRMAR(a.entregadas == b.entregadas, "el número de entregadas no es reproducible");
    AFIRMAR(a.rechazadas == b.rechazadas, "el número de rechazadas no es reproducible");
    AFIRMAR_CERCA(a.ocupacion_diagnostico, b.ocupacion_diagnostico, 0.0,
                  "la ocupación no es reproducible");
    AFIRMAR_CERCA(a.permanencia_diagnostico, b.permanencia_diagnostico, 0.0,
                  "la permanencia en diagnóstico no es reproducible");
    AFIRMAR(a.series_n == b.series_n, "el número de muestras no es reproducible");
}

/* --- Semillas distintas dan resultados distintos ------------------------ */
static void prueba_semillas_distintas(void)
{
    config_t cfg;
    resultado_replica_t a, b;

    escenario_minimo(&cfg);
    modelo_correr(&cfg, 1, &a);
    modelo_correr(&cfg, 2, &b);

    AFIRMAR(a.tpr_suma != b.tpr_suma, "dos semillas distintas dieron el mismo resultado");
}

/* --- Invariantes -------------------------------------------------------- */
static void prueba_invariantes(void)
{
    config_t cfg;
    resultado_replica_t r;
    int i;

    /* Escenario con presión: muchas llegadas, pocos recursos. */
    escenario_minimo(&cfg);
    cfg.llegadas_por_dia = 200.0;
    cfg.bahias_diagnostico = 1;
    cfg.mecanicos = 1;
    cfg.dias = 10;
    /* Inventario holgado: cada vehículo aceptado consume 1 pieza, así que
     * hacen falta más de 2000 unidades para que nunca falte. */
    cfg.refacciones_stock = 100000;

    AFIRMAR(modelo_correr(&cfg, 2026, &r) == 0, "la corrida falló");

    AFIRMAR(r.entregadas + r.rechazadas + r.en_sistema_al_final > 0,
            "no se procesó ningún vehículo");
    AFIRMAR(r.entregadas >= 0 && r.rechazadas >= 0, "conteos negativos");
    AFIRMAR(r.tpr_cantidad == r.entregadas,
            "el número de TPR no coincide con el de vehículos entregados");
    AFIRMAR(r.tpr_min >= 0.0, "el TPR mínimo es negativo");
    AFIRMAR(r.tpr_max >= r.tpr_min, "el TPR máximo es menor que el mínimo");
    AFIRMAR(r.tpr_p90 >= r.tpr_min && r.tpr_p90 <= r.tpr_max,
            "el percentil 90 está fuera de rango");
    AFIRMAR(r.bloqueadas_refaccion == 0,
            "con inventario suficiente no debería haber bloqueos por refacción");

    /* La ocupación no puede pasar del 100 % de los recursos. */
    AFIRMAR(r.ocupacion_diagnostico <= 24.0 * 10.0 * 1.0 + 1e-6,
            "la ocupación de bahías excede el tiempo disponible");
    AFIRMAR(r.ocupacion_reparacion <= 24.0 * 10.0 * 1.0 + 1e-6,
            "la ocupación de mecánicos excede el tiempo disponible");

    /* Los conteos por tipo suman el total. */
    {
        int suma = 0;
        for (i = 0; i < cfg.n_tipos; i++) suma += r.tpr_tipo_cantidad[i];
        AFIRMAR(suma == r.entregadas, "los conteos por tipo no suman el total");
    }

    /* El histograma debe contener exactamente los vehículos entregados. */
    {
        int suma = 0;
        for (i = 0; i < HISTO_CLASES; i++) suma += r.histo[i];
        AFIRMAR(suma == r.entregadas, "el histograma no suma los entregados");
        AFIRMAR(r.histo_ancho > 0.0, "el ancho del histograma es cero");
    }
}

/* --- Una sola avería: se puede validar contra M/M/c --------------------- */
static void prueba_validacion_mmc(void)
{
    config_t cfg;
    resultado_replica_t r;
    double lambda_por_hora;
    double mu;
    double rho;
    double lq_teorico;

    escenario_minimo(&cfg);
    /* Carga moderada: rho por debajo de 1 con margen, para que el sistema
     * sea estable y la comparación sea significativa. */
    cfg.llegadas_por_dia = 40.0;
    cfg.bahias_diagnostico = 2;
    cfg.mecanicos = 3;
    cfg.dias = 60;
    cfg.semilla = 2026;

    /* Varias réplicas para reducir el ruido. */
    {
        int n = 40;
        int i;
        double suma_lq = 0.0, suma_w = 0.0, suma_l = 0.0;
        for (i = 0; i < n; i++) {
            resultado_replica_t rr;
            modelo_correr(&cfg, (uint32_t)(3000 + i), &rr);
            suma_lq += rr.cola_media_diag;
            suma_w += rr.permanencia_diagnostico;
            suma_l += rr.wip_diagnostico_prom;
        }
        /* lambda en el horario de 24 h */
        lambda_por_hora = cfg.llegadas_por_dia / 24.0;
        mu = 1.0 / cfg.media_diagnostico_h[0];
        rho = lambda_por_hora / (cfg.bahias_diagnostico * mu);
        AFIRMAR(rho < 1.0, "el escenario de validación es inestable");

        /* Lq teórica de M/M/c, en la forma de Erlang-C:
         * P_espera = P0·a^c/(c!(1-rho)) y Lq = P_espera·rho/(1-rho). */
        {
            double a = lambda_por_hora / mu;
            double suma = 1.0, termino = 1.0;   /* el término n=0 es 1 */
            int k;
            for (k = 1; k < cfg.bahias_diagnostico; k++) {
                termino *= a / (double)k;
                suma += termino;
            }
            termino *= a / (double)cfg.bahias_diagnostico;
            suma += termino / (1.0 - rho);
            {
                double p0 = 1.0 / suma;
                double factorial = 1.0;
                double p_espera;
                for (k = 2; k <= cfg.bahias_diagnostico; k++) factorial *= (double)k;
                p_espera = p0 * pow(a, (double)cfg.bahias_diagnostico)
                           / (factorial * (1.0 - rho));
                lq_teorico = p_espera * rho / (1.0 - rho);
            }
        }

        AFIRMAR_CERCA(suma_lq / n, lq_teorico, lq_teorico * 0.25,
                      "la cola media simulada no cuadra con M/M/c");

        /* W = Lq/mu + 1/mu  (tiempo en el sistema) */
        {
            double w_teorico = lq_teorico / mu + 1.0 / mu;
            AFIRMAR_CERCA(suma_w / n, w_teorico, w_teorico * 0.20,
                          "la permanencia en diagnostico no cuadra con M/M/c");
        }
        /* L = Lq + a */
        {
            double a = lambda_por_hora / mu;
            double l_teorico = lq_teorico + a;
            AFIRMAR_CERCA(suma_l / n, l_teorico, l_teorico * 0.20,
                          "el WIP en diagnostico no cuadra con M/M/c");
        }
    }
    (void)r;
}

/* --- Sin refacciones no hay bloqueo; con pocas, sí ------------------------ */
static void prueba_bloqueo_por_refaccion(void)
{
    config_t cfg;
    resultado_replica_t r;

    escenario_minimo(&cfg);
    cfg.refacciones_stock = 0;      /* nunca hay piezas */
    cfg.refacciones_lote = 0;
    cfg.dias = 5;
    cfg.llegadas_por_dia = 30.0;

    AFIRMAR(modelo_correr(&cfg, 2026, &r) == 0, "la corrida falló");
    AFIRMAR(r.bloqueadas_refaccion > 0,
            "con inventario cero debería haber bloqueo por refacciones");
    /* Nadie puede terminar la reparación si no hay piezas. */
    AFIRMAR(r.entregadas == 0, "no debería entregarse ningún vehículo sin piezas");
}

/* --- El horizonte se respeta --------------------------------------------- */
static void prueba_horizonte(void)
{
    config_t cfg;
    resultado_replica_t r;
    int i;

    for (i = 1; i <= 3; i++) {
        escenario_minimo(&cfg);
        cfg.dias = i;
        cfg.llegadas_por_dia = 50.0;
        AFIRMAR(modelo_correr(&cfg, 2026, &r) == 0, "la corrida falló");
        /* Con 3 días de horizonte y 50 llegadas/día, con un solo mecánico y
         * reparaciones de 2 h, la capacidad máxima es 12 vehículos/día. */
        AFIRMAR(r.entregadas <= i * 50, "se entregaron más vehículos que llegadas");
    }
}

int main(void)
{
    printf("  Modelo\n");
    prueba_reproducible();
    prueba_semillas_distintas();
    prueba_invariantes();
    prueba_validacion_mmc();
    prueba_bloqueo_por_refaccion();
    prueba_horizonte();
    return test_resumen("modelo");
}
