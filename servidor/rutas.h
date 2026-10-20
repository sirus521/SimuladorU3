/* rutas.h — Rutas de la API del simulador.
 *
 * Contrato congelado en la fase F5 (ver notas/05-plan-de-trabajo.md).
 * Si se cambia una ruta o una clave, hay que actualizar también los tipos
 * de TypeScript en web/src/tipos.ts y esta nota.
 */
#ifndef SIM_RUTAS_H
#define SIM_RUTAS_H

#include <stddef.h>

/* Atiende una petición ya leída. Devuelve 1 si se respondió. */
int rutas_atender(int fd, const char *raiz_web, const char *metodo,
                  const char *ruta, const char *cuerpo, size_t longitud_cuerpo);

#endif /* SIM_RUTAS_H */
