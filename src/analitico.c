/* analitico.c — ver analitico.h */
#include "sim/analitico.h"
#include <math.h>

mmc_t analitico_mmc(double lambda, double mu, int c)
{
    mmc_t r;
    int n;
    double suma = 0.0;   /* sumas del numerador de P0 */
    double a;            /* carga ofrecida = lambda/mu */

    r.lambda = lambda;
    r.mu = mu;
    r.c = c;
    r.lq = r.l = r.wq = r.w = 0.0;
    r.p0 = 0.0;
    r.rho = 0.0;
    r.estable = 0;

    if (c < 1 || mu <= 0.0 || lambda < 0.0) {
        return r;
    }

    a = lambda / mu;
    r.rho = a / (double)c;

    if (r.rho >= 1.0) {
        /* Sistema inestable: la cola crece sin límite. No hay solución
         * de estado estacionario que comparar. */
        return r;
    }
    r.estable = 1;

    /* P0 = [ sum_{n=0}^{c-1} a^n/n!  +  a^c/(c!·(1-rho)) ]^-1 */
    double termino = 1.0;
    suma = 1.0; /* n = 0 */
    for (n = 1; n < c; n++) {
        termino *= a / (double)n;
        suma += termino;
    }
    termino *= a / (double)c; /* ahora termino = a^c / c! */
    suma += termino / (1.0 - r.rho);

    r.p0 = 1.0 / suma;

    /* Lq = P_espera · ρ/(1-ρ), con P_espera = P0 · a^c/(c!·(1-ρ)).
     *
     * Es la forma de Erlang-C, y equivale a
     *     Lq = P0 · a^c · ρ / (c! · (1-ρ)²)      con a = λ/μ = c·ρ
     *
     * OJO: la forma P0·ρ^c/(c!(1-ρ)²) NO es la correcta: le falta el
     * factor (c·ρ)^c. Esa versión se detectó comparando contra Erlang-C
     * durante la fase F4 (ver notas/08-reporte.md). */
    {
        double factorial = 1.0;
        double p_espera;
        for (n = 2; n <= c; n++) {
            factorial *= (double)n;
        }
        p_espera = r.p0 * pow(a, (double)c) / (factorial * (1.0 - r.rho));
        r.lq = p_espera * r.rho / (1.0 - r.rho);
    }

    /* L = Lq + lambda/mu   (Little / identidad de Little con utilizacion) */
    r.l = r.lq + a;

    if (lambda > 0.0) {
        r.wq = r.lq / lambda;
        r.w = r.l / lambda;
    }

    return r;
}
