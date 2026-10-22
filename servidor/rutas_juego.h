/* rutas_juego.h — API de partida.
 *
 * Rutas:
 *   POST /api/partida/iniciar   crea una partida y devuelve su estado
 *   POST /api/partida/accion    aplica una acción del jugador
 *   POST /api/partida/avanzar   avanza el reloj de la partida
 *   GET  /api/partida           estado actual
 *
 * El servidor guarda una sola partida activa (es un juego local de una
 * persona). Ver notas/04-parte-visual.md.
 */
#ifndef SIM_RUTAS_JUEGO_H
#define SIM_RUTAS_JUEGO_H

#include <stddef.h>

/* Atiende una petición de la API de partida. Devuelve 1 si se respondió. */
int rutas_juego_atender(int fd, const char *metodo, const char *ruta,
                        const char *cuerpo, size_t longitud_cuerpo);

#endif /* SIM_RUTAS_JUEGO_H */
