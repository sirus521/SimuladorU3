/* colas.c — ver colas.h */
#include "sim/colas.h"

#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Cola de prioridad                                                   */
/* ------------------------------------------------------------------ */

int cola_ini(cola_t *c, int cap)
{
    c->v = (evento_t *)malloc(sizeof(evento_t) * (size_t)cap);
    c->n = 0;
    c->cap = cap;
    c->orden = 0;
    return c->v != NULL;
}

void cola_lib(cola_t *c)
{
    free(c->v);
    c->v = NULL;
    c->n = c->cap = 0;
}

static int evento_menor(const evento_t *a, const evento_t *b)
{
    if (a->t != b->t) return a->t < b->t;
    return a->orden < b->orden;
}

int cola_push(cola_t *c, double t, int tipo, int indice)
{
    int i;
    if (c->n == c->cap) {
        int nuevo = c->cap * 2;
        evento_t *tmp = (evento_t *)realloc(c->v, sizeof(evento_t) * (size_t)nuevo);
        if (tmp == NULL) return 0;
        c->v = tmp;
        c->cap = nuevo;
    }
    c->v[c->n].t = t;
    c->v[c->n].orden = c->orden++;
    c->v[c->n].tipo = tipo;
    c->v[c->n].indice = indice;
    i = c->n++;
    while (i > 0) {
        int padre = (i - 1) / 2;
        evento_t tmp2;
        if (!evento_menor(&c->v[i], &c->v[padre])) break;
        tmp2 = c->v[i]; c->v[i] = c->v[padre]; c->v[padre] = tmp2;
        i = padre;
    }
    return 1;
}

int cola_pop(cola_t *c, evento_t *salida)
{
    int i = 0;
    if (c->n == 0) return 0;
    *salida = c->v[0];
    c->n--;
    if (c->n > 0) c->v[0] = c->v[c->n];
    for (;;) {
        int izq = 2 * i + 1;
        int der = 2 * i + 2;
        int menor = i;
        if (izq < c->n && evento_menor(&c->v[izq], &c->v[menor])) menor = izq;
        if (der < c->n && evento_menor(&c->v[der], &c->v[menor])) menor = der;
        if (menor == i) break;
        { evento_t tmp = c->v[i]; c->v[i] = c->v[menor]; c->v[menor] = tmp; }
        i = menor;
    }
    return 1;
}

int cola_frente(const cola_t *c, evento_t *salida)
{
    if (c->n == 0) return 0;
    *salida = c->v[0];
    return 1;
}

/* ------------------------------------------------------------------ */
/* Cola FIFO                                                           */
/* ------------------------------------------------------------------ */

int fifo_ini(fifo_t *f)
{
    f->v = NULL;
    f->n = f->cabeza = f->cap = 0;
    return 1;
}

void fifo_lib(fifo_t *f)
{
    free(f->v);
    f->v = NULL;
    f->n = f->cabeza = f->cap = 0;
}

int fifo_push(fifo_t *f, int valor)
{
    if (f->n >= f->cap) {
        int nuevo = f->cap ? f->cap * 2 : 64;
        int *tmp = (int *)realloc(f->v, sizeof(int) * (size_t)nuevo);
        if (tmp == NULL) return 0;
        f->v = tmp;
        f->cap = nuevo;
    }
    f->v[f->n++] = valor;
    return 1;
}

int fifo_pop(fifo_t *f)
{
    if (f->cabeza >= f->n) return -1;
    return f->v[f->cabeza++];
}

int fifo_vacia(const fifo_t *f)
{
    return f->cabeza >= f->n;
}

int fifo_pending(const fifo_t *f)
{
    return f->n - f->cabeza;
}

int fifo_frente(const fifo_t *f)
{
    if (f->cabeza >= f->n) return -1;
    return f->v[f->cabeza];
}
