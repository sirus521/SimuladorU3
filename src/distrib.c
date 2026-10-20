/* distrib.c — ver distrib.h */
#include "sim/distrib.h"
#include <math.h>

double dist_exponencial(prng_t *rng, double media)
{
    /* Inversión: X = -media · ln(1 - U), con U en (0,1). prng_uniforme nunca
     * devuelve 0 ni 1, así que el logaritmo es siempre finito. */
    double u = prng_uniforme(rng);
    return -media * log(1.0 - u);
}

int dist_categorica(prng_t *rng, const double *acum, int n)
{
    double u = prng_uniforme(rng);
    int i;
    for (i = 0; i < n; i++) {
        if (u < acum[i]) {
            return i;
        }
    }
    /* Por redondeo, un valor puede quedar por encima del último acumulado.
     * Se devuelve el último índice en vez de fallar: es un error de punto
     * flotante, no del modelo. */
    return n - 1;
}

int dist_bernoulli(prng_t *rng, double p)
{
    return prng_uniforme(rng) < p ? 1 : 0;
}

int dist_poisson(prng_t *rng, double lambda)
{
    /* Método de Knuth. Es estable para lambda pequeño (aquí se usa para
     * números de piezas, con lambda ≤ 3). */
    double limite = exp(-lambda);
    double producto = 1.0;
    int k = 0;
    do {
        k++;
        producto *= prng_uniforme(rng);
    } while (producto > limite && k < 10000);
    return k - 1;
}
