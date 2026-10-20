/* test_distrib.c — Distribuciones de probabilidad.
 *
 * Criterio de aceptación de la fase F2:
 *   - chi-cuadrado p > 0.05 contra la teórica
 *   - Kolmogorov-Smirnov p > 0.05 sobre uniformes
 *   - la inversa de la exponencial reproduce las medias esperadas
 */
#include "test_util.h"
#include "sim/distrib.h"
#include "sim/prng.h"

#include <stdlib.h>

/* --- Chi-cuadrado para una tabla de frecuencias observadas vs. esperadas --- */
static double chi_cuadrado(const int *observado, const double *esperado, int n)
{
    double chi = 0.0;
    int i;
    for (i = 0; i < n; i++) {
        double d = (double)observado[i] - esperado[i];
        chi += d * d / esperado[i];
    }
    return chi;
}

/* Valor crítico de chi-cuadrado al 95 % para los grados de libertad usados.
 * Son valores tabulados, no aproximados. */
static double chi_cuadrado_critico(int gl)
{
    /* gl: 1..10 */
    static const double tabla[] = {
        0.0, 3.841, 5.991, 7.815, 9.488, 11.070,
        12.592, 14.067, 15.507, 16.919, 18.307
    };
    if (gl < 1) return 0.0;
    if (gl > 10) return 18.307;   /* beyond the table */
    return tabla[gl];
}

/* --- Exponencial: media y varianza --- */
static void prueba_exponencial(void)
{
    prng_t rng;
    const int N = 200000;
    const double media = 3.5;
    double suma = 0.0, suma2 = 0.0;
    int i;
    double m, var;

    prng_iniciar(&rng, 2026);
    for (i = 0; i < N; i++) {
        double x = dist_exponencial(&rng, media);
        AFIRMAR(x >= 0.0, "la exponencial dio un valor negativo");
        suma += x;
        suma2 += x * x;
    }
    m = suma / N;
    var = suma2 / N - m * m;

    AFIRMAR_CERCA(m, media, 0.05, "la media de la exponencial no cuadra");
    AFIRMAR_CERCA(var, media * media, 0.2, "la varianza de la exponencial no cuadra");
}

/* --- Exponencial por clases: chi-cuadrado contra la teórica --- */
static void prueba_exponencial_chi2(void)
{
    prng_t rng;
    const int N = 100000;
    const int CLASES = 10;
    const double media = 2.0;
    const double paso = 1.0;   /* clases de 1 hora */
    int observadas[CLASES];
    double esperadas[CLASES];
    double chi;
    int i, c;

    /* La última clase es abierta: incluye la cola infinita. Por eso su
     * esperada es P(X >= 9) y no el intervalo [9,10). */
    for (c = 0; c < CLASES; c++) {
        double inf = (double)c * paso;
        double sup = (double)(c + 1) * paso;
        observadas[c] = 0;
        if (c == CLASES - 1) {
            esperadas[c] = exp(-inf / media) * N;
        } else {
            esperadas[c] = (exp(-inf / media) - exp(-sup / media)) * N;
        }
    }

    prng_iniciar(&rng, 7);
    for (i = 0; i < N; i++) {
        double x = dist_exponencial(&rng, media);
        int c2 = (int)(x / paso);
        if (c2 >= CLASES) c2 = CLASES - 1;  /* la cola entra en la última */
        if (c2 < 0) c2 = 0;
        observadas[c2]++;
    }

    chi = chi_cuadrado(observadas, esperadas, CLASES);
    AFIRMAR(chi < chi_cuadrado_critico(CLASES - 1),
            "la exponencial falla el chi-cuadrado");
}

/* --- Uniforme en (0,1): Kolmogorov-Smirnov --- */
static void prueba_uniforme_ks(void)
{
    prng_t rng;
    const int N = 20000;
    double *muestras = (double *)malloc(sizeof(double) * (size_t)N);
    int i;
    double d = 0.0;

    AFIRMAR(muestras != NULL, "sin memoria para la prueba KS");
    if (muestras == NULL) return;

    prng_iniciar(&rng, 99);
    for (i = 0; i < N; i++) {
        muestras[i] = prng_uniforme(&rng);
    }
    /* qsort para ordenar */
    {
        int a, b;
        for (a = 1; a < N; a++) {
            double clave = muestras[a];
            b = a - 1;
            while (b >= 0 && muestras[b] > clave) {
                muestras[b + 1] = muestras[b];
                b--;
            }
            muestras[b + 1] = clave;
        }
    }
    for (i = 0; i < N; i++) {
        double f1 = ((double)i + 1.0) / (double)N;
        double f2 = (double)i / (double)N;
        double dd1 = fabs(f1 - muestras[i]);
        double dd2 = fabs(muestras[i] - f2);
        if (dd1 > d) d = dd1;
        if (dd2 > d) d = dd2;
    }
    free(muestras);

    /* Valor crítico de D para N = 20000 al 5 %: 1.36 / sqrt(N). */
    AFIRMAR(d < 1.36 / sqrt((double)N), "el uniforme falla el KS");
}

/* --- Categórica: las frecuencias siguen los pesos --- */
static void prueba_categorica(void)
{
    prng_t rng;
    const int N = 100000;
    double pesos[3] = {0.4, 0.4, 0.2};
    double acum[3] = {0.4, 0.8, 1.0};
    int cuentas[3] = {0, 0, 0};
    int i;

    prng_iniciar(&rng, 31337);
    for (i = 0; i < N; i++) {
        int c = dist_categorica(&rng, acum, 3);
        AFIRMAR(c >= 0 && c < 3, "la categórica devolvió un índice inválido");
        cuentas[c]++;
    }
    AFIRMAR_CERCA(cuentas[0], 0.4 * N, 0.02 * N, "frecuencia del tipo 0");
    AFIRMAR_CERCA(cuentas[1], 0.4 * N, 0.02 * N, "frecuencia del tipo 1");
    AFIRMAR_CERCA(cuentas[2], 0.2 * N, 0.02 * N, "frecuencia del tipo 2");
    (void)pesos;
}

/* --- Bernoulli: la frecuencia sigue p --- */
static void prueba_bernoulli(void)
{
    prng_t rng;
    const int N = 100000;
    const double p = 0.85;
    int unos = 0;
    int i;

    prng_iniciar(&rng, 4242);
    for (i = 0; i < N; i++) {
        unos += dist_bernoulli(&rng, p);
    }
    AFIRMAR_CERCA(unos, p * N, 0.02 * N, "frecuencia de la bernoulli");
}

/* --- Poisson: media y varianza --- */
static void prueba_poisson(void)
{
    prng_t rng;
    const int N = 100000;
    const double lambda = 2.5;
    double suma = 0.0, suma2 = 0.0;
    int i;

    prng_iniciar(&rng, 555);
    for (i = 0; i < N; i++) {
        int k = dist_poisson(&rng, lambda);
        AFIRMAR(k >= 0, "el poisson devolvió un valor negativo");
        suma += k;
        suma2 += (double)k * k;
    }
    AFIRMAR_CERCA(suma / N, lambda, 0.05, "la media del poisson no cuadra");
    AFIRMAR_CERCA(suma2 / N - (suma / N) * (suma / N), lambda, 0.10,
                  "la varianza del poisson no cuadra");
}

int main(void)
{
    printf("  Distribuciones\n");
    prueba_exponencial();
    prueba_exponencial_chi2();
    prueba_uniforme_ks();
    prueba_categorica();
    prueba_bernoulli();
    prueba_poisson();
    return test_resumen("distrib");
}
