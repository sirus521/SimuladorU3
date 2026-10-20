/* distrib.h — Distribuciones de probabilidad sobre el PRNG propio.
 *
 * Todas se generan por el método de inversión a partir del uniforme de
 * prng_uniforme(), tal como describe notas/03-modelo-matematico.md.
 */
#ifndef SIM_DISTRIB_H
#define SIM_DISTRIB_H

#include "sim/prng.h"

/* Exponencial de media `media` (> 0). */
double dist_exponencial(prng_t *rng, double media);

/* Uniforme discreta en {0, 1, ..., n-1} usando probabilidades acumuladas.
 * `acum` debe tener n elementos y acabar en 1.0. */
int dist_categorica(prng_t *rng, const double *acum, int n);

/* Bernoulli: 1 con probabilidad p, 0 con probabilidad 1-p. */
int dist_bernoulli(prng_t *rng, double p);

/* Poisson de media lambda, por el método de Knuth (lambda pequeño).
 * Se usa para el número de piezas requeridas; no para el proceso de
 * llegadas, que se genera como proceso de renewing con saltos exponenciales. */
int dist_poisson(prng_t *rng, double lambda);

#endif /* SIM_DISTRIB_H */
