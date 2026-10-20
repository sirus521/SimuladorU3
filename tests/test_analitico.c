/* test_analitico.c — Solución analítica M/M/c.
 *
 * Estas pruebas existen porque en la fase F4 se detectó que la fórmula de
 * Lq que se había escrito en el módulo analítico estaba mal: le faltaba el
 * factor (c·ρ)^c. El simulador daba 0.1828 y la fórmula daba 0.1050 (74 % de
 * diferencia), mientras que Erlang-C daba 0.1751.
 *
 * Aquí la fórmula se contrasta contra dos referencias independientes:
 *   1. Erlang-C, calculada de otra forma.
 *   2. Casos cerrados conocidos (M/M/1 con Lq = ρ²/(1-ρ)).
 */
#include "test_util.h"
#include "sim/analitico.h"

#include <math.h>

/* Lq por la forma de Erlang-C, escrita de forma independiente. */
static double lq_erlang_c(double lambda, double mu, int c)
{
    double a = lambda / mu;
    double rho = a / (double)c;
    double suma = 1.0, termino = 1.0;
    double factorial = 1.0;
    double p0, p_espera;
    int k;

    for (k = 1; k < c; k++) {
        termino *= a / (double)k;
        suma += termino;
    }
    termino *= a / (double)c;
    suma += termino / (1.0 - rho);
    p0 = 1.0 / suma;
    for (k = 2; k <= c; k++) factorial *= (double)k;
    p_espera = p0 * pow(a, (double)c) / (factorial * (1.0 - rho));
    return p_espera * rho / (1.0 - rho);
}

static void prueba_contra_erlang_c(void)
{
    struct { double lambda, mu; int c; } casos[] = {
        { 1.6666666666666667, 2.0, 2 },
        { 1.5,               2.0, 1 },
        { 5.0,               2.0, 3 },
        { 0.5,               1.0, 1 },
        { 10.0,              3.0, 4 },
        { 1.0,               1.5, 2 }
    };
    size_t i;
    for (i = 0; i < sizeof(casos) / sizeof(casos[0]); i++) {
        mmc_t r = analitico_mmc(casos[i].lambda, casos[i].mu, casos[i].c);
        double esperado = lq_erlang_c(casos[i].lambda, casos[i].mu, casos[i].c);
        AFIRMAR(r.estable, "el sistema deberia ser estable");
        AFIRMAR_CERCA(r.lq, esperado, 1e-12,
                      "Lq no coincide con Erlang-C");
    }
}

/* M/M/1: Lq = ρ²/(1-ρ) y W = 1/(μ-λ). */
static void prueba_m_m_1(void)
{
    mmc_t r = analitico_mmc(2.0, 4.0, 1);
    double rho = 0.5;

    AFIRMAR_CERCA(r.rho, rho, 1e-12, "la utilizacion no cuadra");
    AFIRMAR_CERCA(r.lq, rho * rho / (1.0 - rho), 1e-12, "Lq de M/M/1 no cuadra");
    AFIRMAR_CERCA(r.l, rho * rho / (1.0 - rho) + rho, 1e-12, "L de M/M/1 no cuadra");
    AFIRMAR_CERCA(r.w, 1.0 / (4.0 - 2.0), 1e-12, "W de M/M/1 no cuadra");
    AFIRMAR_CERCA(r.wq, (rho * rho / (1.0 - rho)) / 2.0, 1e-12, "Wq de M/M/1 no cuadra");
    AFIRMAR_CERCA(r.p0, 1.0 - rho, 1e-12, "P0 de M/M/1 no cuadra");
}

/* Un sistema inestable se marca como tal y no da números sin sentido. */
static void prueba_inestable(void)
{
    mmc_t r = analitico_mmc(5.0, 1.0, 2);   /* rho = 2.5 */
    AFIRMAR(!r.estable, "un sistema con rho > 1 debe marcarse inestable");
    AFIRMAR(r.lq == 0.0 && r.w == 0.0, "un sistema inestable no debe dar tiempos");
}

/* Casos límite: sin llegadas o con un solo servidor. */
static void prueba_casos_limite(void)
{
    mmc_t r;

    r = analitico_mmc(0.0, 2.0, 2);
    AFIRMAR(r.estable, "sin llegadas el sistema es estable");
    AFIRMAR_CERCA(r.lq, 0.0, 1e-12, "sin llegadas no hay cola");
    AFIRMAR_CERCA(r.l, 0.0, 1e-12, "sin llegadas no hay gente en el sistema");

    r = analitico_mmc(1.0, 0.0, 2);
    AFIRMAR(!r.estable, "sin servicio no hay sistema estable");

    r = analitico_mmc(1.0, 1.0, 0);
    AFIRMAR(!r.estable, "con cero servidores no hay sistema estable");
}

int main(void)
{
    printf("  Analitico M/M/c\n");
    prueba_contra_erlang_c();
    prueba_m_m_1();
    prueba_inestable();
    prueba_casos_limite();
    return test_resumen("analitico");
}
