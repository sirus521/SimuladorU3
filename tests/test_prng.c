/* test_prng.c — Prueba áurea del PRNG.
 *
 * Los valores esperados son los de notas/09-prng-generador.md, que a su vez
 * vienen de la demostración de la unidad anterior. Si esta prueba pasa, el
 * generador en C es el mismo que el documentado en el papel.
 *
 * Criterio de aceptación de la fase F1:
 *   - las 12 primeras filas de las semillas 1 y 2026 se reproducen exactamente
 *   - el periodo es 65536
 *   - sobre el ciclo completo, cada cara del dado aparece 10922 veces
 *   - hay 4 rechazos en el ciclo
 *   - 65536 celdas 2D distintas (16 bits) y 41654 (8 bits altos)
 */
#include "test_util.h"
#include "sim/prng.h"

#include <string.h>

/* Las 12 primeras filas de la tabla de la semilla 1:
 *   x(i-1), x(i), z, r, r mod 65536, dado */
typedef struct {
    unsigned short x_prev;
    unsigned short x;
    unsigned short z;
    unsigned int   r;
    unsigned int   resto;
    unsigned int   dado;
} fila_t;

static const fila_t SEMILLA_1[12] = {
    {    1, 37550,  2508,   15048, 15048, 1 },
    {37550, 54719, 12346,   74076,  8540, 2 },
    {54719, 65156, 26330,  157980, 26908, 3 },
    {65156,  2701, 26411,  158466, 27394, 3 },
    { 2701, 64682, 22526,  135156,  4084, 3 },
    {64682, 48619,  1871,   11226, 11226, 1 },
    {48619, 62112, 21489,  128934, 63398, 2 },
    {62112, 21337, 22950,  137700,  6628, 3 },
    {21337, 23014,  6579,   39474, 39474, 1 },
    {23014, 21079, 37611,  225666, 29058, 4 },
    {21079,  8188, 28044,  168264, 37192, 3 },
    { 8188, 18021, 43999,  263994,  1850, 5 }
};

static const fila_t SEMILLA_2026[12] = {
    { 2026, 25131, 59280,  355680, 28000, 6 },
    {25131, 33760, 43737,  262422,   278, 5 },
    {33760, 13721, 40037,  240222, 43614, 4 },
    {13721, 16678, 40271,  241626, 45018, 4 },
    {16678, 33431, 47731,  286386, 24242, 5 },
    {33431, 44348,  5669,   34014, 34014, 1 },
    {44348, 21669,  8230,   49380, 49380, 1 },
    {21669,  2466,  5838,   35028, 35028, 1 },
    { 2466, 39747, 58590,  351540, 23860, 6 },
    {39747, 52184, 60619,  363714, 36034, 6 },
    {52184,  2545, 63497,  380982, 53302, 6 },
    { 2545, 64862,  9591,   57546, 57546, 1 }
};

/* Reproduce una tabla fila por fila, comprobando estado, salida y dado. */
static void verificar_tabla(const char *etiqueta, const fila_t *tabla, int n)
{
    prng_t p;
    int i;

    prng_iniciar(&p, tabla[0].x_prev);

    for (i = 0; i < n; i++) {
        unsigned short z = prng_azar(&p);
        int cara;

        AFIRMAR(prng_estado(&p) == tabla[i].x,
                "el estado tras la iteración no coincide con la tabla");
        AFIRMAR(z == tabla[i].z,
                "la salida mezclada no coincide con la tabla");
        AFIRMAR((unsigned int)z * 6u == tabla[i].r,
                "el producto r = z·6 no coincide con la tabla");
        AFIRMAR(((unsigned int)z * 6u) % 65536u == tabla[i].resto,
                "el resto r mod 65536 no coincide con la tabla");

        cara = prng_dado_de_salida(z);
        AFIRMAR(cara == (int)tabla[i].dado,
                "el dado no coincide con la tabla");
    }

    /* La etiqueta se usa sólo en el mensaje de depuración. */
    (void)etiqueta;
}

static void prueba_tabla_semilla_1(void)
{
    verificar_tabla("semilla 1", SEMILLA_1, 12);
}

static void prueba_tabla_semilla_2026(void)
{
    verificar_tabla("semilla 2026", SEMILLA_2026, 12);
}

/* El periodo debe ser 65536: al volver a la semilla, el estado se repite. */
static void prueba_periodo(void)
{
    prng_t p;
    unsigned short inicio = 1;
    int pasos = 0;

    prng_iniciar(&p, inicio);
    do {
        prng_azar(&p);
        pasos++;
        if (pasos > 200000) break;   /* guarda contra un bucle infinito */
    } while (prng_estado(&p) != inicio);

    AFIRMAR(pasos == 65536, "el periodo no es 65536");
    AFIRMAR(p.muestras == 65536u, "el conteo de muestras no cuadra con el periodo");
}

/* Sobre el ciclo completo: uniformidad exacta del dado y 4 rechazos. */
static void prueba_ciclo_completo(void)
{
    prng_t p;
    int cuentas[7];
    int i, total = 0;
    int rechazos = 0;
    unsigned char vistos[65536];
    int celdas2d_16 = 0;
    int celdas2d_8 = 0;
    unsigned char visto8[256][256];
    unsigned short salidas[65536];

    memset(cuentas, 0, sizeof(cuentas));
    memset(vistos, 0, sizeof(vistos));
    memset(visto8, 0, sizeof(visto8));

    prng_iniciar(&p, 1);

    /* Se recorre el ciclo completo mezclando, y en paralelo se cuenta el
     * dado. Para no alterar la secuencia, el dado se calcula a partir de la
     * misma salida z que se está midiendo. */
    for (i = 0; i < 65536; i++) {
        unsigned short z = prng_azar(&p);
        int cara;

        salidas[i] = z;
        if (!vistos[z]) {
            vistos[z] = 1;
            celdas2d_16++;
        }

        cara = prng_dado_de_salida(z);
        if (cara == 0) {
            rechazos++;
        } else {
            cuentas[cara]++;
            total++;
        }
    }

    /* Celdas 2D contadas de forma cíclica: los pares son
     * (z_0,z_1), (z_1,z_2), ..., (z_65535,z_0). Es lo que describe la nota
     * ("pares (z_i, z_{i+1})") y arroja 41654 celdas de byte alto. */
    for (i = 0; i < 65536; i++) {
        unsigned char ah = (unsigned char)(salidas[i] >> 8);
        unsigned char bh = (unsigned char)(salidas[(i + 1) % 65536] >> 8);
        if (!visto8[ah][bh]) {
            visto8[ah][bh] = 1;
            celdas2d_8++;
        }
    }

    AFIRMAR(total == 65532, "no se aceptaron 65532 valores en el ciclo");
    AFIRMAR(rechazos == 4, "no hubo exactamente 4 rechazos en el ciclo");
    for (i = 1; i <= 6; i++) {
        AFIRMAR(cuentas[i] == 10922, "una cara del dado no aparece 10922 veces");
    }
    AFIRMAR(celdas2d_16 == 65536, "no se visitaron 65536 celdas 2D de 16 bits");
    AFIRMAR(celdas2d_8 == 41654, "no se visitaron 41654 celdas 2D de byte alto");
}

/* El histograma de 60 000 muestras de la nota. */
static void prueba_histograma_60000(void)
{
    struct { unsigned short semilla; int cuentas[6]; int rechazos; } casos[2] = {
        { 1,    { 10014, 9992, 9978, 10031, 9996, 9989 }, 3 },
        { 2026, { 9992, 9977, 9994, 9986, 10026, 10025 }, 4 }
    };
    int c;

    for (c = 0; c < 2; c++) {
        prng_t p;
        int cuentas[7];
        int i;

        memset(cuentas, 0, sizeof(cuentas));
        prng_iniciar(&p, casos[c].semilla);
        for (i = 0; i < 60000; i++) {
            cuentas[prng_dado(&p)]++;
        }
        for (i = 1; i <= 6; i++) {
            AFIRMAR(cuentas[i] == casos[c].cuentas[i - 1],
                    "el histograma de 60000 muestras no coincide con la nota");
        }
        AFIRMAR((int)p.rechazos == casos[c].rechazos,
                "el número de rechazos no coincide con la nota");
    }
}

/* La mezcla debe ser biyectiva: no puede colapsar valores distintos. */
static void prueba_mezcla_biyectiva(void)
{
    prng_t p;
    unsigned char visto[65536];
    int i;

    memset(visto, 0, sizeof(visto));
    prng_iniciar(&p, 2026);
    for (i = 0; i < 65536; i++) {
        unsigned short z = prng_azar(&p);
        AFIRMAR(!visto[z], "la mezcla produjo una salida repetida");
        visto[z] = 1;
    }
}

/* El uniforme debe estar en (0,1) para que log(1-u) nunca sea indefinido. */
static void prueba_uniforme_en_rango(void)
{
    prng_t p;
    int i;
    double minimo = 1.0, maximo = 0.0;

    prng_iniciar(&p, 7);
    for (i = 0; i < 20000; i++) {
        double u = prng_uniforme(&p);
        AFIRMAR(u > 0.0 && u < 1.0, "el uniforme se salió de (0,1)");
        if (u < minimo) minimo = u;
        if (u > maximo) maximo = u;
    }
    AFIRMAR(minimo > 0.0, "el uniforme mínimo debe ser mayor que cero");
    AFIRMAR(maximo < 1.0, "el uniforme máximo debe ser menor que uno");
}

int main(void)
{
    printf("  PRNG\n");
    prueba_tabla_semilla_1();
    prueba_tabla_semilla_2026();
    prueba_periodo();
    prueba_ciclo_completo();
    prueba_histograma_60000();
    prueba_mezcla_biyectiva();
    prueba_uniforme_en_rango();
    return test_resumen("prng");
}
