/* config.h — Configuración de un escenario del simulador.
 *
 * Un escenario es un conjunto de parámetros: tamaño del taller, demanda,
 * reglas de presupuesto y política de refacciones. Ver notas/03-modelo-matematico.md.
 */
#ifndef SIM_CONFIG_H
#define SIM_CONFIG_H

#include <stddef.h>
#include <stdint.h>

#define SIM_MAX_TIPOS 4
#define SIM_NOMBRE_LEN 64

typedef struct {
    char     nombre[SIM_NOMBRE_LEN];
    uint32_t semilla;
    int      dias;                   /* horizonte en días                         */
    int      dias_calentamiento;     /* días iniciales que se descartan          */
    int      replicas;               /* réplicas independientes                   */
    double   llegadas_por_dia;       /* demanda media dentro del horario          */
    int      bahias_diagnostico;     /* bahías de diagnóstico (c)                 */
    int      mecanicos;              /* mecánicos de reparación (c)              */
    int      refacciones_stock;      /* stock inicial = capacidad máxima          */
    int      refacciones_lote;       /* unidades que repone cada reposición       */
    double   refacciones_periodo_h;  /* horas entre reposiciones                  */
    double   presupuesto_aceptacion; /* probabilidad de aceptar (0..1)            */
    double   presupuesto_decision_h; /* media de la espera de decisión            */
    double   hora_apertura;         /* hora de apertura (0..24)                  */
    double   hora_cierre;            /* hora de cierre (0..24)                    */
    int      n_tipos;                /* tipos de avería                            */
    double   peso[SIM_MAX_TIPOS];         /* probabilidad de cada tipo           */
    double   media_diagnostico_h[SIM_MAX_TIPOS];
    double   media_reparacion_h[SIM_MAX_TIPOS];
    int      refacciones[SIM_MAX_TIPOS];  /* unidades de refacción por trabajo */
} config_t;

/* Valores por defecto del escenario base (ver notas/02-modelo-del-negocio.md). */
void config_valores_defecto(config_t *cfg);

/* Valida los parámetros. Devuelve 0 si todo bien, o un código de error:
 *   1 llegadas <= 0        2 bahias < 1        3 mecánicos < 1
 *   4 refacciones < 0      5 aceptacion fuera de [0,1]
 *   6 días < 1            7 réplicas < 1      8 pesos no suman 1
 *   9 tipos < 1          10 horario inválido 11 stock/lote inconsistentes
 * La función `config_describir_error` devuelve el texto en español. */
int config_validar(const config_t *cfg);
const char *config_describir_error(int codigo);

/* Carga un escenario desde un archivo JSON. Devuelve 0 si salió bien. */
int config_desde_json_archivo(const char *ruta, config_t *cfg, char *error, size_t tam_error);

/* Construye la configuración desde un objeto cJSON ya parseado.
 * Devuelve 0 si salió bien. */
struct cJSON;
int config_desde_json(const struct cJSON *obj, config_t *cfg, char *error, size_t tam_error);

/* Serializa la configuración a JSON (para la API y para guardar escenarios). */
struct cJSON *config_a_json(const config_t *cfg);

#endif /* SIM_CONFIG_H */
