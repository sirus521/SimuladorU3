/* juego.h — Motor de partida: el taller como videojuego.
 *
 * Es la capa que convierte el modelo de eventos discretos en algo jugable.
 * La diferencia esencial con el motor de análisis (`sim/modelo.h`) es que
 * aquí **el trabajo lo inicia alguien**: el jugador o un empleado. El motor
 * de análisis deja que las bahías se ocupen solas; aquí no.
 *
 * Ver notas/03-modelo-matematico.md § 4.1.
 */
#ifndef SIM_JUEGO_H
#define SIM_JUEGO_H

#include "sim/colas.h"
#include "sim/config.h"
#include "sim/prng.h"
#include "sim/resultado.h"

#define JUEGO_MAX_VEHICULOS 512
#define JUEGO_MAX_EMPLEADOS 24
#define JUEGO_MAX_PUESTOS   6
#define JUEGO_MAX_TIPOS     4
#define JUEGO_HISTORIAL_DIAS 64

/* Los seis puestos del taller. El orden importa: es el orden del proceso. */
typedef enum {
    PUESTO_RECEPCION = 0,
    PUESTO_DIAGNOSTICO,
    PUESTO_COTIZACION,
    PUESTO_ALMACEN,
    PUESTO_REPARACION,
    PUESTO_ENTREGA,
    JUEGO_N_PUESTOS
} puesto_t;

/* Estado de un vehículo dentro de la partida. */
typedef enum {
    V_ESPERA_RECEPCION = 0,
    V_EN_RECEPCION,
    V_ESPERA_DIAGNOSTICO,
    V_EN_DIAGNOSTICO,
    V_ESPERA_COTIZACION,
    V_EN_COTIZACION,
    V_ESPERA_ALMACEN,
    V_EN_ALMACEN,
    V_ESPERA_REPARACION,
    V_EN_REPARACION,
    V_ESPERA_ENTREGA,
    V_EN_ENTREGA,
    V_ENTREGADO,
    V_RECHAZADO,
    V_IDO
} estado_vehiculo_juego_t;

/* Puesto que ocupa un empleado. */
typedef enum {
    EMPLEADO_RECEPCIONISTA = 0,
    EMPLEADO_MECANICO,
    EMPLEADO_ALMACENISTA,
    EMPLEADO_COTIZADOR,
    EMPLEADO_N_TIPOS
} tipo_empleado_t;

typedef struct {
    int    id;
    int    tipo;              /* tipo_empleado_t */
    int    puesto_actual;     /* puesto_t, o -1 si está libre */
    double t_libre_desde;     /* cuándo quedó disponible */
    int    ocupado;           /* 1 si está trabajando */
    int    vehiculo;          /* índice del vehículo que atiende, o -1 */
    int    dias_trabajados;
} empleado_t;

typedef struct {
    estado_vehiculo_juego_t estado;
    int    tipo;              /* tipo de avería */
    double t_llegada;
    double t_entrada_sistema;
    double t_salida;
    double t_inicio_trabajo;  /* cuándo empezó el trabajo en curso */
    double t_inicio_espera;   /* cuándo empezó a esperar en la cola actual */
    double disposicion;       /* disposición a pagar del cliente */
    int    precio_ofrecido;   /* lo que se le cotizó */
    int    piezas_gastadas;
    int    espera_cotizar;    /* 1 si está esperando decisión del cliente */
    int    motivo_salida;     /* 0 = entregado, 1 = rechazado, 2 = se cansó */
} vehiculo_juego_t;

/* Un día de la partida, para el historial. */
typedef struct {
    int    dia;
    int    entregas;
    int    cuota;
    int    cumplio_cuota;
    int    clientes_perdidos;
    double ingresos;
    double gastos;
    double saldo;
    int    reputacion;
    double espera_media_rep;
} dia_juego_t;

/* La partida completa. */
typedef struct {
    config_t     cfg;
    prng_t       rng;
    uint32_t     semilla;

    /* Reloj */
    double       t_actual;      /* horas desde el inicio de la partida */
    int          dia_actual;    /* 1..N */
    int          pausado;
    int          velocidad;     /* 1, 2, 4 */

    /* Puestos */
    int          capacidad_bahias_diag;
    int          capacidad_bahias_rep;
    int          en_uso_diag;
    int          en_uso_rep;

    /* Vehículos */
    vehiculo_juego_t vehiculos[JUEGO_MAX_VEHICULOS];
    int          n_vehiculos;
    int          cap_vehiculos;

    /* Colas por puesto */
    fifo_t       cola[JUEGO_N_PUESTOS];

    /* Empleados */
    empleado_t   empleados[JUEGO_MAX_EMPLEADOS];
    int          n_empleados;

    /* Economía */
    int          saldo;
    int          ingresos_dia;
    int          gastos_dia;
    int          ingresos_totales;
    int          gastos_totales;
    int          reputacion;
    int          cuota_diaria;
    int          dias_sin_cuota;

    /* Almacén */
    int          stock_refacciones;
    int          lote_en_camino;
    double       t_lote_llega;

    /* Cola de eventos de la partida */
    cola_t       eventos;

    /* Puesto donde está el jugador, o -1 */
    int          jugador_puesto;
    int          jugador_vehiculo;   /* vehículo que atiende el jugador, o -1 */
    double       jugador_t_inicio;

    /* Contadores del día en curso */
    int          entregas_dia;
    int          perdidos_dia;
    int          ingresos_dia_int;
    int          gastos_dia_int;

    /* Historial */
    dia_juego_t  historial[JUEGO_HISTORIAL_DIAS];
    int          n_historial;

    /* Estado de la partida */
    int          terminada;      /* 1 = terminada */
    int          ganada;         /* 1 = victoria */
    char         motivo[64];     /* por qué terminó */

    /* Acumuladores de espera (para el parte) */
    double       espera_rep_suma;
    int          espera_rep_n;

    uint32_t     muestras_prng;
} juego_t;
/* --- Ciclo de vida ------------------------------------------------- */

/* Crea una partida nueva. Devuelve 0 si todo bien. */
int juego_iniciar(juego_t *g, const config_t *cfg, uint32_t semilla);

/* Avanza la simulación hasta el instante `dt` horas más adelante.
 * El servidor la llama a ritmo fijo (20 Hz) con el tiempo real transcurrido
 * escalada. */
void juego_avanzar(juego_t *g, double dt_horas);

/* --- Acciones del jugador ------------------------------------------- */

/* Atender el puesto indicado (el cliente dice en cuál está).
 * `puesto` es un puesto_t. Devuelve 1 si la acción se aceptó. */
int juego_accion_atender(juego_t *g, int puesto);

/* Fija el precio del presupuesto del vehicle que el jugador tiene delante. */
int juego_accion_cotizar(juego_t *g, int precio);

/* Pide un lote de refacciones. */
int juego_accion_pedir_lote(juego_t *g);

/* Pide un lote urgente: llega en una hora, pero cuesta el doble. */
int juego_accion_pedir_lote_ex(juego_t *g, int urgente);

/* Contrata un empleado del tipo dado. */
int juego_accion_contratar(juego_t *g, int tipo_empleado);

/* --- Consulta ------------------------------------------------------- */

/* ¿Hay algo que hacer en el puesto donde está el jugador? */
int juego_hay_accion(const juego_t *g);

/* Puesto que el jugador puede atender, o -1. */
int juego_puesto_disponible(const juego_t *g);

/* ¿Hay trabajo esperando en ese puesto? */
int juego_hay_trabajo(const juego_t *g, int puesto);

/* Estado de un vehículo, para la ficha. */
const vehiculo_juego_t *juego_vehiculo(const juego_t *g, int indice);

/* Nombre legible de un puesto (para la interfaz y los avisos). */
const char *juego_nombre_puesto(int puesto);

/* Pide un snapshot en JSON del estado de la partida (para el cliente). */
struct cJSON *juego_a_json(const juego_t *g);

#endif /* SIM_JUEGO_H */
