/* test_juego.c — Motor de partida.
 *
 * Criterios de aceptación de la fase G2:
 *   - la partida avanza y el reloj corre
 *   - el trabajo NO empieza solo: sin jugador ni NPCs, las colas crecen
 *   - con un NPC, el trabajo se hace solo
 *   - la economía no crea ni destroys dinero
 *   - la cuota y la reputación reaccionan
 *   - la partida es reproducible con la misma semilla
 */
#include "test_util.h"

#include "sim/config.h"
#include "sim/juego.h"

#include <string.h>

#include "cjson/cJSON.h"

/* --- Utilidades ------------------------------------------------------ */

/* Avanza la partida N segundos de tiempo real con la escala ×1.
 * Con la escala ×1, 1 segundo real = 1 minuto de juego, así que 1 minuto
 * real = 1 hora de juego. */
static void avanzar_segundos(juego_t *g, double segundos)
{
    /* 1 s real = 1 min de juego = 1/60 h */
    juego_avanzar(g, segundos / 60.0);
}

static void escenario_simple(config_t *cfg)
{
    config_valores_defecto(cfg);
    cfg->llegadas_por_dia = 18.0;
    cfg->bahias_diagnostico = 2;
    cfg->mecanicos = 3;
    cfg->refacciones_stock = 40;
    cfg->refacciones_lote = 40;
    cfg->refacciones_periodo_h = 48.0;
    cfg->hora_apertura = 9.0;
    cfg->hora_cierre = 18.0;
}

/* --- Pruebas --------------------------------------------------------- */

/* El reloj debe avanzar y las llegadas deben producirse. */
static void prueba_reloj_avanza(void)
{
    config_t cfg;
    juego_t g;

    escenario_simple(&cfg);
    juego_iniciar(&g, &cfg, 2026);

    AFIRMAR(g.t_actual > 0.0, "el reloj no arrancó");
    AFIRMAR(g.dia_actual == 1, "el día inicial no es 1");

    avanzar_segundos(&g, 30.0);
    AFIRMAR(g.t_actual > 9.0, "el reloj no avanzó");
    AFIRMAR(g.n_vehiculos > 0, "no llegó ningún cliente");
    AFIRMAR(fifo_pending(&g.cola[PUESTO_RECEPCION]) > 0,
            "el cliente no quedó en la cola de recepción");
}

/* Sin nadie que trabaje, las colas crecen: el trabajo no se hace solo. */
static void prueba_trabajo_no_automatico(void)
{
    config_t cfg;
    juego_t g;
    int antes;

    escenario_simple(&cfg);
    juego_iniciar(&g, &cfg, 2026);

    avanzar_segundos(&g, 5.0);
    antes = fifo_pending(&g.cola[PUESTO_RECEPCION]);
    AFIRMAR(antes > 0, "no hay clientes esperando");

    /* Sigue avanzando sin que nadie atienda: la cola no baja. */
    avanzar_segundos(&g, 5.0);
    AFIRMAR(fifo_pending(&g.cola[PUESTO_RECEPCION]) >= antes,
            "la cola bajó sin que nadie atendiera: el trabajo se hizo solo");

    /* Y nadie llegó a diagnóstico, porque nadie recibió. */
    AFIRMAR(fifo_pending(&g.cola[PUESTO_DIAGNOSTICO]) == 0,
            "hubo vehículos en diagnóstico sin pasar por recepción");
}

/* Con un recepcionista, el trabajo avanza solo. */
static void prueba_npc_trabaja(void)
{
    config_t cfg;
    juego_t g;
    int i;
    int avanzo = 0;

    escenario_simple(&cfg);
    juego_iniciar(&g, &cfg, 2026);

    /* Contrata un recepcionista. */
    AFIRMAR(juego_accion_contratar(&g, EMPLEADO_RECEPCIONISTA) == 1,
            "no se pudo contratar");
    AFIRMAR(g.n_empleados == 1, "no se registró el empleado");

    for (i = 0; i < 40; i++) {
        int antes = fifo_pending(&g.cola[PUESTO_RECEPCION]);
        avanzar_segundos(&g, 3.0);
        if (fifo_pending(&g.cola[PUESTO_RECEPCION]) < antes) avanzo++;
    }
    AFIRMAR(avanzo > 0, "el NPC nunca atendió a nadie");
}

/* El jugador puede atender un puesto concreto. */
static void prueba_accion_jugador(void)
{
    config_t cfg;
    juego_t g;

    escenario_simple(&cfg);
    juego_iniciar(&g, &cfg, 2026);

    avanzar_segundos(&g, 5.0);
    AFIRMAR(juego_hay_trabajo(&g, PUESTO_RECEPCION),
            "debería haber trabajo en recepción");

    AFIRMAR(juego_accion_atender(&g, PUESTO_RECEPCION) == 1,
            "no se pudo atender recepción");
    AFIRMAR(g.jugador_vehiculo >= 0, "no se registró el vehículo del jugador");
    AFIRMAR(g.jugador_puesto == PUESTO_RECEPCION, "el puesto no se registró");

    /* Mientras el jugador está ocupado, no puede atender otro puesto. */
    AFIRMAR(juego_accion_atender(&g, PUESTO_RECEPCION) == 0,
            "el jugador pudo tomar dos trabajos a la vez");

    /* Tras el trabajo, se libera. */
    avanzar_segundos(&g, 20.0);
    AFIRMAR(g.jugador_vehiculo == -1, "el jugador no liberó el vehículo");
    AFIRMAR(g.jugador_puesto == -1, "el jugador no liberó el puesto");
}

/* Atender un puesto vacío no hace nada. */
static void prueba_accion_invalida(void)
{
    config_t cfg;
    juego_t g;

    escenario_simple(&cfg);
    juego_iniciar(&g, &cfg, 2026);

    AFIRMAR(juego_accion_atender(&g, PUESTO_DIAGNOSTICO) == 0,
            "se aceptó un puesto sin trabajo");
    AFIRMAR(juego_accion_atender(&g, 99) == 0,
            "se aceptó un puesto inexistente");
    AFIRMAR(juego_accion_atender(&g, -1) == 0,
            "se aceptó un puesto negativo");
}

/* El jugador puede cotizar y el precio lo respeta el cliente.
 *
 * El flujo real del juego es: recepción → diagnóstico → cotización. Cada
 * puesto lo tiene que atender el jugador, así que el test recorre los tres. */
static void prueba_cotizacion(void)
{
    config_t cfg;
    juego_t g;
    int v;

    escenario_simple(&cfg);
    juego_iniciar(&g, &cfg, 2026);

    /* Recibe al cliente. */
    AFIRMAR(juego_accion_atender(&g, PUESTO_RECEPCION) == 1, "no se pudo atender");
    avanzar_segundos(&g, 10.0);
    AFIRMAR(fifo_pending(&g.cola[PUESTO_DIAGNOSTICO]) > 0,
            "el cliente no llegó a diagnóstico");

    /* Lo diagnostica. */
    AFIRMAR(juego_accion_atender(&g, PUESTO_DIAGNOSTICO) == 1,
            "no se pudo diagnosticar");
    avanzar_segundos(&g, 120.0);
    AFIRMAR(fifo_pending(&g.cola[PUESTO_COTIZACION]) > 0,
            "el cliente no llegó a cotización");

    /* Lo cotiza a un precio altísimo: el cliente debe rechazar. */
    AFIRMAR(juego_accion_atender(&g, PUESTO_COTIZACION) == 1,
            "no se pudo atender la cotización");
    v = g.jugador_vehiculo;
    AFIRMAR(v >= 0, "no hay vehículo en cotización");
    AFIRMAR(juego_accion_cotizar(&g, 999999) == 1, "no se pudo cotizar");
    AFIRMAR(g.vehiculos[v].precio_ofrecido == 999999, "no se guardó el precio");

    /* Tras un momento, el cliente decide y rechaza. */
    avanzar_segundos(&g, 30.0);
    AFIRMAR(g.vehiculos[v].estado == V_RECHAZADO,
            "el cliente aceptó un precio absurdo");
    AFIRMAR(g.perdidos_dia >= 1, "no se contó el cliente perdido");
}

/* Un precio razonable sí se acepta y el cliente avanza. */
static void prueba_cotizacion_razonable(void)
{
    config_t cfg;
    juego_t g;
    int v;

    escenario_simple(&cfg);
    juego_iniciar(&g, &cfg, 2026);

    juego_accion_atender(&g, PUESTO_RECEPCION);
    avanzar_segundos(&g, 10.0);
    juego_accion_atender(&g, PUESTO_DIAGNOSTICO);
    avanzar_segundos(&g, 120.0);
    AFIRMAR(juego_accion_atender(&g, PUESTO_COTIZACION) == 1, "no se pudo cotizar");
    v = g.jugador_vehiculo;

    /* Cotiza justo por debajo de la disposición del cliente. */
    AFIRMAR(juego_accion_cotizar(&g, (int)(g.vehiculos[v].disposicion * 0.9)) == 1,
            "no se pudo cotizar");

    avanzar_segundos(&g, 30.0);
    AFIRMAR(g.vehiculos[v].estado == V_ESPERA_ALMACEN,
            "el cliente no avanzó a almacén con un precio razonable");
}

/* La economía: el saldo solo baja por gastos y sube por ingresos. */
static void prueba_economia(void)
{
    config_t cfg;
    juego_t g;
    int saldo_inicial;

    escenario_simple(&cfg);
    juego_iniciar(&g, &cfg, 2026);
    saldo_inicial = g.saldo;

    /* Contratar un empleado cuesta nómina al cerrar el día. */
    AFIRMAR(juego_accion_contratar(&g, EMPLEADO_MECANICO) == 1, "no se pudo contratar");
    AFIRMAR(g.saldo == saldo_inicial,
            "contratar quitó dinero del saldo al instante (debería ser diario)");

    /* Pedir un lote sí cuesta dinero ya. */
    AFIRMAR(juego_accion_pedir_lote(&g) == 1, "no se pudo pedir el lote");
    AFIRMAR(g.saldo == saldo_inicial - 1500, "el lote no costó lo esperado");
    AFIRMAR(g.lote_en_camino == 20, "no se registró el lote en camino");

    /* No se puede pedir un segundo lote mientras hay uno en camino. */
    AFIRMAR(juego_accion_pedir_lote(&g) == 0, "se permitió dos lotes a la vez");
}

/* Sin dinero no se puede contratar. */
static void prueba_sin_dinero_no_contrata(void)
{
    config_t cfg;
    juego_t g;

    escenario_simple(&cfg);
    juego_iniciar(&g, &cfg, 2026);
    g.saldo = 100;

    AFIRMAR(juego_accion_contratar(&g, EMPLEADO_MECANICO) == 0,
            "se contrató sin dinero");
}

/* La partida termina y se registra el motivo. */
static void prueba_fin_de_partida(void)
{
    config_t cfg;
    juego_t g;
    int i;

    escenario_simple(&cfg);
    juego_iniciar(&g, &cfg, 2026);

    /* Se agota el mes: 30 días × 24 h. */
    for (i = 0; i < 200 && !g.terminada; i++) {
        avanzar_segundos(&g, 60.0);
    }
    AFIRMAR(g.terminada, "la partida no terminó");
    AFIRMAR(g.motivo[0] != '\0', "no se registró el motivo");
    AFIRMAR(g.n_historial > 0, "no se guardó historial");
}

/* La misma semilla da la misma partida. */
static void prueba_reproducible(void)
{
    config_t cfg;
    juego_t a, b;
    int i;

    escenario_simple(&cfg);
    juego_iniciar(&a, &cfg, 2026);
    juego_iniciar(&b, &cfg, 2026);

    for (i = 0; i < 30; i++) {
        avanzar_segundos(&a, 5.0);
        avanzar_segundos(&b, 5.0);
    }
    AFIRMAR(a.n_vehiculos == b.n_vehiculos, "llegaron clientes distintos");
    AFIRMAR(a.t_actual == b.t_actual, "los relojes difieren");
    AFIRMAR(fifo_pending(&a.cola[PUESTO_RECEPCION]) ==
            fifo_pending(&b.cola[PUESTO_RECEPCION]), "las colas difieren");
}

/* Semillas distintas dan partidas distintas. */
static void prueba_semillas_distintas(void)
{
    config_t cfg;
    juego_t a, b;
    int i;

    escenario_simple(&cfg);
    juego_iniciar(&a, &cfg, 1);
    juego_iniciar(&b, &cfg, 2);

    for (i = 0; i < 30; i++) {
        avanzar_segundos(&a, 5.0);
        avanzar_segundos(&b, 5.0);
    }
    AFIRMAR(a.n_vehiculos != b.n_vehiculos ||
            fifo_pending(&a.cola[PUESTO_RECEPCION]) !=
            fifo_pending(&b.cola[PUESTO_RECEPCION]),
            "dos semillas dieron la misma partida");
}

/* El consumo de muestras del PRNG no se dispara. */
static void prueba_prng_sano(void)
{
    config_t cfg;
    juego_t g;
    int i;

    escenario_simple(&cfg);
    juego_iniciar(&g, &cfg, 2026);
    for (i = 0; i < 400; i++) {
        avanzar_segundos(&g, 10.0);
    }
    AFIRMAR(g.muestras_prng < 65536u / 4u,
            "la partida consumió demasiadas muestras del PRNG");
}

/* El snapshot JSON no puede estar roto. */
static void prueba_snapshot(void)
{
    config_t cfg;
    juego_t g;
    cJSON *json;

    escenario_simple(&cfg);
    juego_iniciar(&g, &cfg, 2026);
    avanzar_segundos(&g, 5.0);

    json = juego_a_json(&g);
    AFIRMAR(json != NULL, "el snapshot es NULL");
    if (json != NULL) {
        AFIRMAR(cJSON_GetObjectItemCaseSensitive(json, "saldo") != NULL,
                "falta 'saldo' en el snapshot");
        AFIRMAR(cJSON_GetObjectItemCaseSensitive(json, "colas") != NULL,
                "faltan las colas en el snapshot");
        AFIRMAR(cJSON_GetObjectItemCaseSensitive(json, "vehiculos") != NULL,
                "faltan los vehículos en el snapshot");
        cJSON_Delete(json);
    }
}

int main(void)
{
    printf("  Motor de partida\n");
    prueba_reloj_avanza();
    prueba_trabajo_no_automatico();
    prueba_npc_trabaja();
    prueba_accion_jugador();
    prueba_accion_invalida();
    prueba_cotizacion();
    prueba_cotizacion_razonable();
    prueba_economia();
    prueba_sin_dinero_no_contrata();
    prueba_fin_de_partida();
    prueba_reproducible();
    prueba_semillas_distintas();
    prueba_prng_sano();
    prueba_snapshot();
    return test_resumen("juego");
}
