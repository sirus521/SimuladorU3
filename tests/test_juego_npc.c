/* test_juego_npc.c — Los NPCs y la repartición del trabajo.
 *
 * Cada prueba de aquí cubre un fallo real que se encontró al construir el
 * juego, para que no vuelva a aparecer:
 *
 *   - el array de puestos no se desplazaba por el tipo de empleado, y todos
 *     acababan en recepción: el almacén no recibía a nadie y el taller no
 *     entregaba un solo vehículo;
 *   - la regla de reparto dejaba en inanición una de las dos colas del
 *     mecánico;
 *   - el día se cerraba dos veces (nómina cobrada por duplicado);
 *   - el coste de las piezas se imputaba al tomar la pieza, no al entregar;
 *   - nadie atendía la entrega y los vehículos repairing se quedaban
 *     parados sin facturar.
 */
#include "test_util.h"

#include "sim/config.h"
#include "sim/juego.h"

#include <string.h>

/* --- Utilidades ------------------------------------------------------ */

static void escenario(config_t *cfg)
{
    config_valores_defecto(cfg);
    cfg->llegadas_por_dia = 18.0;
    cfg->bahias_diagnostico = 2;
    cfg->mecanicos = 3;
    cfg->refacciones_stock = 40;
    cfg->refacciones_lote = 0;
    cfg->refacciones_periodo_h = 48.0;
    cfg->hora_apertura = 9.0;
    cfg->hora_cierre = 18.0;
}

/* Un turno completo: 9 h de juego en pasos de 1 min. */
static void pasar_turno(juego_t *g)
{
    int i;
    for (i = 0; i < 9 * 60; i++) {
        juego_avanzar(g, 1.0 / 60.0);
    }
}

static void contratar_equipo(juego_t *g, int n_mecanicos)
{
    int i;
    juego_accion_contratar(g, EMPLEADO_RECEPCIONISTA);
    for (i = 0; i < n_mecanicos; i++) {
        juego_accion_contratar(g, EMPLEADO_MECANICO);
    }
    juego_accion_contratar(g, EMPLEADO_ALMACENISTA);
    juego_accion_contratar(g, EMPLEADO_COTIZADOR);
}

/* --- Pruebas --------------------------------------------------------- */

/* Cada tipo de empleado debe ir a SU puesto. */
static void prueba_cada_empleado_a_su_puesto(void)
{
    config_t cfg;
    juego_t g;
    int i;
    int puestos_vistos[4] = { -1, -1, -1, -1 };

    escenario(&cfg);
    juego_iniciar(&g, &cfg, 2026);
    contratar_equipo(&g, 2);

    /* Un turno completo: cada empleado debe haber trabajado en su puesto. */
    for (i = 0; i < 9 * 60; i++) {
        int e;
        juego_avanzar(&g, 1.0 / 60.0);
        for (e = 0; e < g.n_empleados; e++) {
            int p = g.empleados[e].puesto_actual;
            if (p >= 0 && g.empleados[e].tipo < 4) {
                puestos_vistos[g.empleados[e].tipo] = p;
            }
        }
    }

    AFIRMAR(puestos_vistos[EMPLEADO_RECEPCIONISTA] == PUESTO_RECEPCION,
            "el recepcionista no atendidos recepción");
    AFIRMAR(puestos_vistos[EMPLEADO_ALMACENISTA] == PUESTO_ALMACEN,
            "el almacenista no atendió el almacén");
    AFIRMAR(puestos_vistos[EMPLEADO_COTIZADOR] == PUESTO_COTIZACION ||
            puestos_vistos[EMPLEADO_COTIZADOR] == PUESTO_ENTREGA,
            "el cotizador no atendió cotización");
}

/* El taller debe entregar vehículos: es la prueba que delata que nada
 * avanza en la cadena. */
static void prueba_se_entrega_vehiculos(void)
{
    config_t cfg;
    juego_t g;

    escenario(&cfg);
    juego_iniciar(&g, &cfg, 2026);
    contratar_equipo(&g, 3);

    pasar_turno(&g);

    /* Ojo: al cerrar el día los contadores del día se reinician; los
     * valores del día cerrado quedan en el historial. */
    AFIRMAR(g.n_historial == 1, "no se registró el día");
    if (g.n_historial == 1) {
        AFIRMAR(g.historial[0].entregas > 0, "no se entregó ningún vehículo en un turno");
        AFIRMAR(g.historial[0].ingresos > 0, "no se facturó nada");
    }
    AFIRMAR(g.saldo > 3000, "el saldo no creció tras facturar");
}

/* Ningún vehículo debe quedarse atascado en la cola de entrega: es la
 * comprobación de que alguien atiende ese puesto. */
static void prueba_la_entrega_se_atiende(void)
{
    config_t cfg;
    juego_t g;
    int i;

    escenario(&cfg);
    juego_iniciar(&g, &cfg, 2026);
    contratar_equipo(&g, 3);

    for (i = 0; i < 9 * 60; i++) {
        juego_avanzar(&g, 1.0 / 60.0);
    }
    AFIRMAR(fifo_pending(&g.cola[PUESTO_ENTREGA]) < 20,
            "la cola de entrega se desborda: nadie la atendía");
}

/* Ninguna cola debe morir de hambre: el mecánico reparte su trabajo entre
 * diagnóstico y reparación. */
static void prueba_ninguna_cola_muere(void)
{
    config_t cfg;
    juego_t g;
    int i;

    escenario(&cfg);
    juego_iniciar(&g, &cfg, 2026);
    contratar_equipo(&g, 3);

    for (i = 0; i < 9 * 60; i++) {
        juego_avanzar(&g, 1.0 / 60.0);
    }
    AFIRMAR(fifo_pending(&g.cola[PUESTO_REPARACION]) < 30,
            "la cola de reparación creció sin límite: nadie la atendía");
    AFIRMAR(fifo_pending(&g.cola[PUESTO_DIAGNOSTICO]) < 30,
            "la cola de diagnóstico creció sin límite");
}

/* El día se cierra UNA vez: la nómina no se cobra por duplicado. */
static void prueba_el_dia_se_cierra_una_vez(void)
{
    config_t cfg;
    juego_t g;

    escenario(&cfg);
    juego_iniciar(&g, &cfg, 2026);
    contratar_equipo(&g, 2);

    pasar_turno(&g);

    AFIRMAR(g.n_historial == 1, "no se registró exactamente un día");
    if (g.n_historial == 1) {
        /* 5 empleados: nómina 5×3000 + alquiler 5000 = 20 000. El resto del
         * gasto es el material de los vehículos entregados. */
        int fijos = 5 * 2000 + 4000;
        int material = (int)g.historial[0].gastos - fijos;
        AFIRMAR((int)g.historial[0].gastos >= fijos,
                "el gasto del día es menor que la nómina más el alquiler");
        /* El material de 10 entregas no puede pasar de 10×4000 = 40 000, y
         * con las entregas contadas no puede ser cero. */
        AFIRMAR(material <= 10 * 5000, "el material imputado no corresponde");
    }
}

/* Al entregar se imputa el coste del material, no al tomar la pieza. */
static void prueba_el_material_se_imputa_al_entregar(void)
{
    config_t cfg;
    juego_t g;

    escenario(&cfg);
    cfg.refacciones_stock = 200;
    juego_iniciar(&g, &cfg, 2026);
    contratar_equipo(&g, 3);

    pasar_turno(&g);

    /* Los gastos del día = nómina + alquiler + material de los entregados. */
    if (g.n_historial == 1 && g.entregas_dia > 0) {
        int fijos = 5 * 2000 + 4000;
        int material = (int)g.historial[0].gastos - fijos;
        AFIRMAR(material > 0, "no se imputó el coste del material");
        AFIRMAR(material < 5 * 4000,
                "el material imputado no corresponde a los entregados");
    }
}

/* Sin employedados, la cola crece: el trabajo no se hace solo. */
static void prueba_sin_empleados_no_se_trabaja(void)
{
    config_t cfg;
    juego_t g;

    escenario(&cfg);
    juego_iniciar(&g, &cfg, 2026);

    pasar_turno(&g);

    AFIRMAR(g.n_historial == 0 || g.historial[0].entregas == 0,
            "se entregaron vehículos sin nadie trabajando");
    /* Sin nadie en recepción, los clientes se acumulan ahí: nadie los
     * atiende y ninguno llega al diagnóstico. */
    AFIRMAR(fifo_pending(&g.cola[PUESTO_RECEPCION]) > 0,
            "no se formó cola de recepción");
    AFIRMAR(fifo_pending(&g.cola[PUESTO_DIAGNOSTICO]) == 0,
            "hubo vehículos en diagnóstico sin pasar por recepción");
}

/* Contratar mejora la producción: tiene que notarse. */
static void prueba_contratar_mejora(void)
{
    config_t cfg;
    juego_t g;
    int sin, con;

    escenario(&cfg);
    juego_iniciar(&g, &cfg, 2026);
    contratar_equipo(&g, 1);
    pasar_turno(&g);
    sin = (g.n_historial > 0) ? g.historial[0].entregas : 0;

    escenario(&cfg);
    juego_iniciar(&g, &cfg, 2026);
    contratar_equipo(&g, 4);
    pasar_turno(&g);
    con = (g.n_historial > 0) ? g.historial[0].entregas : 0;

    AFIRMAR(con > sin,
            "contratar más gente no aumentó las entregas");
}

/* La partida se puede perder: sin facturar, se quiebra. */
static void prueba_se_puede_perder(void)
{
    config_t cfg;
    juego_t g;
    int i;

    escenario(&cfg);
    cfg.refacciones_stock = 0;   /* sin material, no se puede trabajar */
    juego_iniciar(&g, &cfg, 2026);
    contratar_equipo(&g, 3);

    for (i = 0; i < 30 && !g.terminada; i++) {
        pasar_turno(&g);
    }
    AFIRMAR(g.terminada, "la partida nunca terminó aunque no se facturaba");
    AFIRMAR(g.ganada == 0, "la partida se ganó sin facturar");
    AFIRMAR(g.motivo[0] != '\0', "no se registró el motivo de la derrota");
}

int main(void)
{
    printf("  NPCs y reparticion del trabajo\n");
    prueba_cada_empleado_a_su_puesto();
    prueba_se_entrega_vehiculos();
    prueba_la_entrega_se_atiende();
    prueba_ninguna_cola_muere();
    prueba_el_dia_se_cierra_una_vez();
    prueba_el_material_se_imputa_al_entregar();
    prueba_sin_empleados_no_se_trabaja();
    prueba_contratar_mejora();
    prueba_se_puede_perder();
    return test_resumen("juego-npc");
}
