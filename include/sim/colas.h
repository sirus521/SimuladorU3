/* colas.h — Estructuras de cola compartidas por el motor de análisis y el
 * de juego.
 *
 * Antes de existir este archivo, estas estructuras vivían dentro de
 * src/modelo.c. Se extrajeron porque el motor de juego necesita las mismas
 * dos primitivas, y duplicarlas habría sido una fuente de bugs
 * garantizada.
 *
 * Ver notas/05-plan-de-trabajo.md, fase G1.
 */
#ifndef SIM_COLAS_H
#define SIM_COLAS_H

#include <stdint.h>

/* ------------------------------------------------------------------ */
/* Cola de prioridad por tiempo (min-heap)                             */
/* ------------------------------------------------------------------ */

/* Los tipos de evento son del motor; aquí sólo se declara el contenedor
 * genérico, con el tiempo y un orden de inserción para desempatar. */
typedef struct {
    double   t;
    uint32_t orden;
    int      tipo;     /* lo interpreta el motor que lo usa */
    int      indice;   /*Payload: índice de vehículo, NPC, etc. */
} evento_t;

typedef struct {
    evento_t *v;
    int       n;
    int       cap;
    uint32_t  orden;   /* contador global de inserción */
} cola_t;

int  cola_ini(cola_t *c, int cap);
void cola_lib(cola_t *c);
/* Inserta un evento. Devuelve 0 si no hay memoria. */
int  cola_push(cola_t *c, double t, int tipo, int indice);
/* Extrae el evento más próximo. Devuelve 0 si la cola está vacía. */
int  cola_pop(cola_t *c, evento_t *salida);
/* Mira el evento más próximo sin sacarlo. Devuelve 0 si está vacía. */
int  cola_frente(const cola_t *c, evento_t *salida);

/* ------------------------------------------------------------------ */
/* Cola FIFO de índices (con cabeza que avanza)                        */
/* ------------------------------------------------------------------ */

typedef struct {
    int *v;
    int  n;       /* elementos totales insertados */
    int  cabeza;  /* índice del primero pendiente */
    int  cap;
} fifo_t;

int  fifo_ini(fifo_t *f);
void fifo_lib(fifo_t *f);
int  fifo_push(fifo_t *f, int valor);
int  fifo_pop(fifo_t *f);          /* -1 si está vacía */
int  fifo_vacia(const fifo_t *f);
int  fifo_pending(const fifo_t *f); /* elementos en espera */
int  fifo_frente(const fifo_t *f);  /* el primero, sin sacarlo; -1 si vacía */

#endif /* SIM_COLAS_H */
