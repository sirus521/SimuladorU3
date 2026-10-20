/* test_util.h — Asserts mínimos para las pruebas en C.
 *
 * No usamos ningún framework externo: no se puede instalar nada en esta
 * máquina (sudo pide password) y una dependencia de pruebas sería más
 * pesada que 60 líneas propias.
 */
#ifndef SIM_TEST_UTIL_H
#define SIM_TEST_UTIL_H

#include <math.h>
#include <stdio.h>

static int test_fallidas = 0;
static int test_total = 0;

#define AFIRMAR(cond, msg)                                                    \
    do {                                                                       \
        test_total++;                                                          \
        if (!(cond)) {                                                         \
            test_fallidas++;                                                   \
            fprintf(stderr, "   FALLA %s:%d  %s\n", __FILE__, __LINE__, (msg)); \
        }                                                                      \
    } while (0)

#define AFIRMAR_CERCA(a, b, tol, msg)                                         \
    do {                                                                       \
        double _a = (a), _b = (b);                                             \
        test_total++;                                                          \
        if (!(fabs(_a - _b) <= (tol))) {                                       \
            test_fallidas++;                                                   \
            fprintf(stderr, "   FALLA %s:%d  %s  (%.10g vs %.10g, tol %.3g)\n", \
                    __FILE__, __LINE__, (msg), _a, _b, (double)(tol));          \
        }                                                                      \
    } while (0)

static int test_resumen(const char *nombre)
{
    if (test_fallidas == 0) {
        printf("   OK  %s (%d comprobaciones)\n", nombre, test_total);
        return 0;
    }
    printf("   FALLO %s (%d de %d comprobaciones fallaron)\n",
           nombre, test_fallidas, test_total);
    return 1;
}

#endif /* SIM_TEST_UTIL_H */
