/* prng.h — Generador de números pseudoaleatorios propio.
 *
 * Implementación del LCG de 16 bits documentado en notas/09-prng-generador.md:
 *
 *   x(n+1) = (25205·x(n) + 12345) mod 65536
 *
 * con una etapa de mezcla xorshift biyectiva aplicada SOLO a la salida.
 * Periodo máximo 65536 (Hull-Dobell verificado en la nota).
 *
 * El generador no guarda historia: cada llamada avanza el estado una vez.
 */
#ifndef SIM_PRNG_H
#define SIM_PRNG_H

#include <stdint.h>

typedef struct {
    uint16_t estado;    /* x actual, siempre en [0, 65535]            */
    uint32_t muestras;  /* cuántas salidas se han pedido                */
    uint32_t rechazos;  /* cuántos rechazos ha hecho el mapeo del dado   */
} prng_t;

/* Inicializa con la semilla. La semilla 0 es válida (permite periodo
 * completo); la convención del proyecto es usar semillas en [1, 65535]. */
void prng_iniciar(prng_t *p, uint16_t semilla);

/* Avanza el estado una vez y devuelve la salida mezclada z.
 * Este es el único generador de la secuencia: todo lo demás se apoya aquí. */
uint16_t prng_azar(prng_t *p);

/* Estado interno actual (el último x avanzado), sin avanzar. */
uint16_t prng_estado(const prng_t *p);

/* Dado de 6 caras uniforme por el método de multiplicación con hueco.
 * Devuelve 1..6. Rechaza internamente las 4 salidas del hueco. */
unsigned prng_dado(prng_t *p);

/* Aplica el mapeo de dado a una salida ya generada.
 * Devuelve 1..6, o 0 si la salida cae en el hueco y debe rechazarse.
 * Se expone para poder probar el mapeo sin avanzar el generador. */
int prng_dado_de_salida(uint16_t z);

/* Uniforme en [0,1): u = (z + 0.5) / 65536.
 * El +0.5 evita que u sea exactamente 0. */
double prng_uniforme(prng_t *p);

#endif /* SIM_PRNG_H */
