---
tipo: modelo-matematico
proyecto: Simulador de Taller de Reparación
estado: borrador
---

# Modelo matemático

> **Qué cambió respecto a la versión anterior.** El modelo base (reloj
> simulado, llegadas de Poisson, tiempos de servicio) **se conserva entero**:
> es lo que hace el juego creíble. Lo que se añade es (a) la escala del
> tiempo real, (b) la economía, y (c) el hecho de que **el tiempo de servicio
> lo aporta el jugador**, no el azar.

## 1. Reloj y unidades

- **Unidad del modelo: horas de juego.** Todo el motor trabaja en horas.
- Un día de juego son 9 horas de atención (09:00–18:00).
- **Escala de tiempo real:** 1 segundo real = 1 minuto de juego
  (configurable: ×1, ×2, ×4). Con la escala ×1, un día de juego dura **9
  minutos reales**; a ×4, unos 2 minutos y cuarto.
- La noche (18:00–09:00) **no es jugable**: el juego avanza solo y la
  temporada siguiente trae clientes nuevos.

### Por qué la escala importa

El jugador tiene que **sentir** el reloj sin aburrirse. La regla de diseño
es: **ninguna acción del jugador debe tardar más de ~20 segundos reales**.
Con escala ×1, la reparación más larga (6 h de carrocería) son 6 minutos
reales: demasiado. Por eso el jugador **no mantiene pulsada la tecla**: la
acción se inicia, el trabajo **continúa solo**, y el jugador puede atender
otro puesto mientras tanto. Lo que el jugador controla es **cuándo empieza**
cada cosa, no cuánto tarda.

Esto es lo que hace que el juego sea de **triage y no de ritmo**, y coincide
con el modelo: los tiempos de servicio son deterministas por tipo de avería.

## 2. Aleatoriedad

Toda la aleatoriedad sigue viniendo del **PRNG propio** ([[09-prng-generador]]):
LCG de 16 bits con periodo máximo 65536 y mezcla xorshift.

| Sorteo | Distribución | Para qué |
|---|---|---|
| Llegadas | Proceso de Poisson, no estacionario | Cuándo llega cada cliente |
| Tipo de avería | Categórica (40/40/20) | Qué avería trae el cliente |
| Aceptación del presupuesto | Bernoulli con umbral de precio | Si el cliente contrata |
| Ruido en el trabajo | Uniforme ±10 % | Variación menor en el tiempo real |

**El PRNG sigue siendo autoritativo y reproducible**: misma semilla + mismas
acciones del jugador ⇒ misma partida. Eso permite depurar un bug del juego
reproduciendo la partida exacta.

> **Restricción nueva:** el LCG tiene periodo 65536. En una partida de 30
> días × 9 h, con ~18 clientes por día, se consumen del orden de 30 000
> muestras: por debajo del periodo, pero **no muy lejos**. El motor vigila
> el consumo y avisa si una partida se acerca a un cuarto del periodo, porque
> a partir de ahí los ciclos cortos empiezan a sesgar el resultado.

## 3. Distribuciones

### 3.1 Llegadas

Proceso de Poisson **restringido al horario de atención**. La tasa dentro de
la ventana es:

```
λ_ventana = (llegadas_por_dia / 9) · factor_reputación
```

El **factor de reputación** realimenta la demanda:

```
factor = 0.6 + 0.8 · (reputación / 100)
```

Con reputación 70 (inicial) el factor es 1.16, así que llegan ~21 clientes en
vez de 18. Con reputación 20, factor 0.76 → ~14 clientes. Con 0, factor 0.6
→ ~11. **Un taller con mala fama recibe menos clientes**: la espiral
descendente es real y está en el modelo, no en el guion.

### 3.2 Tipo de avería

Categórica con pesos 0.40 / 0.40 / 0.20 (eléctrico, mecánico, carrocería).

### 3.3 Aceptación del presupuesto

**Determinista con ruido**, no una probabilidad fija. El cliente tiene una
**disposición a pagar** que se sortea una vez, al llegar:

```
disposición = precio_justo · (1 + U(-0.25, +0.25))
```

El cliente acepta si `precio_ofertado <= disposición`. Esto **cierra el punto
ciego** del modelo anterior, donde el 15 % de rechazos no tenía explicación:
ahora el rechazo tiene una causa clara y **el jugador la ve** (el cliente
tiene su disposición a pagar en la ficha).

### 3.4 Ruido en el trabajo

El tiempo de servicio es determinista por tipo, con una variación uniforme
de ±10 %:

```
duración = base · (1 + U(-0.10, +0.10))
```

## 4. Eventos

La cola de eventos discretos **se conserva**, con desempate por orden de
inserción (determinismo).

| # | Evento | Qué dispara | Qué hace |
|---|---|---|---|
| E1 | `LLEGADA` | Reloj | Llega un cliente → espera en recepción |
| E2 | `FIN_RECEPCION` | Reloj | El cliente pasó a diagnóstico |
| E3 | `FIN_DIAGNOSTICO` | Reloj | La avería se conoce → espera cotización |
| E4 | `DECISION_CLIENTE` | Reloj | Acepta o rechaza según su disposición |
| E5 | `FIN_ALMACEN` | Reloj | Se consumieron las piezas → espera reparación |
| E6 | `FIN_REPARACION` | Reloj | El vehículo está listo → espera entrega |
| E7 | `FIN_ENTREGA` | Reloj | Se cobra → el cliente se va |
| E8 | `LOTE_PROVEEDOR` | Reloj | Llega el lote de refacciones |
| E9 | `FIN_COTIZACION` | Reloj | El cliente ya sabe el precio y decide |
| E10 | `FIN_DIA` | Reloj | Se cierra el día: nómina, alquiler, cuota |
| E11 | `FIN_PARTIDA` | Reloj | Se acaba el mes |

### 4.1 El cambio de fondo: el jugador inicia el trabajo

En el modelo anterior, el servicio empezaba **solo** cuando un recurso
quedaba libre. En el juego, **el servicio no empieza hasta que alguien lo
empieza**:

| Puesto | Quién lo inicia | Qué pasa mientras tanto |
|---|---|---|
| Recepción | Jugador o recepcionista | El cliente espera en la puerta |
| Diagnóstico | Jugador o mecánico | El vehículo ocupa la bahía, sin avanzar |
| Cotización | Jugador o recepcionista | El cliente espera la decisión |
| Almacén | Jugador o almacenista | El vehículo no puede avanzar |
| Reparación | Jugador o mecánico | El vehículo ocupa la bahía, sin avanzar |
| Entrega | Jugador o recepcionista | No se factura |

**El jugador no puede hacer su trabajo más rápido.** Solo puede decidir
*cuándo*. Ese es el núcleo del diseño de juego y la razón por la que un
empleado contratado (que siempre está disponible) libera al jugador de un
puesto.

### 4.2 Diagrama de estados de un vehículo

```
                    [llegó]
                        │  E1
                        ▼
              [espera_recepcion]
                        │  (jugador: atender)  E2
                        ▼
              [espera_diagnostico]
                        │  (jugador: diagnosticar) E3
                        ▼
              [espera_cotizacion]
                        │  (jugador: cotizar)  E9 → E4
                        ├── (rechaza) ──────────► [se_va]
                        ▼
              [espera_almacen]
                        │  (jugador: tomar pieza)  E5
                        ▼
              [espera_reparacion]
                        │  (jugador: reparar)  E6
                        ▼
              [espera_entrega]
                        │  (jugador: entregar)  E7
                        ▼
                    [entregado]  → ingreso
```

## 5. Estadísticas y partida

### 5.1 Durante la partida

Se acumulan en caliente: entregas, ingresos, costes, tiempos de espera por
puesto, clientes perdidos, y las colas de cada bahía. El HUD muestra en
todo momento:

- Día y hora de juego
- Saldo, ingresos del día, gastos del día
- Cuota del día y cuánto falta
- Reputación
- Colas: recepción, diagnóstico, cotización, almacén, reparación, entrega
- Empleados: cuántos hay en cada puesto

### 5.2 Parte diario

Al cerrar cada día (E10) se calcula el **parte**:

| Concepto | Cómo se calcula |
|---|---|
| Ingresos | Suma de los precios cobrados ese día |
| Gastos | Nómina + alquiler + refacciones + lotes pedidos |
| Saldo | Acumulado |
| Entregas | Trabajos entregados ese día |
| Cuota | ¿Entregas ≥ cuota? |
| Clientes perdidos | Los que se fueron o rechazaron |
| Espera media | Tiempo promedio de espera por puesto |

**El parte es lo que convierte la partida en simulación**: el jugador ve
después de cada día un desglose con el que puede razonar, no solo jugar.

### 5.3 Al final de la partida

Se guarda el **historial de los 30 días** (una fila por día) con todos los
conceptos del parte. Ese historial es el entregable de simulación de la
partida: sirve para comparar dos partidas, para ver el efecto de contratar,
y para el reporte.

## 6. La escala de tiempos del juego

El juego usa **su propia escala de tiempos**, más corta que la del modelo
analítico. La razón es práctica y no de gusto:

- Un turno de 9 h de juego dura unos minutos reales (a velocidad ×1, un día
  de juego son unos 9 minutos; a ×4, unos 2 minutos y cuarto).
- El jugador tiene que **sentir** el reloj sin aburrirse. Ninguna acción
  debe tardar más de ~20 segundos reales.
- Con los tiempos del modelo analítico (2 a 6 h de reparación), un turno de
  9 h no daba ni para 5 trabajos y la cuota era **inalcanzable por
  construcción**: no sería un juego difícil, sería un juego imposible.

| Magnitud | Modelo analítico | Juego |
|---|---|---|
| Diagnóstico (eléctrico / mecánico / carrocería) | 0.5 / 0.8 / 1.2 h | 0.20 / 0.30 / 0.45 h |
| Reparación (eléctrico / mecánico / carrocería) | 2.0 / 3.5 / 6.0 h | 0.80 / 1.30 / 2.20 h |
| Recepción, cotización, almacén, entrega | 0.1 h | 0.10 h |
| Duración del día de juego | 24 h | 9 h de atención |

**Lo que no cambia** es la estructura del modelo: las colas, los recursos
compartidos entre el jugador y los NPCs, el cuello de botella, la relación
L = λW y la realimentación de la reputación sobre la demanda. Lo que cambia
es la escala, que es un parámetro de presentación, no del modelo conceptual.

Por eso la comparación contra M/M/c (sección siguiente) se hace sobre el
**modelo analítico**, no sobre el del juego: con servicio determinista por
tipo de avería y llegadas restringidas a la ventana de atención, el
subsistema de diagnóstico del juego no es un M/M/c exacto y la comparación
sería engañosa.

## 7. La validación del modelo (se conserva)

El modelo de **diagnóstico** sigue siendo comparable con un M/M/c cuando el
servicio es exponencial. En el juego el tiempo de servicio es **determinista
por tipo**, no exponencial, así que la comparación exacta ya no aplica.

**Pero el modelo de llegadas y de espera por puesto sí lo es.** Por eso se
mantiene la verificación: con un escenario de un solo tipo de avería y
servicio exponencial (el modo `validacion`), el motor debe seguir
coincidiendo con la solución analítica dentro del 0.5 %.

> Esta verificación es la que garantiza que **el motor de eventos discretos
> sigue siendo correcto** aunque se le hayan añadido la economía, los NPCs
> y el jugador. Si el motor de fondo se rompió, la partida dejaría de
> reflejar la realidad del taller, y eso es justo lo que la asignatura pide
> evitar.

## 8. Supuestos del modelo matemático

| # | Supuesto | Justificación | Riesgo si es falso |
|---|---|---|---|
| S1 | El tiempo de servicio es determinista por tipo | Hace el juego de triage, no de ritmo | La variabilidad real haría el juego más ruidoso |
| S2 | Las llegadas son Poisson en la ventana | Comportamiento estándar de demanda | Picos reales más altos |
| S3 | La disposición a pagar se sortea una vez por cliente | Un cliente tiene un presupuesto, no lo recalcula | En la realidad puede negociar |
| S4 | El proveedor tarda 24 h y cobra por lote | Simplifica el almacén | El stock real depende del proveedor |
| S5 | La reputación afecta solo a la demanda | Los proveedores no te distinguen | Un buen nombre abre precios |
| S6 | El trabajo continúa aunque el jugador no esté | Es la clave del diseño | — |
| S7 | Los empleados trabajan a velocidad de empleado | Da sentido a contratar | — |
| S8 | El juego no modela calidad de trabajo | Fuera de alcance declarado | Un mal trabajo no tiene castigo |

## Ver también

- [[02-modelo-del-negocio]] — el proceso y los puestos
- [[09-prng-generador]] — el generador
- [[08-reporte]] — resultados y validación
