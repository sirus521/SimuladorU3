int g_tb_calls=0; int g_tb_none=0;
/* juego.c — Motor de partida. Ver juego.h y notas/03-modelo-matematico.md.
 *
 * Idea central: el servicio **no empieza solo**. Un puesto sólo avanza si el
 * jugador o un empleado lo inicia. Eso es lo que convierte el modelo en
 * juego: el jugador no compite contra el azar, decide a qué se dedication.
 */
#include "sim/juego.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cjson/cJSON.h"

/* ------------------------------------------------------------------ */
/* Eventos de la partida                                                */
/* ------------------------------------------------------------------ */

typedef enum {
    EV_LLEGADA = 0,
    EV_FIN_RECEPCION,
    EV_FIN_DIAGNOSTICO,
    EV_DECISION_CLIENTE,
    EV_FIN_ALMACEN,
    EV_FIN_REPARACION,
    EV_FIN_ENTREGA,
    EV_LOTE_PROVEEDOR,
    EV_FIN_COTIZACION,
    EV_FIN_DIA,
    EV_FIN_PARTIDA
} ev_juego_t;

/* Puestos que puede ocupar un tipo de empleado.
 *
 * El recepcionista y el cotizador cubren también la entrega: los dos están
 * en el frente del local, que es donde se cobra. Si nadie atendiera la
 * entrega, los vehículos repairing se quedaban parados en la cola de
 * entrega y el taller no facturaba nada. */
static const int PUESTOS_DE_EMPLEADO[EMPLEADO_N_TIPOS] = {
    PUESTO_RECEPCION,             /* recepcionista: recepción             */
    PUESTO_REPARACION,            /* mecánico: diagnóstico o reparación   */
    PUESTO_ALMACEN,               /* almacenista: almacén                 */
    PUESTO_COTIZACION             /* cotizador: cotización                */
};

/* Puestos alternativos, por tipo de empleado. */
static const int PUESTOS_ALT_RECEPCIONISTA[2] = { PUESTO_RECEPCION, PUESTO_ENTREGA };
static const int PUESTOS_ALT_COTIZADOR[2]    = { PUESTO_COTIZACION, PUESTO_ENTREGA };

/* Puestos que un mecánico puede tomar, en orden de preferencia. */
static const int PUESTOS_MECANICO[2] = { PUESTO_DIAGNOSTICO, PUESTO_REPARACION };

/* ------------------------------------------------------------------ */
/* Economía: valores por defecto                                        */
/* ------------------------------------------------------------------ */

/* --- Escala de tiempos del juego -------------------------------------
 *
 * El juego usa su propia escala, más corta que la del modelo analítico
 * (notas/03 § 6.1). La razón es práctica: un turno de 9 h de juego dura
 * unos 9 minutos reales, así que los tiempos de servicio tienen que caber
 * en ese turno. Con los tiempos del modelo analítico (2 a 6 h de
 * reparación) un turno de 9 h no daba ni para 5 trabajos, y la cuota de 12
 * era inalcanzable por construcción.
 *
 * La ESTRUCTURA del modelo es la misma (colas, cuello de botella, relaciones
 * L = λW); lo que cambia es la escala de los tiempos.
 */
#define PRECIO_BASE_TIPO   { 6000, 11000, 18000 }
#define COSTE_PIEZA_TIPO   { 1500, 3000, 5000 }
#define MEDIA_DIAG_TIPO    { 0.20, 0.30, 0.45 }
#define MEDIA_REP_TIPO     { 0.80, 1.30, 2.20 }

#define MEDIA_RECEPCION    0.10
#define MEDIA_COTIZACION   0.10
#define MEDIA_ALMACEN      0.10
#define MEDIA_ENTREGA      0.10

#define NOMINA_DIARIA        2000
#define ALQUILER_DIARIO     4000
/* Saldo inicial: cubre alquiler y la nómina de los primeros turnos, para
 * que el jugador pueda contratar a alguien nada más empezar. */
#define SALDO_INICIAL      25000
#define PRECIO_LOTE         1500
#define PIEZAS_POR_LOTE     20
/* Plazo normal del proveedor: llega al abrir del dia siguiente. */
#define PLAZO_LOTE_HORAS    24.0
/* Lote urgente: llega en una hora, pero cuesta el doble. Es la salida
 * cuando el taller se queda sin material a media jornada. */
#define PRECIO_LOTE_URGENTE 3000
#define PLAZO_LOTE_URGENTE_H 1.0
#define REPUTACION_INICIAL  70
#define CUOTA_DIARIA        10
#define DIAS_PARTIDA        30
#define HORAS_ATENCION      9.0
#define HORA_APERTURA       9.0

/* Tolerancia del cliente sobre el precio justo: la disposición a pagar
 * se sortea en [0.80, 1.20] × precio justo.
 *
 * Esto es lo que da al jugador una decisión con contenido: el NPC cotiza
 * al 85 % del precio justo (y por tanto pierde ~12 % de los clientes), pero
 * el jugador puede subir el precio y ganar margen a costa de clientes, o
 * bajarlo para llenar el taller. Con la disposición en [0.8, 1.2] y un
 * precio en 0.85, el rechazo es de ~12 %.
 */
#define TOLERANCIA_PRECIO 0.20
/* Variación del trabajo. */
#define RUIDO_TRABAJO      0.10
/* Un cliente se va si espera más de esto en recepción. */
#define PACIENCIA_RECEPCION_H 1.5

/* ------------------------------------------------------------------ */
/* Utilidades                                                          */
/* ------------------------------------------------------------------ */

static int precio_tipo(int tipo)
{
    static const int precios[3] = PRECIO_BASE_TIPO;
    if (tipo < 0 || tipo > 2) return 0;
    return precios[tipo];
}

static int coste_pieza(int tipo)
{
    static const int costes[3] = COSTE_PIEZA_TIPO;
    if (tipo < 0 || tipo > 2) return 0;
    return costes[tipo];
}

static double media_diag_tipo(int tipo)
{
    static const double medias[3] = MEDIA_DIAG_TIPO;
    if (tipo < 0 || tipo > 2) return 0.5;
    return medias[tipo];
}

static double media_rep_tipo(int tipo)
{
    static const double medias[3] = MEDIA_REP_TIPO;
    if (tipo < 0 || tipo > 2) return 2.0;
    return medias[tipo];
}

/* Número de piezas que consume cada tipo. */
static int piezas_de_tipo(int tipo)
{
    return tipo == 2 ? 3 : (tipo == 1 ? 2 : 1);
}

/* Uniforme en [0,1) del PRNG propio. */
static double uniforme(juego_t *g)
{
    double u = prng_uniforme(&g->rng);
    g->muestras_prng = g->rng.muestras;
    return u;
}

/* Reparte un vehículo en el estado indicado y lo mete en la cola. */
static void encolar(juego_t *g, int v, int puesto, estado_vehiculo_juego_t estado)
{
    g->vehiculos[v].estado = estado;
    g->vehiculos[v].t_inicio_espera = g->t_actual;
    fifo_push(&g->cola[puesto], v);
}

/* ------------------------------------------------------------------ */
/* Puestos: capacidad y ocupación                                       */
/* ------------------------------------------------------------------ */

/* Capacidad de un puesto (nº de sitios paralelos). */
/* ¿Está el puesto ocupado por encima de su capacidad? */
static int puesto_saturado(const juego_t *g, int puesto)
{
    switch (puesto) {
        case PUESTO_DIAGNOSTICO: return g->en_uso_diag >= g->capacidad_bahias_diag;
        case PUESTO_REPARACION:  return g->en_uso_rep >= g->capacidad_bahias_rep;
        default: return 0;
    }
}

static void ocupar_puesto(juego_t *g, int puesto)
{
    if (puesto == PUESTO_DIAGNOSTICO) g->en_uso_diag++;
    else if (puesto == PUESTO_REPARACION) g->en_uso_rep++;
}

static void liberar_puesto(juego_t *g, int puesto)
{
    if (puesto == PUESTO_DIAGNOSTICO) { if (g->en_uso_diag > 0) g->en_uso_diag--; }
    else if (puesto == PUESTO_REPARACION) { if (g->en_uso_rep > 0) g->en_uso_rep--; }
}

/* ------------------------------------------------------------------ */
/* Llegadas                                                             */
/* ------------------------------------------------------------------ */

/* Factor de demanda por reputación (notas/03 § 3.1). */
static double factor_reputacion(const juego_t *g)
{
    return 0.6 + 0.8 * ((double)g->reputacion / 100.0);
}

/* Genera la próxima llegada. Poisson no estacionario dentro del horario. */
static double proxima_llegada(juego_t *g, double t_actual)
{
    double lambda_por_hora = (g->cfg.llegadas_por_dia / HORAS_ATENCION) * factor_reputacion(g);
    double media_salto = 1.0 / lambda_por_hora;
    double t = t_actual;

    for (;;) {
        double dia = floor(t / 24.0);
        double apertura = dia * 24.0 + HORA_APERTURA;
        double cierre = dia * 24.0 + HORA_APERTURA + HORAS_ATENCION;

        if (t < apertura) { t = apertura; continue; }
        if (t >= cierre) { t = (dia + 1.0) * 24.0 + HORA_APERTURA; continue; }

        {
            double salto = -media_salto * log(1.0 - uniforme(g));
            if (t + salto < cierre) return t + salto;
            t = (dia + 1.0) * 24.0 + HORA_APERTURA;
        }
    }
}

/* Crea un cliente nuevo y lo pone en la cola de recepción. */
static void generar_llegada(juego_t *g)
{
    int v;
    double acum[JUEGO_MAX_TIPOS];
    double suma = 0.0;
    int i;

    if (g->n_vehiculos >= g->cap_vehiculos) {
        if (g->cap_vehiculos >= JUEGO_MAX_VEHICULOS) return;
        g->cap_vehiculos = g->cap_vehiculos ? g->cap_vehiculos * 2 : 64;
        if (g->cap_vehiculos > JUEGO_MAX_VEHICULOS) g->cap_vehiculos = JUEGO_MAX_VEHICULOS;
    }
    v = g->n_vehiculos++;
    memset(&g->vehiculos[v], 0, sizeof(vehiculo_juego_t));
    g->vehiculos[v].estado = V_ESPERA_RECEPCION;
    g->vehiculos[v].t_llegada = g->t_actual;
    g->vehiculos[v].t_entrada_sistema = g->t_actual;
    g->vehiculos[v].t_inicio_espera = g->t_actual;

    /* Tipo de avería (categórica 40/40/20). */
    for (i = 0; i < g->cfg.n_tipos; i++) {
        suma += g->cfg.peso[i];
        acum[i] = suma;
    }
    acum[g->cfg.n_tipos - 1] = 1.0;
    {
        double u = uniforme(g);
        int tipo = g->cfg.n_tipos - 1;
        for (i = 0; i < g->cfg.n_tipos; i++) {
            if (u < acum[i]) { tipo = i; break; }
        }
        g->vehiculos[v].tipo = tipo;
    }

    /* Disposición a pagar: precio justo con ruido ±25 %. */
    {
        double justo = (double)precio_tipo(g->vehiculos[v].tipo);
        double ruido = (uniforme(g) * 2.0 - 1.0) * TOLERANCIA_PRECIO;
        g->vehiculos[v].disposicion = justo * (1.0 + ruido);
    }

    fifo_push(&g->cola[PUESTO_RECEPCION], v);
}

/* ------------------------------------------------------------------ */
/* Acciones sobre un vehículo                                           */
/* ------------------------------------------------------------------ */

/* Inicia el trabajo de un vehículo en un puesto. Devuelve 1 si se aceptó. */
static int iniciar_trabajo(juego_t *g, int v, int puesto)
{
    vehiculo_juego_t *veh = &g->vehiculos[v];
    double duracion;

    switch (puesto) {
        case PUESTO_RECEPCION:
            duracion = MEDIA_RECEPCION;
            veh->estado = V_EN_RECEPCION;
            break;
        case PUESTO_DIAGNOSTICO:
            duracion = media_diag_tipo(veh->tipo);
            veh->estado = V_EN_DIAGNOSTICO;
            break;
        case PUESTO_COTIZACION:
            duracion = MEDIA_COTIZACION;
            veh->estado = V_EN_COTIZACION;
            break;
        case PUESTO_ALMACEN:
            duracion = MEDIA_ALMACEN;
            veh->estado = V_EN_ALMACEN;
            break;
        case PUESTO_REPARACION:
            duracion = media_rep_tipo(veh->tipo);
            veh->estado = V_EN_REPARACION;
            break;
        case PUESTO_ENTREGA:
            duracion = MEDIA_ENTREGA;
            veh->estado = V_EN_ENTREGA;
            break;
        default:
            return 0;
    }

    /* Ruido de ejecución ±10 %. */
    duracion *= (1.0 + (uniforme(g) * 2.0 - 1.0) * RUIDO_TRABAJO);
    if (duracion < 0.01) duracion = 0.01;

    veh->t_inicio_trabajo = g->t_actual;
    ocupar_puesto(g, puesto);

    {
        int ev = EV_FIN_RECEPCION;
        switch (puesto) {
            case PUESTO_RECEPCION:    ev = EV_FIN_RECEPCION; break;
            case PUESTO_DIAGNOSTICO:  ev = EV_FIN_DIAGNOSTICO; break;
            case PUESTO_COTIZACION:   ev = EV_FIN_COTIZACION; break;
            case PUESTO_ALMACEN:      ev = EV_FIN_ALMACEN; break;
            case PUESTO_REPARACION:   ev = EV_FIN_REPARACION; break;
            case PUESTO_ENTREGA:      ev = EV_FIN_ENTREGA; break;
            default: break;
        }
        cola_push(&g->eventos, g->t_actual + duracion, ev, v);
    }
    return 1;
}

/* Transición al puesto siguiente tras terminar un trabajo. */
static void avanzar_puesto(juego_t *g, int v, int puesto)
{
    switch (puesto) {
        case PUESTO_RECEPCION:
            encolar(g, v, PUESTO_DIAGNOSTICO, V_ESPERA_DIAGNOSTICO);
            break;
        case PUESTO_DIAGNOSTICO:
            encolar(g, v, PUESTO_COTIZACION, V_ESPERA_COTIZACION);
            break;
        case PUESTO_COTIZACION:
            /* Ya se cotizó: el cliente decide tras su tiempo de espera. */
            g->vehiculos[v].espera_cotizar = 1;
            cola_push(&g->eventos, g->t_actual + 0.2, EV_DECISION_CLIENTE, v);
            break;
        case PUESTO_ALMACEN:
            encolar(g, v, PUESTO_REPARACION, V_ESPERA_REPARACION);
            break;
        case PUESTO_REPARACION:
            encolar(g, v, PUESTO_ENTREGA, V_ESPERA_ENTREGA);
            break;
        case PUESTO_ENTREGA:
            /* Se entrega aquí: el cobro ocurre en la acción del jugador. */
            break;
        default:
            break;
    }
}

const char *juego_nombre_puesto(int puesto)
{
    switch (puesto) {
        case PUESTO_RECEPCION:    return "recepcion";
        case PUESTO_DIAGNOSTICO:  return "diagnostico";
        case PUESTO_COTIZACION:   return "cotizacion";
        case PUESTO_ALMACEN:      return "almacen";
        case PUESTO_REPARACION:   return "reparacion";
        case PUESTO_ENTREGA:      return "entrega";
        default: return "desconocido";
    }
}

/* ------------------------------------------------------------------ */
/* Ciclo de vida                                                        */
/* ------------------------------------------------------------------ */

int juego_iniciar(juego_t *g, const config_t *cfg, uint32_t semilla)
{
    int i;
    memset(g, 0, sizeof(*g));
    g->cfg = *cfg;
    g->semilla = semilla;
    prng_iniciar(&g->rng, (uint16_t)(semilla & 0xFFFFu));

    for (i = 0; i < JUEGO_N_PUESTOS; i++) {
        fifo_ini(&g->cola[i]);
    }
    if (!cola_ini(&g->eventos, 1024)) {
        return 1;
    }
    g->cap_vehiculos = 64;

    g->capacidad_bahias_diag = cfg->bahias_diagnostico;
    /* Cada mecánico ocupa una bahía de reparación. La capacidad de
     * reparación la da el equipo contratado, no un número fijo: si no,
     * contratar más gente sólo añadiría nómina y no capacidad, y el juego
     * sería una trampa. La bahía mínima es 1 para que siempre se pueda
     * trabajar. */
    g->capacidad_bahias_rep = cfg->mecanicos > 0 ? cfg->mecanicos : 1;

    g->saldo = SALDO_INICIAL;
    g->reputacion = REPUTACION_INICIAL;
    g->cuota_diaria = CUOTA_DIARIA;
    g->stock_refacciones = cfg->refacciones_stock;
    g->lote_en_camino = 0;
    g->t_lote_llega = 0.0;

    g->jugador_puesto = -1;
    g->jugador_vehiculo = -1;
    g->dia_actual = 1;
    g->t_actual = HORA_APERTURA;
    g->velocidad = 1;

    /* Al abrir, ya hay un par de clientes esperando. Un taller que abre
     * vacío y tarda un minuto de juego real en tener al primer cliente se
     * siente como un juego roto: el jugador no tiene nada que hacer. */
    generar_llegada(g);
    generar_llegada(g);

    /* La primera llegada. */
    cola_push(&g->eventos, proxima_llegada(g, g->t_actual), EV_LLEGADA, -1);
    /* Cierre del primer día. */
    cola_push(&g->eventos, HORA_APERTURA + HORAS_ATENCION, EV_FIN_DIA, -1);
    /* Fin de la partida. */
    /* La partida termina al cerrar el último día de trabajo. El taller
     * abre de 09:00 a 18:00, así que el día 30 acaba en la hora
     * 29*24 + 18 = 714, no en 30*24 = 720: con el evento al final del
     * reloj la partida se alargaba un turno de más. */
    cola_push(&g->eventos,
              (double)(DIAS_PARTIDA - 1) * 24.0 + HORA_APERTURA + HORAS_ATENCION,
              EV_FIN_PARTIDA, -1);

    return 0;
}

/* ------------------------------------------------------------------ */
/* Cierre del día                                                       */
/* ------------------------------------------------------------------ */

static void cerrar_dia(juego_t *g)
{
    dia_juego_t *d;
    int nomina = 0;
    int dia_siguiente;
    int i;

    for (i = 0; i < g->n_empleados; i++) {
        nomina += NOMINA_DIARIA;
    }
    g->gastos_dia_int += nomina + ALQUILER_DIARIO;
    g->saldo -= nomina + ALQUILER_DIARIO;

    /* ¿Se cumplió la cuota? */
    if (g->entregas_dia >= g->cuota_diaria) {
        g->dias_sin_cuota = 0;
        g->reputacion += 1;
    } else {
        g->dias_sin_cuota++;
        g->reputacion -= 2;
    }
    if (g->reputacion > 100) g->reputacion = 100;
    if (g->reputacion < 0) g->reputacion = 0;

    /* Guarda el día. */
    if (g->n_historial < JUEGO_HISTORIAL_DIAS) {
        d = &g->historial[g->n_historial++];
        d->dia = g->dia_actual;
        d->entregas = g->entregas_dia;
        d->cuota = g->cuota_diaria;
        d->cumplio_cuota = (g->entregas_dia >= g->cuota_diaria);
        d->clientes_perdidos = g->perdidos_dia;
        d->ingresos = g->ingresos_dia_int;
        d->gastos = g->gastos_dia_int;
        d->saldo = g->saldo;
        d->reputacion = g->reputacion;
        d->espera_media_rep = g->espera_rep_n > 0 ? g->espera_rep_suma / g->espera_rep_n : 0.0;
    }

    /* Condiciones de derrota. */
    if (g->saldo < 0) {
        g->terminada = 1;
        g->ganada = 0;
        snprintf(g->motivo, sizeof(g->motivo), "El taller quebró");
        return;
    }
    if (g->reputacion <= 0) {
        g->terminada = 1;
        g->ganada = 0;
        snprintf(g->motivo, sizeof(g->motivo), "El taller perdió su fama");
        return;
    }
    if (g->dias_sin_cuota >= 3) {
        g->terminada = 1;
        g->ganada = 0;
        snprintf(g->motivo, sizeof(g->motivo), "Los clientes se fueron");
        return;
    }

    /* Empieza el día siguiente. Se guarda el número del día nuevo ANTES de
     * incrementarlo, porque las horas de apertura se calculan a partir de
     * él. Con el orden anterior, `dia_actual - 1` pointed al día ya
     * cerrado: el cierre se reagendaba en el pasado y el taller quedaba
     * sin cerrar los días siguientes. */
    dia_siguiente = g->dia_actual + 1;
    g->dia_actual = dia_siguiente;
    /* Los contadores del día se reinician, pero sus valores ya están en el
     * historial. OJO: el reinicio NO puede ocurrir antes de guardar el
     * parte, o el día cerrado aparecería con cero entregas. */
    g->entregas_dia = 0;
    g->perdidos_dia = 0;
    g->ingresos_dia_int = 0;
    g->gastos_dia_int = 0;
    g->espera_rep_suma = 0.0;
    g->espera_rep_n = 0;

    /* Al abrir el día nuevo hay clientes esperando, y se agenda la
     * siguiente llegada. */
    {
        double inicio = (double)(dia_siguiente - 1) * 24.0 + HORA_APERTURA;
        generar_llegada(g);
        cola_push(&g->eventos, inicio, EV_LLEGADA, -1);
        cola_push(&g->eventos, inicio + HORAS_ATENCION, EV_FIN_DIA, -1);
    }

    /* Lote que estaba en camino: llega al abrir el taller. El plazo de
     * entrega cruzaba la noche (el taller cierra de 18:00 a 09:00), así
     * que un lote pedido a las 10:00 no llegaba hasta las 10:00 del día
     * siguiente: el taller se quedaba medio día con la cola parada y sin
     * poder hacer nada. Con la entrega al abrir, el proveedor cumple en
     * un día, que es lo que dice el juego. */
    if (g->lote_en_camino > 0) {
        double entrega = g->t_lote_llega;
        double apertura = (double)(dia_siguiente - 1) * 24.0 + HORA_APERTURA;
        /* Si el lote tenía que llegar antes de que abriera el taller (es
         * el caso del urgente), se entrega en cuanto abre. */
        if (entrega < apertura) entrega = apertura;
        cola_push(&g->eventos, entrega, EV_LOTE_PROVEEDOR, -1);
    }
}

/* ------------------------------------------------------------------ */
/* NPCs: la IA que ocupa los puestos                                    */
/* ------------------------------------------------------------------ */

/* Elige el puesto que un empleado debería atender ahora, o -1.
 *
 * Para el mecánico, que puede trabajar en diagnóstico o en reparación, se
 * aplica la regla de **cola más corta**: se va al puesto con menos trabajo
 * pendiente (de los que no están saturados).
 *
 * No se usa "el que más cola tiene": con esa regla el mecánico se quedaba
 * siempre en el diagnóstico —su cola siempre era la más larga— y la
 * reparación nunca recibía a nadie. La cola de reparación crecía sin
 * freno y el taller no entregaba nada. Es inanición de cola, y se nota en
 * cuanto se mira el reparto de trabajo.
 */
static int elegir_puesto_empleado(const juego_t *g, int tipo_empleado)
{
    int mejor = -1;
    double mejor_espera = -1.0;
    int i;

    if (tipo_empleado == EMPLEADO_MECANICO) {
        for (i = 0; i < 2; i++) {
            int p = PUESTOS_MECANICO[i];
            int cabeza;
            double espera;
            if (puesto_saturado(g, p)) continue;
            cabeza = fifo_frente(&g->cola[p]);
            if (cabeza < 0) continue;
            /* Se atiende primero a quien mas ha esperado. Con la regla
             * contraria (la cola mas larga) una de las dos colas se queda
             * sin gente para siempre: los dos mecánicos se iban siempre al
             * diagnóstico y la reparación no recibía a nadie. */
            espera = g->t_actual - g->vehiculos[cabeza].t_inicio_espera;
            if (espera > mejor_espera) {
                mejor_espera = espera;
                mejor = p;
            }
        }
        return mejor;
    }

    {
        const int *alternativos = NULL;
        int n_alternativos = 1;
        int j;

        switch (tipo_empleado) {
            case EMPLEADO_RECEPCIONISTA:
                alternativos = PUESTOS_ALT_RECEPCIONISTA; n_alternativos = 2; break;
            case EMPLEADO_COTIZADOR:
                alternativos = PUESTOS_ALT_COTIZADOR;    n_alternativos = 2; break;
            default:
                /* El puesto propio del tipo de empleado. OJO: hay que
                 * desplazar por el índice del tipo; con el array entero
                 * todos los empleados elegían el puesto del primero
                 * (recepción) y el almacén nunca recibía a nadie. */
                alternativos = PUESTOS_DE_EMPLEADO + tipo_empleado;
                n_alternativos = 1;
                break;
        }

        for (j = 0; j < n_alternativos; j++) {
            int p = alternativos[j];
            int cabeza;
            double espera;
            if (puesto_saturado(g, p)) continue;
            cabeza = fifo_frente(&g->cola[p]);
            if (cabeza < 0) continue;
            espera = g->t_actual - g->vehiculos[cabeza].t_inicio_espera;
            if (espera > mejor_espera) {
                mejor_espera = espera;
                mejor = p;
            }
        }
        return mejor;
    }
}

/* Hace trabajar a los empleados: toman un puesto si hay cola. */
static void trabajar_empleados(juego_t *g)
{
    int i;
    { extern int g_tb_calls; g_tb_calls++; }
    for (i = 0; i < g->n_empleados; i++) {
        empleado_t *e = &g->empleados[i];
        int puesto;
        int v;

        if (e->ocupado) continue;

        puesto = elegir_puesto_empleado(g, e->tipo);
        { extern int g_tb_calls; extern int g_tb_none; if (puesto < 0) g_tb_none++;
          else fprintf(stderr,"    [emp %d tipo %d] -> puesto %d\n", e->id, e->tipo, puesto); }
        if (puesto < 0) continue;

        /* Si el jugador está en ese puesto, el NPC espera. */
        if (g->jugador_puesto == puesto) continue;

        v = fifo_pop(&g->cola[puesto]);
        if (v < 0) continue;

        e->ocupado = 1;
        e->puesto_actual = puesto;
        e->vehiculo = v;
        g->vehiculos[v].t_inicio_espera = g->t_actual;

        if (puesto == PUESTO_COTIZACION) {
            /* El NPC cotiza al precio justo y el cliente decide en breve.
             * El vehículo pasa por el estado "en cotización" y el puesto
             * queda ocupado mientras tanto, como en cualquier otro. */
            g->vehiculos[v].estado = V_EN_COTIZACION;
            /* El NPC cotiza un 15 % por debajo del precio justo: si
             * cotizara al precio justo, la mitad de los clientes lo
             * rechazaría (su disposición a pagar va de -25 % a +25 % del
             * precio justo) y el taller no podría funcionar ni sin
             * jugador. El margen se lo queda el taller. */
            g->vehiculos[v].precio_ofrecido =
                (int)(precio_tipo(g->vehiculos[v].tipo) * 0.85);
            g->vehiculos[v].espera_cotizar = 1;
            ocupar_puesto(g, puesto);
            cola_push(&g->eventos, g->t_actual + MEDIA_COTIZACION, EV_FIN_COTIZACION, v);
            continue;
        }
        iniciar_trabajo(g, v, puesto);
    }
}

/* ------------------------------------------------------------------ */
/* Procesado de eventos                                                 */
/* ------------------------------------------------------------------ */

/* Libera a quien estuviera trabajando en ese vehículo: el jugador o un
 * empleado. Sin esto, un puesto quedaría ocupado para siempre y el NPC se
 * quedaría "trabajando" un trabajo ya terminado. */
static void liberar_trabajador(juego_t *g, int vehiculo)
{
    int i;
    if (g->jugador_vehiculo == vehiculo) {
        g->jugador_vehiculo = -1;
        g->jugador_puesto = -1;
    }
    for (i = 0; i < g->n_empleados; i++) {
        empleado_t *e = &g->empleados[i];
        if (e->vehiculo == vehiculo) {
            e->ocupado = 0;
            e->vehiculo = -1;
            e->puesto_actual = -1;
        }
    }
}

static void procesar_evento(juego_t *g, const evento_t *ev)
{
    switch (ev->tipo) {
    case EV_LLEGADA:
        if (!g->terminada) {
            generar_llegada(g);
            cola_push(&g->eventos, proxima_llegada(g, g->t_actual), EV_LLEGADA, -1);
        }
        break;

    case EV_FIN_RECEPCION: {
        int v = ev->indice;
        int p = PUESTO_RECEPCION;
        liberar_puesto(g, p);
        liberar_trabajador(g, v);
        avanzar_puesto(g, v, p);
        break;
    }

    case EV_FIN_DIAGNOSTICO: {
        int v = ev->indice;
        int p = PUESTO_DIAGNOSTICO;
        liberar_puesto(g, p);
        liberar_trabajador(g, v);
        avanzar_puesto(g, v, p);
        break;
    }

    case EV_FIN_COTIZACION: {
        int v = ev->indice;
        int p = PUESTO_COTIZACION;
        liberar_puesto(g, p);
        liberar_trabajador(g, v);
        /* El precio lo puso el jugador o el NPC. Si nadie lo puso (por
         * ejemplo, si el puesto se liberó de otra forma), se usa el justo. */
        if (g->vehiculos[v].precio_ofrecido <= 0) {
            g->vehiculos[v].precio_ofrecido = precio_tipo(g->vehiculos[v].tipo);
        }
        g->vehiculos[v].espera_cotizar = 1;
        /* El cliente tarda un poco en decidir. */
        cola_push(&g->eventos, g->t_actual + 0.2, EV_DECISION_CLIENTE, v);
        break;
    }

    case EV_DECISION_CLIENTE: {
        int v = ev->indice;
        vehiculo_juego_t *veh = &g->vehiculos[v];
        if ((double)veh->precio_ofrecido <= veh->disposicion) {
            /* Acepta: pasa a almacén. */
            encolar(g, v, PUESTO_ALMACEN, V_ESPERA_ALMACEN);
        } else {
            /* Rechaza: se va. */
            veh->estado = V_RECHAZADO;
            veh->t_salida = g->t_actual;
            veh->motivo_salida = 1;
            g->perdidos_dia++;
            /* El castigo por cliente perdido es de 2 puntos, no de 5: con
             * 14 llegadas al día, un mal día (6 perdidos) restaba 30
             * puntos y mataba la partida en dos días, sin margen para
             * reaccionar. Con 2, la reputación baja cuando el jugador falla
             * de verdad y sube cuando rectifica. */
            g->reputacion -= 2;
            if (g->reputacion < 0) g->reputacion = 0;
        }
        break;
    }

    case EV_FIN_ALMACEN: {
        int v = ev->indice;
        int p = PUESTO_ALMACEN;
        int necesarias = piezas_de_tipo(g->vehiculos[v].tipo);
        liberar_puesto(g, p);
        liberar_trabajador(g, v);
        if (g->stock_refacciones >= necesarias) {
            g->stock_refacciones -= necesarias;
            g->vehiculos[v].piezas_gastadas = necesarias;
            /* El coste de la pieza NO se imputa aquí: se imputa al entregar
             * el trabajo. Si se cargara al tomar la pieza, un día en el que
             * se empieza mucho trabajo pero no se entrega nada dejaría al
             * taller sin caja, y la partida sería imposible de Sobrevivir
             * por un mal día. */
            encolar(g, v, PUESTO_REPARACION, V_ESPERA_REPARACION);
        } else {
            /* No hay piezas: vuelve a esperar en almacén. La espera se
             * reinicia, porque si no el puesto de almacén parecería el más
             * antigo y se llevaría siempre a todos los empleados. */
            g->vehiculos[v].estado = V_ESPERA_ALMACEN;
            g->vehiculos[v].t_inicio_espera = g->t_actual;
            fifo_push(&g->cola[PUESTO_ALMACEN], v);
        }
        break;
    }

    case EV_FIN_REPARACION: {
        int v = ev->indice;
        int p = PUESTO_REPARACION;
        double espera = g->t_actual - g->vehiculos[v].t_entrada_sistema;
        liberar_puesto(g, p);
        liberar_trabajador(g, v);
        g->espera_rep_suma += espera;
        g->espera_rep_n++;
        encolar(g, v, PUESTO_ENTREGA, V_ESPERA_ENTREGA);
        break;
    }

    case EV_FIN_ENTREGA: {
        int v = ev->indice;
        int p = PUESTO_ENTREGA;
        int precio = g->vehiculos[v].precio_ofrecido;
        liberar_puesto(g, p);
        liberar_trabajador(g, v);
        g->vehiculos[v].estado = V_ENTREGADO;
        g->vehiculos[v].t_salida = g->t_actual;
        g->vehiculos[v].motivo_salida = 0;
        g->saldo += precio;
        g->ingresos_dia_int += precio;
        g->ingresos_totales += precio;
        /* Imputa el coste del material usado en este trabajo. */
        {
            int coste = coste_pieza(g->vehiculos[v].tipo);
            g->gastos_dia_int += coste;
            g->saldo -= coste;
        }
        g->entregas_dia++;
        /* Cada entrega sube la reputación 3 puntos y cada cliente perdido
         * la baja 2: así un día con la cuota cumplida compensa un día
         * flojo, y la reputación es un termómetro del desempeño, no una
         * sentencia. */
        g->reputacion += 3;
        if (g->reputacion > 100) g->reputacion = 100;
        break;
    }

    case EV_LOTE_PROVEEDOR:
        g->stock_refacciones += g->lote_en_camino;
        g->lote_en_camino = 0;
        break;

    case EV_FIN_DIA:
        if (!g->terminada) {
            cerrar_dia(g);
        }
        break;

    case EV_FIN_PARTIDA:
        if (!g->terminada) {
            g->terminada = 1;
            g->ganada = (g->saldo >= 0 && g->reputacion > 0);
            snprintf(g->motivo, sizeof(g->motivo), "El taller sobrevivió al mes");
        }
        break;

    default:
        break;
    }
}

/* ------------------------------------------------------------------ */
/* Avance del tiempo                                                     */
/* ------------------------------------------------------------------ */

void juego_avanzar(juego_t *g, double dt_horas)
{
    evento_t ev;
    double objetivo;

    if (g->terminada || dt_horas <= 0.0) return;

    objetivo = g->t_actual + dt_horas;

    while (!g->terminada && cola_frente(&g->eventos, &ev)) {
        if (ev.t > objetivo) {
            /* El evento más próximo está en el futuro: se sale sin sacarlo.
             *
             * Antes se sacaba y se volvía a encolar, y eso duplicaba
             * el cierre de día: el EV_FIN_DIA ya agendado se re-encolaba
             * (con un orden nuevo) *y* cerrar_dia() agendaba otro, así que
             * la nómina se cobraba dos veces por jornada y el taller
             * quebraba el primer día. Con `cola_frente` el evento se
             * consulta sin tocar la cola.
             */
            break;
        }
        cola_pop(&g->eventos, &ev);
        g->t_actual = ev.t;
        procesar_evento(g, &ev);

        /* Tras cada evento, los empleados pueden tomar trabajo. */
        if (!g->terminada) {
            trabajar_empleados(g);
        }
    }
    g->t_actual = objetivo;
}

/* ------------------------------------------------------------------ */
/* Acciones del jugador                                                 */
/* ------------------------------------------------------------------ */

int juego_puesto_disponible(const juego_t *g)
{
    int p;
    for (p = 0; p < JUEGO_N_PUESTOS; p++) {
        if (fifo_vacia(&g->cola[p])) continue;
        if (puesto_saturado(g, p)) continue;
        if (g->jugador_puesto == p) continue;
        return p;
    }
    return -1;
}

int juego_hay_accion(const juego_t *g)
{
    return juego_puesto_disponible(g) >= 0;
}

int juego_hay_trabajo(const juego_t *g, int puesto)
{
    if (puesto < 0 || puesto >= JUEGO_N_PUESTOS) return 0;
    if (puesto_saturado(g, puesto)) return 0;
    return !fifo_vacia(&g->cola[puesto]);
}

int juego_accion_atender(juego_t *g, int puesto)
{
    int v;

    if (puesto < 0 || puesto >= JUEGO_N_PUESTOS) return 0;
    if (puesto_saturado(g, puesto)) return 0;

    /* Un puesto con el jugador ocupado no se puede volver a tomar. */
    if (g->jugador_vehiculo >= 0) return 0;

    v = fifo_pop(&g->cola[puesto]);
    if (v < 0) return 0;

    g->jugador_puesto = puesto;
    g->jugador_vehiculo = v;
    g->jugador_t_inicio = g->t_actual;
    g->vehiculos[v].t_inicio_espera = g->t_actual;

    if (puesto == PUESTO_COTIZACION) {
        /* Cotizar no es un trabajo con resultado aleatorio: es una decisión
         * del jugador. Se queda en el mostrador esperando el precio, y lo
         * libera con juego_accion_cotizar(). */
        g->vehiculos[v].estado = V_EN_COTIZACION;
        g->vehiculos[v].precio_ofrecido = 0;
        return 1;
    }

    iniciar_trabajo(g, v, puesto);
    return 1;
}

int juego_accion_cotizar(juego_t *g, int precio)
{
    int v = g->jugador_vehiculo;

    if (g->jugador_puesto != PUESTO_COTIZACION || v < 0) {
        return 0;
    }
    if (precio < 0) return 0;

    g->vehiculos[v].precio_ofrecido = precio;
    g->vehiculos[v].espera_cotizar = 1;

    /* El cliente tarda un poco en decidir. Mientras tanto, el jugador ya
     * está libre para atender otro puesto. */
    cola_push(&g->eventos, g->t_actual + 0.2, EV_DECISION_CLIENTE, v);

    g->jugador_puesto = -1;
    g->jugador_vehiculo = -1;
    return 1;
}

int juego_accion_pedir_lote_ex(juego_t *g, int urgente)
{
    int precio = urgente ? PRECIO_LOTE_URGENTE : PRECIO_LOTE;
    double plazo = urgente ? PLAZO_LOTE_URGENTE_H : PLAZO_LOTE_HORAS;

    if (g->saldo < precio) return 0;
    if (g->lote_en_camino > 0) return 0;   /* ya hay un lote en camino */
    g->saldo -= precio;
    g->gastos_dia_int += precio;
    g->lote_en_camino = PIEZAS_POR_LOTE;
    g->t_lote_llega = g->t_actual + plazo;
    cola_push(&g->eventos, g->t_lote_llega, EV_LOTE_PROVEEDOR, -1);
    return 1;
}

int juego_accion_pedir_lote(juego_t *g)
{
    return juego_accion_pedir_lote_ex(g, 0);
}

int juego_accion_contratar(juego_t *g, int tipo)
{
    empleado_t *e;
    if (tipo < 0 || tipo >= EMPLEADO_N_TIPOS) return 0;
    if (g->n_empleados >= JUEGO_MAX_EMPLEADOS) return 0;
    /* No se puede contratar sin dinero para la nómina del día. */
    if (g->saldo < NOMINA_DIARIA) return 0;
    /* Ni se pueden recibir másnomina de la que el taller aguanta: con el
     * saldo justo, contratar de más llevaría a la quiebra segura. */
    {
        int nomina_actual = g->n_empleados * NOMINA_DIARIA;
        if (nomina_actual + NOMINA_DIARIA + ALQUILER_DIARIO > g->saldo) {
            return 0;
        }
    }

    e = &g->empleados[g->n_empleados++];
    memset(e, 0, sizeof(*e));
    e->id = g->n_empleados;
    e->tipo = tipo;
    e->puesto_actual = -1;
    e->vehiculo = -1;
    e->t_libre_desde = g->t_actual;

    /* Un mecánico más es una bahía de reparación más. */
    if (tipo == EMPLEADO_MECANICO) {
        g->capacidad_bahias_rep++;
    }
    return 1;
}

const vehiculo_juego_t *juego_vehiculo(const juego_t *g, int indice)
{
    if (indice < 0 || indice >= g->n_vehiculos) return NULL;
    return &g->vehiculos[indice];
}

/* ------------------------------------------------------------------ */
/* Snapshot para el cliente                                             */
/* ------------------------------------------------------------------ */

struct cJSON *juego_a_json(const juego_t *g)
{
    cJSON *raiz = cJSON_CreateObject();
    cJSON *vehiculos = cJSON_CreateArray();
    cJSON *colas = cJSON_CreateObject();
    cJSON *empleados = cJSON_CreateArray();
    cJSON *historial = cJSON_CreateArray();
    int i;

    cJSON_AddNumberToObject(raiz, "dia", g->dia_actual);
    cJSON_AddNumberToObject(raiz, "t_actual", g->t_actual);
    cJSON_AddNumberToObject(raiz, "saldo", g->saldo);
    cJSON_AddNumberToObject(raiz, "ingresos_dia", g->ingresos_dia_int);
    cJSON_AddNumberToObject(raiz, "gastos_dia", g->gastos_dia_int);
    cJSON_AddNumberToObject(raiz, "reputacion", g->reputacion);
    cJSON_AddNumberToObject(raiz, "cuota", g->cuota_diaria);
    cJSON_AddNumberToObject(raiz, "entregas_dia", g->entregas_dia);
    cJSON_AddNumberToObject(raiz, "perdidos_dia", g->perdidos_dia);
    cJSON_AddNumberToObject(raiz, "stock_refacciones", g->stock_refacciones);
    cJSON_AddNumberToObject(raiz, "lote_en_camino", g->lote_en_camino);
    cJSON_AddNumberToObject(raiz, "jugador_puesto", g->jugador_puesto);
    cJSON_AddNumberToObject(raiz, "jugador_vehiculo", g->jugador_vehiculo);
    cJSON_AddNumberToObject(raiz, "terminada", g->terminada);
    cJSON_AddNumberToObject(raiz, "ganada", g->ganada);
    cJSON_AddStringToObject(raiz, "motivo", g->motivo);
    cJSON_AddNumberToObject(raiz, "semilla", (double)g->semilla);

    /* Reglas económicas: las manda el servidor para que el cliente no las
     * duplique (y no se desincronicen). */
    cJSON_AddNumberToObject(raiz, "nomina_diaria", NOMINA_DIARIA);
    cJSON_AddNumberToObject(raiz, "alquiler_diario", ALQUILER_DIARIO);
    cJSON_AddNumberToObject(raiz, "precio_lote", PRECIO_LOTE);
    cJSON_AddNumberToObject(raiz, "precio_lote_urgente", PRECIO_LOTE_URGENTE);
    cJSON_AddNumberToObject(raiz, "plazo_lote_urgente_h", PLAZO_LOTE_URGENTE_H);
    cJSON_AddNumberToObject(raiz, "piezas_por_lote", PIEZAS_POR_LOTE);
    cJSON_AddNumberToObject(raiz, "plazo_lote_h", PLAZO_LOTE_HORAS);
    cJSON_AddNumberToObject(raiz, "horas_atencion", HORAS_ATENCION);
    cJSON_AddNumberToObject(raiz, "dias_partida", DIAS_PARTIDA);

    for (i = 0; i < JUEGO_N_PUESTOS; i++) {
        static const char *nombres[JUEGO_N_PUESTOS] = {
            "recepcion", "diagnostico", "cotizacion", "almacen", "reparacion", "entrega"
        };
        cJSON_AddNumberToObject(colas, nombres[i], fifo_pending(&g->cola[i]));
    }
    cJSON_AddItemToObject(raiz, "colas", colas);

    for (i = 0; i < g->n_vehiculos; i++) {
        const vehiculo_juego_t *v = &g->vehiculos[i];
        /* Sólo los vehículos activos: los ya entregados no se dibujan. */
        if (v->estado == V_ENTREGADO || v->estado == V_RECHAZADO ||
            v->estado == V_IDO) {
            continue;
        }
        {
            cJSON *o = cJSON_CreateObject();
            cJSON_AddNumberToObject(o, "id", i);
            cJSON_AddNumberToObject(o, "tipo", v->tipo);
            cJSON_AddNumberToObject(o, "estado", (double)v->estado);
            cJSON_AddNumberToObject(o, "t_llegada", v->t_llegada);
            cJSON_AddNumberToObject(o, "espera", g->t_actual - v->t_entrada_sistema);
            cJSON_AddNumberToObject(o, "disposicion", v->disposicion);
            cJSON_AddNumberToObject(o, "precio_ofrecido", v->precio_ofrecido);
            cJSON_AddNumberToObject(o, "precio_justo", precio_tipo(v->tipo));
            cJSON_AddNumberToObject(o, "coste_pieza", coste_pieza(v->tipo));
            cJSON_AddNumberToObject(o, "piezas_necesarias", piezas_de_tipo(v->tipo));
            cJSON_AddNumberToObject(o, "en_espera", g->t_actual - v->t_inicio_espera);
            cJSON_AddItemToArray(vehiculos, o);
        }
    }
    cJSON_AddItemToObject(raiz, "vehiculos", vehiculos);

    for (i = 0; i < g->n_empleados; i++) {
        const empleado_t *e = &g->empleados[i];
        cJSON *o = cJSON_CreateObject();
        cJSON_AddNumberToObject(o, "id", e->id);
        cJSON_AddNumberToObject(o, "tipo", e->tipo);
        cJSON_AddNumberToObject(o, "puesto", e->puesto_actual);
        cJSON_AddNumberToObject(o, "ocupado", e->ocupado);
        cJSON_AddItemToArray(empleados, o);
    }
    cJSON_AddItemToObject(raiz, "empleados", empleados);

    for (i = 0; i < g->n_historial; i++) {
        const dia_juego_t *d = &g->historial[i];
        cJSON *o = cJSON_CreateObject();
        cJSON_AddNumberToObject(o, "dia", d->dia);
        cJSON_AddNumberToObject(o, "entregas", d->entregas);
        cJSON_AddNumberToObject(o, "cuota", d->cuota);
        cJSON_AddNumberToObject(o, "cumplo_cuota", d->cumplio_cuota);
        cJSON_AddNumberToObject(o, "clientes_perdidos", d->clientes_perdidos);
        cJSON_AddNumberToObject(o, "ingresos", d->ingresos);
        cJSON_AddNumberToObject(o, "gastos", d->gastos);
        cJSON_AddNumberToObject(o, "saldo", d->saldo);
        cJSON_AddNumberToObject(o, "reputacion", d->reputacion);
        cJSON_AddNumberToObject(o, "espera_media_rep", d->espera_media_rep);
        cJSON_AddItemToArray(historial, o);
    }
    cJSON_AddItemToObject(raiz, "historial", historial);

    return raiz;
}
