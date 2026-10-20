---
tipo: reporte
proyecto: Simulador de Taller de Reparación
estado: aprobado
---

# Reporte: Simulador de Operaciones de un Taller de Reparación (SOTR)

**Materia:** Simulación · **Unidad:** 3
**Autor:** Sirus521
**Código:** `https://github.com/sirus521/SimuladorU3`

---

## 1. Resumen

Se construyó un simulador de eventos discretos de un taller de reparación
automotriz, con el motor de simulación escrito en C y una interfaz web que
consume la API del propio servidor en C.

El simulador responde cuatro preguntas de operación —cuánto tarda un vehículo
en salir, cuánto se cierra al día, qué tan saturado está el taller y si el
cuello de botella son las personas o el almacén— con cada cifra acompañada de
su **intervalo de confianza al 95 %**, calculado sobre 30 réplicas
independientes.

**Resultado principal.** En el escenario de referencia (18 vehículos/día, 2
bahías de diagnóstico, 3 mecánicos), el TPR medio es **9.29 h ± 0.82 h** y se
entregan **15.04 ± 0.26** órdenes de trabajo por día. La etapa de
**reparación** es el cuello de botella (69.8 % de utilización) frente al
diagnóstico (28.0 %). El almacén de refacciones no es el problema en este
escenario: sólo el 6.8 % de las OT queda esperando una pieza.

**Verificación.** En un escenario donde el subsistema de diagnóstico es
exactamente un M/M/c, el simulador coincide con la solución analítica de
Pollaczek-Khinchine: **error relativo de 0.20 %** en la permanencia media, de
0.7 % en la longitud de la cola y de 0.2 % en el número en el sistema.

---

## 2. Objetivos y alcance

El objetivo está detallado en [[01-definicion-del-simulador]]. En resumen: la
pregunta que ordena el proyecto es **¿el cuello de botella del taller es la
gente o son las refacciones?** y la interfaz tiene que ayudar a responderla.

**Dentro del alcance:** el proceso completo (recepción → diagnóstico →
presupuesto → refacciones → reparación → entrega), tamaño del taller
configurable, réplicas con IC 95 %, comparación contra la solución analítica,
e interfaz web de control.

**Fuera del alcance:** dinero y rentabilidad, calidad del trabajo, rotación
de personal, llegada estacional por hora, persistencia y usuarios. El
supuesto más importante que queda fuera es que **el cliente que rechaza el
presupuesto se va para siempre** y que el que recibe el vehículo reparado
siempre regresa.

---

## 3. El modelo

El proceso completo, con sus recursos, colas y reglas, está en
[[02-modelo-del-negocio]]. Las distribuciones, los eventos y los supuestos
numéricos están en [[03-modelo-matematico]].

### 3.1 Estructura del modelo

| Etapa | Recurso | Cola que genera | Distribución del servicio |
|---|---|---|---|
| Diagnóstico | 2 bahías | `cola_diagnostico` | Exponencial (0.5 / 0.8 / 1.2 h) |
| Presupuesto | — | `espera_decision` | Exponencial (0.5 h) + Bernoulli(0.85) |
| Refacciones | almacén | `cola_refacciones` | Reposición periódica (48 h) |
| Reparación | 3 mecánicos | `cola_reparacion` | Exponencial (2.0 / 3.5 / 6.0 h) |

Las llegadas siguen un proceso de Poisson **restringido al horario de
atención** (09:00–18:00). Los vehículos que ya entraron siguen su curso
aunque el taller cierre, lo que mantiene una sola tasa de servicio.

### 3.2 Aleatoriedad

Toda la aleatoriedad sale de un PRNG propio: un LCG de 16 bits con periodo
máximo 65536 y una etapa de mezcla xorshift biyectiva, documentado en
[[09-prng-generador]]. Cada réplica usa su propia semilla, de modo que las
réplicas son **independientes** —requisito del intervalo de confianza— y cada
una es reproducible por separado.

El generador se verifica contra las tablas de la unidad anterior: las 12
primeras iteraciones de las semillas 1 y 2026 se reproducen exactamente, el
periodo es 65536, y sobre el ciclo completo cada cara del dado aparece
10922 veces con 4 rechazos.

### 3.3 Periodo de calentamiento

El taller arranca vacío y la cola se llena durante los primeros días. Medir
desde el primer minuto mezcla el transitorio con el régimen permanente y
**subestima el TPR**: sin calentamiento, el TPR medido crecía con el horizonte
(16.5 h a 30 días, 26.7 h a 60, 38.1 h a 90), no porque el taller empeorara
sino porque cada horizonte incluía más días de cola acumulada.

Por eso el motor **descarta los primeros días** (10 por defecto, configurable)
y sólo acumula estadísticas a partir de ahí.

Además, cada corrida reporta si el sistema **está en estado estacionario**,
comparando el TPR de la primera y la segunda mitad del periodo medido. Si
difieren más de 15 %, el simulador lo dice, porque en un taller saturado el
número publicado depende del horizonte y no describe un régimen.

---

## 4. Verificación y validación

### 4.1 Verificación del motor contra la solución analítica

El escenario `validacion-mmc` abre las 24 h y usa un solo tipo de avería, de
modo que el subsistema de diagnóstico es un **M/M/c exacto**. La comparación
se hace contra Pollaczek-Khinchine:

| Magnitud | Simulado | Teórico M/M/c | Error relativo |
|---|---|---|---|
| Lq (longitud de cola) | 0.0711 | 0.0706 | **0.7 %** |
| L (número en el sistema) | 0.7053 | 0.7039 | **0.2 %** |
| W (permanencia media) | 0.8464 h | 0.8447 h | **0.20 %** |

El error está muy por debajo del margen de ruido de la simulación, lo que
confirma que el motor de eventos discretos, la cola de prioridad y los
acumuladores de estadística funcionan como deben.

> **Un bug que esta comparación encontró.** Durante el desarrollo, la fórmula
> de `Lq` que se había escrito a mano estaba mal: le faltaba el factor
> `(c·ρ)^c`, y daba 0.1050 contra los 0.1751 que produce Erlang-C. El
> simulador, en cambio, daba 0.1828. Es decir, **el motor estaba bien y la
> fórmula estaban mal**, con un 74 % de diferencia. Se detectó justamente
> por tener la comparación analítica como puerta de la fase F4, y se
> corrigió la fórmula, no los números. La historia completa está en
> [[06-bitacora-y-decisiones]].

### 4.2 Validación del modelo

| Prueba | Resultado |
|---|---|
| Uniformidad de la exponencial | Chi-cuadrado p > 0.05 |
| Uniformidad del generador | Kolmogorov-Smirnov p > 0.05 |
| Uniformidad del dado | 10922 apariciones por cara sobre el ciclo completo |
| Reproducibilidad | Misma semilla ⇒ resultado idéntico (bit a bit) |
| Consistencia interna | Little: L ≈ λ·W se cumple |
| Estado estacionario | Deriva del TPR entre mitades: 1.0 % (base) |

### 4.3 El alcance de la comparación analítica

En el escenario base la comparación contra M/M/c es **aproximada**, y la
interfaz lo dice explícitamente. Hay dos razones, y conviene tenerlas claras
porque es la diferencia entre "el modelo es correcto" y "el modelo se parece
a un M/M/c":

1. **Las llegadas no son estacionarias.** Se concentran en 9 horas, así que la
   cola se llena en el turno y se drena de noche. El estado estacionario que
   supone la fórmula no se alcanza.
2. **El servicio es una mezcla de exponenciales** (tres tipos de avería), no
   una exponencial pura.

Por eso el error aparente en W es de 52 % en el escenario base, y por eso la
**verificación rigurosa** se hace en el escenario dedicado, donde el error
baja a 0.20 %.

---

## 5. Resultados

Todos los números salen de `make` + `bin/simulador --escenario … --tabla`, con
30 réplicas y semilla 2026.

### 5.1 Escenario base (18 vehículos/día, 2 bahías, 3 mecánicos)

| Métrica | Valor | IC 95 % |
|---|---|---|
| TPR medio | 9.285 h | ± 0.819 |
| TPR percentil 90 | 18.237 h | ± 1.673 |
| OT entregadas por día | 15.042 | ± 0.258 |
| Presupuestos rechazados | 14.97 % | ± 0.80 |
| Bloqueo por refacción | 6.77 % | ± 2.21 |
| Ociosidad del taller | 12.74 % | ± 1.34 |
| Utilización diagnóstico | 28.02 % | ± 0.82 |
| Utilización reparación | **69.84 %** | ± 1.92 |
| Cola media diagnóstico | 0.365 | ± 0.037 |
| Cola media reparación | **2.315** | ± 0.395 |
| Estado estacionario | sí (deriva 1.0 %) | |

**Lectura.** La reparación es el cuello de botella: 69.8 % de utilización
contra 28.0 % en diagnóstico, y una cola media de 2.3 vehículos contra 0.37.
El diagnóstico no es el problema. Añadir un cuarto mecánico ayudaría; añadir
una tercera bahía, casi nada.

El 15 % de presupuestos rechazados es un **punto ciego del modelo**: no se
sabe si el cliente se fue por el precio o por el tiempo de espera, porque el
modelo no maneja dinero. Es la primera extensión que haría falta.

### 5.2 Escenario saturado (24 vehículos/día, 1 bahía, 2 mecánicos)

| Métrica | Valor |
|---|---|
| TPR medio | 147.560 h ± 7.644 |
| Cola media reparación | 70.124 ± 4.468 |
| Utilización reparación | 100.00 % |
| Bloqueo por refacción | 80.53 % |
| Estado estacionario | **NO** (deriva 56.8 %) |

El taller no tiene capacidad para esa demanda. La cola de reparación crece sin
límite, el TPR crece con el horizonte y el simulador **lo advierte** en lugar
de publicar un número engañoso. Este escenario sirve para mostrar por qué el
diagnóstico de estado estacionario importa.

### 5.3 Escenario con atasco de refacciones (4 unidades de stock)

| Métrica | Valor |
|---|---|
| TPR medio | 408.716 h ± 11.801 |
| OT entregadas por día | 0.515 |
| Bloqueo por refacción | **85.03 %** |
| Utilización reparación | 2.21 % |
| Estado estacionario | **NO** (deriva 62.3 %) |

El resultado más ilustrativo del proyecto: **el taller está ocioso el 97.8 %
del tiempo y, aun así, no entrega vehículos.** Mecánicos sin trabajo,
vehículos parados y el cuello de botella íntegramente en el almacén. Es
exactamente el escenario que justifica tener la métrica de bloqueo por
refacciones, y la respuesta a la pregunta del objetivo: *aquí el cuello de
botella no son las personas*.

### 5.4 Escenario de validación M/M/c

| Métrica | Valor | IC 95 % |
|---|---|---|
| TPR medio | 8.329 h | ± 0.521 |
| Cola media diagnóstico | 0.071 | ± 0.005 |
| Utilización reparación | 80.13 % | ± 1.17 |
| Error en W contra la fórmula | **0.20 %** | — |
| Comparación exacta | sí | — |

---

## 6. La interfaz

Descripción completa en [[04-parte-visual]]. La interfaz es un **tablero de
control**, no un juego: cinco zonas —parámetros, tarjetas de KPI, planta del
taller, series de colas, y las gráficas de comparación— con la regla de que
**ninguna cifra aparece sin su intervalo de confianza**.

La planta se alimenta de los valores medios y finales del horizonte; no se
anima en tiempo real. Es una decisión consciente: animar la simulación en
vivo es un efecto visual que no aporta información que las series no den, y
complicaría el contrato de la API sin ganar nada.

---

## 7. Trabajo futuro

1. **Modelar el precio.** El 15 % de rechazo es la mayor incógnita del
   modelo, y un precio como variable explicativa es lo que permitiría
   simular políticas de descuento.
2. **Reordenamiento de llegadas.** El supuesto S1 (servicio exponencial)
   subestima el percentil 90. Un servicio Erlang de 2 fases es más realista
   y se puede comparar con la misma fórmula.
3. **Comparación lado a lado de escenarios.** Hoy se comparan corriendo dos
   veces; la interfaz podría mostrar la diferencia directamente.
4. **Datos reales.** Todos los parámetros son supuestos documentados. Con
   una bitácora de un taller real, el modelo pasaría de verificado a
   validado.

---

## 8. Cómo reproducir estos resultados

```bash
git clone https://github.com/sirus521/SimuladorU3
cd SimuladorU3
make && make web && make test

./bin/simulador --escenario datos/escenarios/base.json --tabla
./bin/simulador --escenario datos/escenarios/validacion-mmc.json --tabla
```

Con la semilla 2026 y 30 réplicas, los números de la sección 5 se reproducen
exactamente. Cualquier persona con el repositorio obtiene las mismas cifras.

---

## Ver también

- [[01-definicion-del-simulador]] — nombre, objetivo y alcance
- [[02-modelo-del-negocio]] — el proceso de negocio
- [[03-modelo-matematico]] — distribuciones, eventos y supuestos
- [[04-parte-visual]] — la interfaz
- [[05-plan-de-trabajo]] — el método y las fases
- [[06-bitacora-y-decisiones]] — las decisiones y el bug de la fórmula
- [[09-prng-generador]] — el generador de números
