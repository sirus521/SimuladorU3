---
tipo: modelo-matematico
proyecto: Simulador de Taller de Reparación
estado: aprobado
---

# Modelo matemático

## 1. Reloj y unidades

- **Unidad de tiempo: horas.** Todo el modelo trabaja en horas reales.
- `t = 0` es el lunes a las 00:00 del inicio de la simulación.
- Horizonte: `dias` días completos (por defecto 30 → 720 h).
- El día comercial va de `09:00` a `18:00` (9 h).

## 2. Aleatoriedad

Toda la aleatoriedad sale de **un solo PRNG propio** ([[09-prng-generador]]), un
LCG de 16 bits con periodo máximo 65536 y una etapa de mezcla xorshift en la
salida. El generador se avanza **una vez por muestra**, con la semilla al inicio
de cada réplica.

Consecuencia práctica: **misma semilla + mismo escenario ⇒ mismos números**,
bit a bit. Sin eso no hay auditoría posible.

Las muestras se construyen sobre un **uniforme en [0,1)** obtenido del PRNG:
`u = (z + 0.5) / 65536`, con `z` la salida mezclada. El `+0.5` evita que `u`
sea exactamente 0 (que rompería el logaritmo de la inversa de la exponencial).

## 3. Distribuciones

| Magnitud | Distribución | Parámetro | Origen del parámetro |
|---|---|---|---|
| Llegadas | Proceso de Poisson (no estacionario) | λ = 18 / día dentro del horario | Supuesto de negocio |
| Duración del diagnóstico | Exponencial | μ_d por tipo: 0.5 h (eléctrico), 0.8 h (mecánico), 1.2 h (carrocería) | Supuesto, ver §6 |
| Duración de la reparación | Exponencial | μ_r por tipo: 2.0 h, 3.5 h, 6.0 h | Supuesto, ver §6 |
| Decisión del presupuesto | Exponencial + Bernoulli | media 0.5 h; p(acepta) = 0.85 | Supuesto |
| Tipo de avería | Categórica | 0.40 / 0.40 / 0.20 | Supuesto |
| Reposición de refacciones | Periódica determinista | cada 48 h, lote de 40 | Regla de negocio |

### Transformaciones inversas

Las tres distribuciones se generan con el método de inversión:

- **Exponencial:** `x = -media · ln(1 - u)`
- **Poisson (llegadas):** los saltos entre llegadas son `Exp(λ)`, es decir
  `-media · ln(1 - u)` con `media = 9 h / 18 llegadas`.
- **Uniforme discreta (tipo de avería):** se acumula la probabilidad y se toma
  el primer umbral que supera `u`.
- **Bernoulli (aceptación):** `u < 0.85`.

## 4. Eventos

La simulación es de **eventos discretos** con una cola de prioridad por tiempo.
Los empates en el tiempo se rompen por **orden de inserción**, de modo que dos
corridas con la misma semilla producen exactamente la misma secuencia.

| # | Evento | Qué dispara | Qué hace |
|---|---|---|---|
| E1 | `LLEGADA` | Reloj | Cliente llega → se genera OT → va a `cola_diagnostico` |
| E2 | `FIN_DIAGNOSTICO` | Reloj | Libera bahía → el cliente evalúa el presupuesto |
| E3 | `DECISION_PRESUPUESTO` | Reloj | Acepta → pide refacciones / Rechaza → el vehículo se va |
| E4 | `FIN_REPARACION` | Reloj | Libera mecánico → el vehículo se entrega, TPR registrado |
| E5 | `REPOSICION` | Reloj | Repone inventario → libera OTs en espera de refacciones |
| E6 | `FIN_HORIZONTE` | Reloj | Termina la simulación; lo que quede en el sistema se reporta aparte |

### Diagrama de transiciones de estado de una OT

```
   [nueva]
      │ E1
      ▼
 [espera_diagnostico] ──hay bahía──► [en_diagnostico]
      │                                    │ E2
      │                                    ▼
      │                          [espera_decision]
      │                                    │ E3 (acepta)
      │                                    ▼
      │                            [espera_refaccion]  ◄── no hay stock
      │                                    │ E5 (hay stock)
      │                                    ▼
      │                          [espera_reparacion] ──hay mecánico──► [en_reparacion]
      │                                    ▲                                │ E4
      │                                    └────────────────────────────────┘
      │                                                                  ▼
      └──────────────────── (rechazo) ──────────────────────────────► [cerrada]
                                                                     [entregada]
```

## 5. Estadísticas acumuladas

El motor no guarda la historia completa de cada entidad: acumula estadísticas en
caliente, porque un horizonte de 30 días × 18 llegadas produce miles de
registros y no tiene sentido guardarlos todos.

Se acumulan:

- Suma de TPR, TPR² (para la varianza), y un **histograma de TPR** con 20
  clases, del que sale el percentil 90.
- Contadores: OT entregadas, presupuestos rechazados, OT bloqueadas por
  refacciones, minutos de bahía ocupada, minutos de mecánico ocupado.
- **Series temporales** muestreadas cada 15 min: longitud de cada cola, trabajo
  en curso, longitud de la cola de refacciones.

### Réplicas y confianza

Una sola corrida es una **muestra** de un proceso estocástico, no un resultado.
Por eso:

- Se corren **30 réplicas** con semillas distintas (`semilla_base + i`).
- Cada réplica devuelve sus métricas.
- El resultado publicado es la **media entre réplicas** con un
  **intervalo de confianza al 95 %**:
  `IC = 1.96 · s / √n`, con `s` la desviación muestral y `n = 30`.
- Se reporta además la **semilla base**, para que el resultado sea repetible.

### Periodo de calentamiento

El taller arranca vacío: las primeras OT se atendieron de inmediato, porque
no había nadie en la cola. Medir desde el primer minuto mezcla ese
transitorio con el régimen permanente.

La evidencia es directa: **sin calentamiento, el TPR medido crece con el
horizonte**, porque cada horizonte más largo incluye más días de cola
acumulada.

| Horizonte | TPR medido (sin calentamiento) |
|---|---|
| 30 días | 16.57 h ± 3.13 |
| 60 días | 26.70 h ± 5.30 |
| 90 días | 38.10 h ± 7.52 |
| 180 días | 62.86 h ± 13.40 |

El taller no empeora con el tiempo; lo que crece es el peso de los días
acumulados en la cola dentro de la muestra. Por eso el motor **descarta los
primeros `dias_calentamiento` días** (10 por defecto) y sólo a partir de ahí
acumula estadísticas, series y conteos.

### Diagnóstico de estado estacionario

Con calentamiento, un taller saturado sigue sin alcanzar un régimen: la cola
sigue creciendo. Para no publicar un número engañoso, cada corrida compara el
TPR de los vehículos entregados en la **primera mitad** del periodo medido
con el de la **segunda mitad**.

- Si difieren menos de 15 %, el sistema está en estado estacionario.
- Si difieren más, el simulador marca `en_estacionario = 0` y avisa de que
  **el TPR sigue creciendo con el horizonte**, así que el valor publicado
  depende de cuántos días se simularon.

En el escenario base la deriva es de 1.0 % (estacionario). En el escenario
saturado es de 56.8 % y en el de atasco de refacciones de 62.3 %: en los dos
últimos casos el aviso aparece.

Además del promedio se reporta el **error relativo contra la solución
analítica** del modelo M/M/c equivalente, que es la verificación del motor.

## 6. Supuestos del modelo matemático

| # | Supuesto | Justificación | Riesgo si es falso |
|---|---|---|---|
| S1 | Tiempos de servicio exponenciales | Es lo que hace el modelo comparable con M/M/c y con Little | Los tiempos reales son más agrupados → el taller se ve más vacío de lo que es |
| S2 | Las llegadas son Poisson | Comportamiento estándar de demanda de servicios | Picos reales más altos → subestima la saturación |
| S3 | Un tipo de avería por OT, fijado al llegar | Simplifica; en la realidad una OT puede traer varios problemas | Subestima el trabajo total |
| S4 | La decisión del presupuesto es independiente del monto | El modelo no maneja dinero | La tasa de rechazo real varía con el precio |
| S5 | Las refacciones son fungibles entre tipos | No hay surtidos | El bloqueo real puede ser mayor o menor |
| S6 | El cliente que rechaza se va para siempre | No hay recompra modelada | El taller real pierde menos demanda de la que se calcula |
| S7 | La reposición es periódica y de lote fijo | Regla de negocio sencilla | Con inventario crítico, el bloqueo depende del momento del ciclo |
| S8 | Los trabajos no se cancelan por otra causa | Solo hay dos desenlaces: atendido o rechazado | — |

**El supuesto S1 es el más importante.** Está anotado en el reporte porque es el
que más impacto tiene en la interpretación de los resultados: un modelo
exponencial **subestima el percentil 90** (tiempos de servicio reales están más
concentrados alrededor de su media, así que la cola se mueve menos de lo que el
modelo supone).

## 7. Verificación contra teoría

El modelo de **diagnóstico** es un M/M/c con:
- `λ = 18 llegadas / 9 h de horario = 2.0 llegadas/h` (tasa dentro del
  horario). Para la solución de estado estacionario se usa la tasa media
  diaria, `18/24 = 0.75 llegadas/h`, porque el taller sigue trabajando
  fuera del horario de atención.
- `μ_d` = media ponderada de los tiempos de diagnóstico
- `c` = bahías de diagnóstico

Y el de **reparación** es M/M/c con `c` = mecánicos y `λ` = las llegadas que
superan el presupuesto.

Las fórmulas analíticas usadas (Little, M/M/c de Pollaczek-Khinchine):

```
a  = λ / μ                            carga ofrecida
ρ  = a / c                            estabilidad: ρ < 1
P0 = [Σ(n=0..c-1) a^n/n! + a^c/(c!(1-ρ))]⁻¹

P_espera = P0 · a^c / (c!·(1-ρ))      probabilidad de tener que esperar
Lq = P_espera · ρ / (1-ρ)
L  = Lq + a
W  = L / λ        Wq = Lq / λ
```

> **OJO con la forma de `Lq`.** La expresión `P0 · ρ^c / (c!·(1-ρ)²)` es
> **incorrecta**: le falta el factor `(c·ρ)^c`. Durante el desarrollo del
> proyecto esa versión daba 0.1050 para un caso donde Erlang-C da 0.1751, y
> el simulador daba 0.1828 — un 74 % de diferencia. El error estaba en la
> fórmula, no en el motor, y se detectó porque la comparación analítica es la
> puerta de la fase F4. La forma correcta es la de Erlang-C, escrita arriba.
> Ver `tests/test_analitico.c`, que contrasta contra dos referencias
> independientes.

**Cuándo la comparación es exacta.** Sólo cuando se cumplen las dos
condiciones:

1. **Un solo tipo de avería**, para que el servicio sea una exponencial pura
   y no una mezcla.
2. **El taller abre las 24 h**, para que las llegadas sean estacionarias.

El escenario `datos/escenarios/validacion-mmc.json` cumple las dos, y ahí el
error del motor contra la fórmula es de **0.20 %** en W. En los demás
escenarios la comparación es aproximada, y la interfaz lo dice.

El motor se valida contra estas fórmulas: si el error relativo se sale del
intervalo de confianza, **el modelo se corrige, no el número**.

## Ver también

- [[02-modelo-del-negocio]] — el proceso en palabras
- [[09-prng-generador]] — el generador de números
- [[08-reporte]] — resultados y validación
