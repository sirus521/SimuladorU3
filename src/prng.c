/* prng.c — ver prng.h y notas/09-prng-generador.md */
#include "sim/prng.h"

#define M 65536u

void prng_iniciar(prng_t *p, uint16_t semilla)
{
    p->estado = semilla;
    p->muestras = 0;
    p->rechazos = 0;
}

/* Mezcla xorshift biyectiva de 16 bits. Al ser una permutación, no acorta
 * el periodo del LCG. Se aplica únicamente a la salida, nunca al estado. */
static uint16_t mezclar(uint16_t z)
{
    z = (uint16_t)(z ^ (uint16_t)((z << 7) & 0xFFFFu));
    z = (uint16_t)(z ^ (uint16_t)(z >> 9));
    z = (uint16_t)(z ^ (uint16_t)((z << 8) & 0xFFFFu));
    return z;
}

uint16_t prng_azar(prng_t *p)
{
    /* 25205 * 65535 + 12345 cabe en uint32_t (< 2^32), así que no hay
     * desbordamiento con aritmética de 32 bits sin signo. */
    p->estado = (uint16_t)((25205u * (uint32_t)p->estado + 12345u) & 0xFFFFu);
    p->muestras++;
    return mezclar(p->estado);
}

uint16_t prng_estado(const prng_t *p)
{
    return p->estado;
}

int prng_dado_de_salida(uint16_t z)
{
    /* r = z·6 ocupa como máximo 19 bits (65535·6 = 393210). Se reparte en
     * bloques de 65536: el cociente cubre 0..5 y el resto deja un hueco de
     * 4 valores, que se rechazan para conservar la uniformidad exacta. */
    uint32_t r = (uint32_t)z * 6u;
    if ((r & 0xFFFFu) < 4u) {
        return 0; /* hueco: rechazar */
    }
    return (int)(1u + (r >> 16));
}

unsigned prng_dado(prng_t *p)
{
    for (;;) {
        uint16_t z = prng_azar(p);
        int cara = prng_dado_de_salida(z);
        if (cara != 0) {
            return (unsigned)cara;
        }
        p->rechazos++;
    }
}

double prng_uniforme(prng_t *p)
{
    uint16_t z = prng_azar(p);
    return ((double)z + 0.5) / 65536.0;
}
