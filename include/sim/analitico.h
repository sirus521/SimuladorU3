/* analitico.h — Solución analítica M/M/c (Pollaczek-Khinchine).
 *
 * Se usa para VERIFICAR el simulador: si el motor y el modelo están bien,
 * los números simulados deben caer dentro del intervalo de confianza de la
 * fórmula. Ver notas/03-modelo-matematico.md § 7.
 */
#ifndef SIM_ANALITICO_H
#define SIM_ANALITICO_H

typedef struct {
    double lambda;   /* tasa de llegadas (por hora)                    */
    double mu;       /* tasa de servicio por servidor (por hora)      */
    int    c;        /* número de servidores                          */
    double rho;      /* utilización = lambda / (c·mu)                 */
    double p0;       /* probabilidad de que el sistema esté vacío     */
    double lq;       /* longitud media de la cola                      */
    double l;        /* número medio en el sistema                     */
    double wq;       /* espera media en la cola (horas)                */
    double w;        /* permanencia media en el sistema (horas)        */
    int    estable;  /* 1 si rho < 1 (sistema estable)                 */
} mmc_t;

/* Calcula la solución M/M/c. Si el sistema es inestable (rho >= 1), los
 * campos de espera quedan en 0 y `estable` valdrá 0. */
mmc_t analitico_mmc(double lambda, double mu, int c);

#endif /* SIM_ANALITICO_H */
