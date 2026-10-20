/* modelo.c — Motor de simulación de eventos discretos del taller.
 *
 * Implementa el proceso de notas/02-modelo-del-negocio.md:
 *   recepción → diagnóstico → presupuesto → (refacciones) → reparación → entrega
 *
 * Decisiones de diseño relevantes:
 *   - Cola de prioridad por tiempo, con desempate por orden de inserción.
 *     Eso hace la simulación determinista: misma semilla ⇒ misma secuencia.
 *   - El horizonte se cierra con un evento explícito, para que el estado
 *     final sea reproducible.
 *   - Las llegadas son un proceso Poisson **restringido al horario de
 *     atención** (notas/03 §3): dentro del horario los saltos son
 *     exponenciales; al cruzar el cierre, la llegada se pospone a la
 *     apertura siguiente y se vuelve a muestrear (propiedad de memoria nula).
 *   - Los contadores de población son O(1): no se recorre la lista de
 *     vehículos en cada evento.
 */
#include "sim/modelo.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sim/analitico.h"
#include "sim/distrib.h"
#include "sim/prng.h"

/* ------------------------------------------------------------------ */
/* Tipos de evento                                                     */
/* ------------------------------------------------------------------ */

typedef enum {
    EV_LLEGADA = 0,
    EV_FIN_DIAGNOSTICO,
    EV_DECISION_PRESUPUESTO,
    EV_FIN_REPARACION,
    EV_REPOSICION,
    EV_FIN_HORIZONTE
} tipo_evento_t;

typedef struct {
    double        t;        /* hora de ocurrencia              */
    uint32_t      orden;    /* desempate por orden de inserción */
    tipo_evento_t tipo;
    int           vehiculo; /* índice del vehículo, o -1       */
} evento_t;

/* ------------------------------------------------------------------ */
/* Vehículo (orden de trabajo)                                         */
/* ------------------------------------------------------------------ */

typedef enum {
    V_ESPERA_DIAGNOSTICO = 0,
    V_EN_DIAGNOSTICO,
    V_ESPERA_DECISION,
    V_ESPERA_REFACCION,
    V_ESPERA_REPARACION,
    V_EN_REPARACION,
    V_ENTREGADO,
    V_RECHAZADO
} estado_vehiculo_t;

typedef struct {
    estado_vehiculo_t estado;
    int    tipo;
    double t_llegada;
    double t_fin_diagnostico;
    double t_entrada_reparacion;  /* cuando sale de espera de refacción */
    double t_fin_reparacion;
    double t_salida;              /* entrega o rechazo */
    int    esperó_refaccion;
} vehiculo_t;

/* ------------------------------------------------------------------ */
/* Cola de prioridad (min-heap sobre el tiempo)                        */
/* ------------------------------------------------------------------ */

typedef struct {
    evento_t *v;
    int       n;
    int       cap;
    uint32_t  orden;
} cola_t;

static int cola_ini(cola_t *c, int cap)
{
    c->v = (evento_t *)malloc(sizeof(evento_t) * (size_t)cap);
    c->n = 0;
    c->cap = cap;
    c->orden = 0;
    return c->v != NULL;
}

static void cola_lib(cola_t *c)
{
    free(c->v);
    c->v = NULL;
    c->n = c->cap = 0;
}

static int evento_menor(const evento_t *a, const evento_t *b)
{
    if (a->t != b->t) return a->t < b->t;
    return a->orden < b->orden;
}

static int cola_push(cola_t *c, double t, tipo_evento_t tipo, int vehiculo)
{
    int i;
    if (c->n == c->cap) {
        int nuevo = c->cap * 2;
        evento_t *tmp = (evento_t *)realloc(c->v, sizeof(evento_t) * (size_t)nuevo);
        if (tmp == NULL) return 0;
        c->v = tmp;
        c->cap = nuevo;
    }
    c->v[c->n].t = t;
    c->v[c->n].orden = c->orden++;
    c->v[c->n].tipo = tipo;
    c->v[c->n].vehiculo = vehiculo;
    i = c->n++;
    while (i > 0) {
        int padre = (i - 1) / 2;
        evento_t tmp2;
        if (!evento_menor(&c->v[i], &c->v[padre])) break;
        tmp2 = c->v[i]; c->v[i] = c->v[padre]; c->v[padre] = tmp2;
        i = padre;
    }
    return 1;
}

static int cola_pop(cola_t *c, evento_t *salida)
{
    int i = 0;
    if (c->n == 0) return 0;
    *salida = c->v[0];
    c->n--;
    if (c->n > 0) c->v[0] = c->v[c->n];
    for (;;) {
        int izq = 2 * i + 1;
        int der = 2 * i + 2;
        int menor = i;
        if (izq < c->n && evento_menor(&c->v[izq], &c->v[menor])) menor = izq;
        if (der < c->n && evento_menor(&c->v[der], &c->v[menor])) menor = der;
        if (menor == i) break;
        { evento_t tmp = c->v[i]; c->v[i] = c->v[menor]; c->v[menor] = tmp; }
        i = menor;
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/* Cola FIFO de índices (con cabeza que avanza)                        */
/* ------------------------------------------------------------------ */

typedef struct {
    int *v;
    int  n;       /* elementos totales insertados */
    int  cabeza;  /* índice del primero pendiente */
    int  cap;
} fifo_t;

static int fifo_push(fifo_t *f, int valor)
{
    if (f->n >= f->cap) {
        int nuevo = f->cap ? f->cap * 2 : 64;
        int *tmp = (int *)realloc(f->v, sizeof(int) * (size_t)nuevo);
        if (tmp == NULL) return 0;
        f->v = tmp;
        f->cap = nuevo;
    }
    f->v[f->n++] = valor;
    return 1;
}

static int fifo_pop(fifo_t *f)
{
    if (f->cabeza >= f->n) return -1;
    return f->v[f->cabeza++];
}

static int fifo_vacia(const fifo_t *f)
{
    return f->cabeza >= f->n;
}

static void fifo_lib(fifo_t *f)
{
    free(f->v);
    f->v = NULL;
    f->n = f->cabeza = f->cap = 0;
}

/* ------------------------------------------------------------------ */
/* Estado de la simulación                                             */
/* ------------------------------------------------------------------ */

typedef struct {
    const config_t *cfg;
    prng_t   rng;
    cola_t   eventos;

    vehiculo_t *vehiculos;
    int      n_vehiculos;
    int      cap_vehiculos;

    /* Recursos */
    int      bahias_libres;
    int      mecanicos_libres;
    int      stock_refacciones;

    /* Contadores de población (O(1)) */
    int n_espera_diag, n_en_diag;
    int n_espera_rep,  n_en_rep;
    int n_espera_refac;

    /* Colas */
    fifo_t cola_diag;
    fifo_t cola_rep;
    fifo_t cola_refac;

    /* Acumuladores de área (vehículo·hora) */
    double area_wip_diag;   /* en espera + en servicio, etapa diagnóstico */
    double area_wip_rep;    /* en espera + en servicio, etapa reparación  */
    double area_ocup_bahia; /* bahía·hora ocupada                        */
    double area_ocup_mec;   /* mecánico·hora ocupado                     */
    double area_cola_diag;  /* longitud de cola integrate                */
    double area_cola_rep;
    double area_vacia;      /* horas con el taller completamente vacío */

    double   t_actual;
    double   t_ultima_muestra;
    double   intervalo_muestra;
    double   t_calentamiento;   /* hasta aquí no se acumulan estadísticas */
    int      n_series;
    serie_t  series[SERIES_MAX];

    double   histo_max;
    double   histo_ancho;
    int      histo[HISTO_CLASES];

    double  *tpr_valores;
    int      n_tpr, cap_tpr;

    /* Sums para las permanencia por etapa */
    double   diag_dur_suma; int diag_dur_n;
    double   rep_dur_suma;  int rep_dur_n;
} estado_t;

/* ------------------------------------------------------------------ */
/* Utilidades                                                          */
/* ------------------------------------------------------------------ */

/* Próximo instante de llegada, con el proceso restringido al horario. */
static double proxima_llegada(estado_t *s, double t_actual)
{
    const config_t *cfg = s->cfg;
    double horas_atencion = cfg->hora_cierre - cfg->hora_apertura;
    double media_salto = horas_atencion / cfg->llegadas_por_dia;
    double t = t_actual;

    for (;;) {
        double dia = floor(t / 24.0);
        double apertura = dia * 24.0 + cfg->hora_apertura;
        double cierre = dia * 24.0 + cfg->hora_cierre;

        if (t < apertura) {
            t = apertura;
            continue;
        }
        if (t >= cierre) {
            t = (dia + 1.0) * 24.0 + cfg->hora_apertura;
            continue;
        }
        {
            double salto = dist_exponencial(&s->rng, media_salto);
            if (t + salto < cierre) {
                return t + salto;
            }
            /* El salto cruzaría el cierre. Por memoria nula de la
             * exponencial, el tiempo restante se vuelve a sortear en la
             * apertura siguiente. */
            t = (dia + 1.0) * 24.0 + cfg->hora_apertura;
        }
    }
}

/* Acumula las áreas de ocupación y de población en el intervalo [t_ant, t].
 *
 * Nada se acumula antes del fin del periodo de calentamiento: el taller
 * arranca vacío y la cola se llena durante los primeros días. Medir desde
 * el primer minuto mezcla el transitorio con el régimen permanente y
 * subestima el TPR (ver notas/08-reporte.md). */
static void acumular_areas(estado_t *s, double dt)
{
    int en_sistema;
    if (dt <= 0.0) return;

    en_sistema = s->n_espera_diag + s->n_en_diag
               + s->n_espera_rep + s->n_en_rep
               + s->n_espera_refac;

    if (s->t_actual < s->t_calentamiento) {
        return;
    }

    s->area_ocup_bahia += (double)(s->cfg->bahias_diagnostico - s->bahias_libres) * dt;
    s->area_ocup_mec   += (double)(s->cfg->mecanicos - s->mecanicos_libres) * dt;
    s->area_wip_diag   += (double)(s->n_espera_diag + s->n_en_diag) * dt;
    s->area_wip_rep    += (double)(s->n_espera_rep + s->n_en_rep) * dt;
    s->area_cola_diag  += (double)s->n_espera_diag * dt;
    s->area_cola_rep   += (double)s->n_espera_rep * dt;

    /* Ociosidad: tiempo con ningún vehículo dentro del taller. */
    if (en_sistema == 0) {
        s->area_vacia += dt;
    }
}

static void muestrear(estado_t *s)
{
    serie_t *se;
    if (s->n_series >= SERIES_MAX) return;
    if (s->t_actual < s->t_calentamiento) return;
    se = &s->series[s->n_series++];
    se->t = s->t_actual;
    se->cola_diag = (double)s->n_espera_diag;
    se->cola_rep = (double)s->n_espera_rep;
    se->cola_refac = (double)s->n_espera_refac;
    se->en_diag = (double)s->n_en_diag;
    se->en_rep = (double)s->n_en_rep;
}

/* Intenta ocupar una bahía de diagnóstico si hay alguna libre. */
static void intentar_diagnostico(estado_t *s)
{
    while (s->bahias_libres > 0) {
        int v = fifo_pop(&s->cola_diag);
        double dur;
        if (v < 0) break;
        s->bahias_libres--;
        s->n_espera_diag--;
        s->n_en_diag++;
        s->vehiculos[v].estado = V_EN_DIAGNOSTICO;
        dur = dist_exponencial(&s->rng, s->cfg->media_diagnostico_h[s->vehiculos[v].tipo]);
        cola_push(&s->eventos, s->t_actual + dur, EV_FIN_DIAGNOSTICO, v);
    }
}

/* Intenta ocupar un mecánico si hay alguno libre. */
static void intentar_reparacion(estado_t *s)
{
    while (s->mecanicos_libres > 0) {
        int v = fifo_pop(&s->cola_rep);
        double dur;
        if (v < 0) break;
        s->mecanicos_libres--;
        s->n_espera_rep--;
        s->n_en_rep++;
        s->vehiculos[v].estado = V_EN_REPARACION;
        dur = dist_exponencial(&s->rng, s->cfg->media_reparacion_h[s->vehiculos[v].tipo]);
        cola_push(&s->eventos, s->t_actual + dur, EV_FIN_REPARACION, v);
    }
}

/* Asigna refacciones a los vehículos en espera, respetando el FIFO. */
static void intentar_refacciones(estado_t *s)
{
    while (s->stock_refacciones > 0 && !fifo_vacia(&s->cola_refac)) {
        int v = s->cola_refac.v[s->cola_refac.cabeza];
        int necesarias = s->cfg->refacciones[s->vehiculos[v].tipo];
        if (s->stock_refacciones < necesarias) {
            /* No alcanza para el primero de la cola: el FIFO se respeta y
             * los que están detrás también esperan. */
            break;
        }
        s->stock_refacciones -= necesarias;
        s->cola_refac.cabeza++;
        s->n_espera_refac--;
        s->vehiculos[v].estado = V_ESPERA_REPARACION;
        s->vehiculos[v].t_entrada_reparacion = s->t_actual;
        fifo_push(&s->cola_rep, v);
        s->n_espera_rep++;
        intentar_reparacion(s);
    }
}

/* ------------------------------------------------------------------ */
/* Bucle principal                                                     */
/* ------------------------------------------------------------------ */

int modelo_correr(const config_t *cfg, uint32_t semilla, resultado_replica_t *out)
{
    estado_t s;
    evento_t ev;
    double horizonte;
    int terminado = 0;
    int ok = 0;

    memset(&s, 0, sizeof(s));
    memset(out, 0, sizeof(*out));
    out->semilla = semilla;

    s.cfg = cfg;
    prng_iniciar(&s.rng, (uint16_t)(semilla & 0xFFFFu));
    if (!cola_ini(&s.eventos, 1024)) return 1;

    s.cap_vehiculos = 64;
    s.vehiculos = (vehiculo_t *)calloc((size_t)s.cap_vehiculos, sizeof(vehiculo_t));
    if (s.vehiculos == NULL) { cola_lib(&s.eventos); return 1; }

    s.bahias_libres = cfg->bahias_diagnostico;
    s.mecanicos_libres = cfg->mecanicos;
    s.stock_refacciones = cfg->refacciones_stock;

    horizonte = (double)cfg->dias * 24.0;
    s.t_calentamiento = (double)cfg->dias_calentamiento * 24.0;
    if (s.t_calentamiento >= horizonte) {
        /* Un calentamiento mayor que el horizonte no deja nada que medir. */
        s.t_calentamiento = 0.0;
    }
    /* Muestreo cada 15 min como máximo, con un tope de ~2900 puntos para
     * que el JSON que consume la interfaz no crezca sin control. */
    s.intervalo_muestra = 0.25;
    if ((horizonte - s.t_calentamiento) / s.intervalo_muestra > 2900.0) {
        s.intervalo_muestra = (horizonte - s.t_calentamiento) / 2900.0;
    }

    /* Primera llegada: en la apertura del primer día. */
    cola_push(&s.eventos, cfg->hora_apertura, EV_LLEGADA, -1);
    /* Primera reposición de refacciones. */
    if (cfg->refacciones_periodo_h > 0.0) {
        cola_push(&s.eventos, cfg->refacciones_periodo_h, EV_REPOSICION, -1);
    }
    cola_push(&s.eventos, horizonte, EV_FIN_HORIZONTE, -1);

    while (!terminado && cola_pop(&s.eventos, &ev)) {
        double dt = ev.t - s.t_actual;
        acumular_areas(&s, dt);

        while (s.t_ultima_muestra + s.intervalo_muestra <= ev.t) {
            s.t_ultima_muestra += s.intervalo_muestra;
            s.t_actual = s.t_ultima_muestra;
            muestrear(&s);
        }
        s.t_actual = ev.t;

        switch (ev.tipo) {
        case EV_LLEGADA: {
            int v;
            double acum[SIM_MAX_TIPOS];
            double suma = 0.0;
            int i;
            double t_sig;

            if (s.n_vehiculos >= s.cap_vehiculos) {
                int nuevo = s.cap_vehiculos * 2;
                vehiculo_t *tmp = (vehiculo_t *)realloc(s.vehiculos,
                                                        sizeof(vehiculo_t) * (size_t)nuevo);
                if (tmp == NULL) { ok = 1; break; }
                s.vehiculos = tmp;
                s.cap_vehiculos = nuevo;
            }
            v = s.n_vehiculos++;
            memset(&s.vehiculos[v], 0, sizeof(vehiculo_t));
            s.vehiculos[v].estado = V_ESPERA_DIAGNOSTICO;
            s.vehiculos[v].t_llegada = ev.t;

            for (i = 0; i < cfg->n_tipos; i++) {
                suma += cfg->peso[i];
                acum[i] = suma;
            }
            acum[cfg->n_tipos - 1] = 1.0;  /* corrige el redondeo acumulado */
            s.vehiculos[v].tipo = dist_categorica(&s.rng, acum, cfg->n_tipos);

            fifo_push(&s.cola_diag, v);
            s.n_espera_diag++;
            intentar_diagnostico(&s);

            t_sig = proxima_llegada(&s, ev.t);
            if (t_sig < horizonte) {
                cola_push(&s.eventos, t_sig, EV_LLEGADA, -1);
            }
            break;
        }

        case EV_FIN_DIAGNOSTICO: {
            int v = ev.vehiculo;
            double espera;
            s.bahias_libres++;
            s.n_en_diag--;
            s.vehiculos[v].estado = V_ESPERA_DECISION;
            s.vehiculos[v].t_fin_diagnostico = ev.t;
            s.diag_dur_suma += ev.t - s.vehiculos[v].t_llegada;
            s.diag_dur_n++;
            /* La bahía queda libre: puede atender al siguiente. */
            intentar_diagnostico(&s);
            espera = dist_exponencial(&s.rng, cfg->presupuesto_decision_h);
            cola_push(&s.eventos, ev.t + espera, EV_DECISION_PRESUPUESTO, v);
            break;
        }

        case EV_DECISION_PRESUPUESTO: {
            int v = ev.vehiculo;
            if (ev.t >= s.t_calentamiento) {
                out->presupuestos_decididos++;
            }
            if (!dist_bernoulli(&s.rng, cfg->presupuesto_aceptacion)) {
                s.vehiculos[v].estado = V_RECHAZADO;
                s.vehiculos[v].t_salida = ev.t;
                if (ev.t >= s.t_calentamiento) {
                    out->rechazadas++;
                }
                break;
            }
            if (s.stock_refacciones >= cfg->refacciones[s.vehiculos[v].tipo]) {
                s.stock_refacciones -= cfg->refacciones[s.vehiculos[v].tipo];
                s.vehiculos[v].estado = V_ESPERA_REPARACION;
                s.vehiculos[v].t_entrada_reparacion = ev.t;
                fifo_push(&s.cola_rep, v);
                s.n_espera_rep++;
                intentar_reparacion(&s);
            } else {
                s.vehiculos[v].estado = V_ESPERA_REFACCION;
                s.vehiculos[v].esperó_refaccion = 1;
                if (ev.t >= s.t_calentamiento) {
                    out->bloqueadas_refaccion++;
                }
                fifo_push(&s.cola_refac, v);
                s.n_espera_refac++;
            }
            break;
        }

        case EV_FIN_REPARACION: {
            int v = ev.vehiculo;
            double tpr;
            int tipo;
            s.mecanicos_libres++;
            s.n_en_rep--;
            s.vehiculos[v].estado = V_ENTREGADO;
            s.vehiculos[v].t_fin_reparacion = ev.t;
            s.vehiculos[v].t_salida = ev.t;
            s.rep_dur_suma += ev.t - s.vehiculos[v].t_entrada_reparacion;
            s.rep_dur_n++;

            /* Los vehículos entregados durante el calentamiento no cuentan
             * para las estadísticas: su TPR refleja un taller vacío. */
            if (ev.t < s.t_calentamiento) {
                intentar_reparacion(&s);
                break;
            }

            tpr = ev.t - s.vehiculos[v].t_llegada;
            tipo = s.vehiculos[v].tipo;
            out->entregadas++;
            out->tpr_suma += tpr;
            out->tpr_cuadrado += tpr * tpr;
            if (out->tpr_cantidad == 0 || tpr < out->tpr_min) out->tpr_min = tpr;
            if (tpr > out->tpr_max) out->tpr_max = tpr;
            out->tpr_cantidad++;
            out->tpr_tipo_suma[tipo] += tpr;
            out->tpr_tipo_cuadrado[tipo] += tpr * tpr;
            out->tpr_tipo_cantidad[tipo]++;

            /* Mitad del periodo medido: sirve para detectar deriva. */
            {
                double mitad = (s.t_calentamiento + horizonte) / 2.0;
                if (ev.t < mitad) {
                    out->tpr_primera_mitad += tpr;
                    out->entregas_primera_mitad++;
                } else {
                    out->tpr_segunda_mitad += tpr;
                    out->entregas_segunda_mitad++;
                }
            }

            if (s.n_tpr >= s.cap_tpr) {
                int nuevo = s.cap_tpr ? s.cap_tpr * 2 : 256;
                double *tmp = (double *)realloc(s.tpr_valores, sizeof(double) * (size_t)nuevo);
                if (tmp == NULL) { ok = 1; break; }
                s.tpr_valores = tmp;
                s.cap_tpr = nuevo;
            }
            s.tpr_valores[s.n_tpr++] = tpr;
            if (tpr > s.histo_max) s.histo_max = tpr;

            intentar_reparacion(&s);
            break;
        }

        case EV_REPOSICION: {
            /* El stock no pasa de su capacidad: `refacciones_stock` es a la
             * vez el inventario inicial y el tope del almacén. */
            s.stock_refacciones += cfg->refacciones_lote;
            if (s.stock_refacciones > cfg->refacciones_stock) {
                s.stock_refacciones = cfg->refacciones_stock;
            }
            intentar_refacciones(&s);
            {
                double t_sig = ev.t + cfg->refacciones_periodo_h;
                if (t_sig < horizonte) {
                    cola_push(&s.eventos, t_sig, EV_REPOSICION, -1);
                }
            }
            break;
        }

        case EV_FIN_HORIZONTE: {
            int i;
            for (i = 0; i < s.n_vehiculos; i++) {
                if (s.vehiculos[i].estado != V_ENTREGADO &&
                    s.vehiculos[i].estado != V_RECHAZADO) {
                    out->en_sistema_al_final++;
                }
            }
            terminado = 1;
            break;
        }
        }
        if (ok) break;
    }

    if (!ok && !terminado) {
        /* El bucle terminó porque se agotaron los eventos sin llegar al
         * horizonte: es un fallo del modelo, no un resultado. */
        ok = 1;
    }

    /* --- Percentil 90 de los TPR (ordenamiento por inserción) --------- */
    if (s.n_tpr > 0) {
        int i, j;
        for (i = 1; i < s.n_tpr; i++) {
            double clave = s.tpr_valores[i];
            j = i - 1;
            while (j >= 0 && s.tpr_valores[j] > clave) {
                s.tpr_valores[j + 1] = s.tpr_valores[j];
                j--;
            }
            s.tpr_valores[j + 1] = clave;
        }
        {
            int pos = (int)ceil(0.90 * (double)s.n_tpr) - 1;
            if (pos < 0) pos = 0;
            if (pos >= s.n_tpr) pos = s.n_tpr - 1;
            out->tpr_p90 = s.tpr_valores[pos];
        }
    }

    /* --- Histograma de TPR ------------------------------------------- */
    if (s.histo_max > 0.0) {
        int i;
        s.histo_ancho = s.histo_max / (double)HISTO_CLASES;
        for (i = 0; i < s.n_tpr; i++) {
            int clase = (int)(s.tpr_valores[i] / s.histo_ancho);
            if (clase >= HISTO_CLASES) clase = HISTO_CLASES - 1;
            if (clase < 0) clase = 0;
            s.histo[clase]++;
        }
        memcpy(out->histo, s.histo, sizeof(out->histo));
        out->histo_ancho = s.histo_ancho;
        out->histo_max = s.histo_max;
    }

    /* --- Series ------------------------------------------------------ */
    out->series_n = s.n_series;
    if (s.n_series > 0) {
        memcpy(out->series, s.series, sizeof(serie_t) * (size_t)s.n_series);
    }

    /* --- Ocupación, colas y trabajo en el sistema -------------------- */
    out->ocupacion_diagnostico = s.area_ocup_bahia;
    out->ocupacion_reparacion = s.area_ocup_mec;
    out->horas_vacias = s.area_vacia;
    if (horizonte > s.t_calentamiento) {
        double horas_medidas = horizonte - s.t_calentamiento;
        out->wip_diagnostico_prom = s.area_wip_diag / horas_medidas;
        out->wip_reparacion_prom = s.area_wip_rep / horas_medidas;
        out->cola_media_diag = s.area_cola_diag / horas_medidas;
        out->cola_media_rep = s.area_cola_rep / horas_medidas;
    }
    if (s.diag_dur_n > 0) {
        out->permanencia_diagnostico = s.diag_dur_suma / (double)s.diag_dur_n;
    }
    if (s.rep_dur_n > 0) {
        out->permanencia_reparacion = s.rep_dur_suma / (double)s.rep_dur_n;
    }
    out->muestras_prng = s.rng.muestras;

    /* --- Limpieza ---------------------------------------------------- */
    free(s.vehiculos);
    fifo_lib(&s.cola_diag);
    fifo_lib(&s.cola_rep);
    fifo_lib(&s.cola_refac);
    free(s.tpr_valores);
    cola_lib(&s.eventos);

    return ok;
}
