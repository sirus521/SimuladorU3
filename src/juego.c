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

/* Puestos que puede ocupar un tipo de empleado. */
static const int PUESTOS_DE_EMPLEADO[EMPLEADO_N_TIPOS] = {
    PUESTO_RECEPCION,             /* recepcionista: recepción             */
    PUESTO_REPARACION,            /* mecánico: diagnóstico o reparación   */
    PUESTO_ALMACEN,               /* almacenista: almacén                 */
    PUESTO_COTIZACION             /* cotizador: cotización                */
};

/* Puestos que un mecánico puede tomar, en orden de preferencia. */
static const int PUESTOS_MECANICO[2] = { PUESTO_DIAGNOSTICO, PUESTO_REPARACION };

/* ------------------------------------------------------------------ */
/* Economía: valores por defecto                                        */
/* ------------------------------------------------------------------ */

#define PRECIO_BASE_TIPO   { 4000, 8500, 14000 }
#define COSTE_PIEZA_TIPO   { 1200, 2500, 4000 }
#define MEDIA_DIAG_TIPO    { 0.5, 0.8, 1.2 }
#define MEDIA_REP_TIPO     { 2.0, 3.5, 6.0 }

#define NOMINA_DIARIA        900
#define ALQUILER_DIARIO     2500
#define SALDO_INICIAL       8000
#define PRECIO_LOTE         1500
#define PIEZAS_POR_LOTE     10
#define PLAZO_LOTE_HORAS    24.0
#define REPUTACION_INICIAL  70
#define CUOTA_DIARIA        12
#define DIAS_PARTIDA        30
#define HORAS_ATENCION      9.0
#define HORA_APERTURA       9.0

/* Tolerancia del cliente sobre el precio justo. */
#define TOLERANCIA_PRECIO 0.25
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
            duracion = 0.1;
            veh->estado = V_EN_RECEPCION;
            break;
        case PUESTO_DIAGNOSTICO:
            duracion = media_diag_tipo(veh->tipo);
            veh->estado = V_EN_DIAGNOSTICO;
            break;
        case PUESTO_COTIZACION:
            duracion = 0.2;
            veh->estado = V_EN_COTIZACION;
            break;
        case PUESTO_ALMACEN:
            duracion = 0.1;
            veh->estado = V_EN_ALMACEN;
            break;
        case PUESTO_REPARACION:
            duracion = media_rep_tipo(veh->tipo);
            veh->estado = V_EN_REPARACION;
            break;
        case PUESTO_ENTREGA:
            duracion = 0.1;
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
    g->capacidad_bahias_rep = cfg->mecanicos;

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
    cola_push(&g->eventos, (double)DIAS_PARTIDA * 24.0, EV_FIN_PARTIDA, -1);

    return 0;
}

/* ------------------------------------------------------------------ */
/* Cierre del día                                                       */
/* ------------------------------------------------------------------ */

static void cerrar_dia(juego_t *g)
{
    dia_juego_t *d;
    int nomina = 0;
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

    /* Empieza el día siguiente. */
    g->dia_actual++;
    g->entregas_dia = 0;
    g->perdidos_dia = 0;
    g->ingresos_dia_int = 0;
    g->gastos_dia_int = 0;
    g->espera_rep_suma = 0.0;
    g->espera_rep_n = 0;

    /* Al abrir el día nuevo hay clientes esperando, y se agenda la
     * siguiente llegada. */
    {
        double inicio = (double)(g->dia_actual - 1) * 24.0 + HORA_APERTURA;
        generar_llegada(g);
        cola_push(&g->eventos, inicio, EV_LLEGADA, -1);
        cola_push(&g->eventos, inicio + HORAS_ATENCION, EV_FIN_DIA, -1);
    }
}

/* ------------------------------------------------------------------ */
/* NPCs: la IA que ocupa los puestos                                    */
/* ------------------------------------------------------------------ */

/* Elige el puesto que un empleado debería atender ahora, o -1. */
static int elegir_puesto_empleado(const juego_t *g, int tipo_empleado)
{
    int mejor = -1;
    int mejor_cola = 0;
    int i;

    if (tipo_empleado == EMPLEADO_MECANICO) {
        /* Un mecánico va primero a诊断 y luego a reparación. */
        for (i = 0; i < 2; i++) {
            int p = PUESTOS_MECANICO[i];
            int pendientes = fifo_pending(&g->cola[p]);
            if (pendientes > mejor_cola && !puesto_saturado(g, p)) {
                mejor_cola = pendientes;
                mejor = p;
            }
        }
        return mejor;
    }

    {
        int p = PUESTOS_DE_EMPLEADO[tipo_empleado];
        if (puesto_saturado(g, p)) return -1;
        return fifo_vacia(&g->cola[p]) ? -1 : p;
    }
}

/* Hace trabajar a los empleados: toman un puesto si hay cola. */
static void trabajar_empleados(juego_t *g)
{
    int i;
    for (i = 0; i < g->n_empleados; i++) {
        empleado_t *e = &g->empleados[i];
        int puesto;
        int v;

        if (e->ocupado) continue;

        puesto = elegir_puesto_empleado(g, e->tipo);
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
            /* El NPC cotiza al precio justo y el cliente decide en breve. */
            g->vehiculos[v].precio_ofrecido = precio_tipo(g->vehiculos[v].tipo);
            g->vehiculos[v].espera_cotizar = 1;
            cola_push(&g->eventos, g->t_actual + 0.2, EV_DECISION_CLIENTE, v);
            e->ocupado = 0;
            e->vehiculo = -1;
            e->puesto_actual = -1;
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
        /* El precio lo pone el jugador (o el NPC, si atiende él). */
        if (g->vehiculos[v].precio_ofrecido <= 0) {
            g->vehiculos[v].precio_ofrecido = precio_tipo(g->vehiculos[v].tipo);
        }
        g->vehiculos[v].espera_cotizar = 1;
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
            g->reputacion -= 5;
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
            g->gastos_dia_int += coste_pieza(g->vehiculos[v].tipo);
            encolar(g, v, PUESTO_REPARACION, V_ESPERA_REPARACION);
        } else {
            /* No hay piezas: vuelve a esperar en almacén. */
            g->vehiculos[v].estado = V_ESPERA_ALMACEN;
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
        g->entregas_dia++;
        g->reputacion += 2;
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

    while (!g->terminada && cola_pop(&g->eventos, &ev)) {
        if (ev.t > objetivo) {
            /* El evento es futuro: se vuelve a encolar y se sale. */
            cola_push(&g->eventos, ev.t, ev.tipo, ev.indice);
            break;
        }
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

int juego_accion_pedir_lote(juego_t *g)
{
    if (g->saldo < PRECIO_LOTE) return 0;
    if (g->lote_en_camino > 0) return 0;   /* ya hay un lote en camino */
    g->saldo -= PRECIO_LOTE;
    g->gastos_dia_int += PRECIO_LOTE;
    g->lote_en_camino = PIEZAS_POR_LOTE;
    g->t_lote_llega = g->t_actual + PLAZO_LOTE_HORAS;
    cola_push(&g->eventos, g->t_lote_llega, EV_LOTE_PROVEEDOR, -1);
    return 1;
}

int juego_accion_contratar(juego_t *g, int tipo)
{
    empleado_t *e;
    if (tipo < 0 || tipo >= EMPLEADO_N_TIPOS) return 0;
    if (g->n_empleados >= JUEGO_MAX_EMPLEADOS) return 0;
    if (g->saldo < NOMINA_DIARIA) return 0;

    e = &g->empleados[g->n_empleados++];
    memset(e, 0, sizeof(*e));
    e->id = g->n_empleados;
    e->tipo = tipo;
    e->puesto_actual = -1;
    e->vehiculo = -1;
    e->t_libre_desde = g->t_actual;
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
