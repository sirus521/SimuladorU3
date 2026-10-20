/* modelo.h — Motor de simulación de eventos discretos del taller.
 *
 * Implementa el proceso descrito en notas/02-modelo-del-negocio.md.
 * El motor no hace I/O: recibe una config_t y devuelve una
 * resultado_replica_t. Eso lo hace usable desde el CLI y desde el servidor.
 */
#ifndef SIM_MODELO_H
#define SIM_MODELO_H

#include "sim/config.h"
#include "sim/resultado.h"

/* Corre una réplica completa con la semilla dada. Devuelve 0 si todo bien. */
int modelo_correr(const config_t *cfg, uint32_t semilla, resultado_replica_t *out);

#endif /* SIM_MODELO_H */
