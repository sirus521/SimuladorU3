---
tipo: modelo-negocio
proyecto: Simulador de Taller de Reparación
estado: borrador
---

# Modelo del negocio

## La idea que ordena todo

El taller es una **cadena de seis puestos**. Un cliente recorre la cadena
completa, y en cada puesto hay que hacer una cosa. La tensión del juego no
está en ninguna de esas seis cosas: está en que **el jugador solo puede estar
en un puesto a la vez**.

Ese es el corazón del diseño. Un puesto mal atendido no se rompe de golpe:
se llena la cola del siguiente, y el cliente que espera demasiado se va.

## Los seis puestos

| # | Puesto | Qué haces | Cuánto tarda | Qué pasa si tardas |
|---|---|---|---|---|
| 1 | **Recepción** | Atender al cliente que llega y registrar su vehículo | 0.1 h | El cliente espera en la puerta y se va si tarda mucho |
| 2 | **Diagnóstico** | Revisar el vehículo y determinar la avería | 0.5 / 0.8 / 1.2 h (según tipo) | Nadie puede cotizar hasta que lo revisas |
| 3 | **Cotización** | Presentar el presupuesto y el cliente decide | 0.2 h | El cliente se va sin contratar |
| 4 | **Almacén** | Tomar o comprar las refacciones | 0.1 h | La reparación no empieza |
| 5 | **Reparación** | Arreglar el vehículo | 2.0 / 3.5 / 6.0 h (según tipo) | El vehículo no se entrega |
| 6 | **Entrega** | Cobrar y despachar el vehículo | 0.1 h | No facturas: pierdes el ingreso |

**Dato clave:** los tiempos de los puestos 2 y 5 **no dependen de ti**. El
diagnóstico de un problema eléctrico siempre tarda 30 minutos. Lo que
depende de ti es **cuándo te pones a hacerlo**. Esa es la diferencia entre
un juego de simulación y un juego de ritmo.

## Actores

| Actor | Quién es | Qué quiere | Qué hace |
|---|---|---|---|
| **Cliente** | Persona con un vehículo roto | Salir rápido y pagar lo justo | Llega, espera, acepta o rechaza el presupuesto, recibe el vehículo |
| **Tú** | El jugador | Sobrevivir el mes | Ocupas los puestos, pones precios, contratas gente, compras refacciones |
| **Empleado** | NPC que contratas | Cobrar su nómina | Hace los puestos que le asignes, con su propia lentitud |
| **Proveedor** | No se ve | Cobrar las refacciones | Entrega el lote de piezas pasado el plazo |

## Recursos y colas

| Recurso | Cuántos | Lo ocupa | Cola que genera |
|---|---|---|---|
| **Bahías de diagnóstico** | 2 (ampliable) | Un vehículo en revisión | `cola_diagnostico` |
| **Bahías de reparación** | 3 (ampliable) | Un vehículo en arreglo | `cola_reparacion` |
| **Almacén de refacciones** | 40 unidades | Un vehículo esperando pieza | `cola_refacciones` |
| **Tú** | 1 | El puesto en el que estés | — |
| **Empleados** | 0 al inicio | Su puesto asignado | — |

Cuando un empleado ocupa un puesto, ese puesto ya no lo ocupas tú. Los
recursos son **compartidos entre el jugador y los NPCs**, que es lo que hace
que contratar tenga un coste real: no es "más capacidad", es "el jugador deja
de tener que estar ahí".

## Proceso completo, paso a paso

```
LLEGADA DEL CLIENTE
   │  (el cliente aparece en la puerta)
   ▼
ESPERA EN RECEPCIÓN ─── el jugador o un recepcionista lo atiende
   │                       │
   │                       └─ si espera más de X, se va (reputación −)
   ▼
DIAGNÓSTICO ────────── se determina el tipo de avería
   │                   (el jugador puede equivocarse: el trabajo sale mal)
   ▼
COTIZACIÓN ─────────── el jugador pone un precio
   │                   │
   │                   ├─ el cliente acepta  → sigue
   │                   └─ el cliente rechaza → se va (no hay ingreso,
   │                                   pero tampoco coste)
   ▼
ALMACÉN ────────────── se consumen refacciones
   │                   │
   │                   └─ no hay stock → el vehículo espera
   ▼                      hasta que llegue el lote
REPARACIÓN ─────────── se repara el vehículo
   │
   ▼
ENTREGA ────────────── el cliente paga y se va
                       (ingreso = precio cobrado)
```

## La avería: tres tipos

El tipo de avería determina **cuánto tarda, cuánto cuesta arreglar y cuánto
pagarán por ello**.

| Tipo | Peso | Diagnóstico | Reparación | Coste de piezas | Precio de venta |
|---|---|---|---|---|---|
| **Eléctrico** | 40 % | 0.5 h | 2.0 h | $1 200 | $4 000 |
| **Mecánico** | 40 % | 0.8 h | 3.5 h | $2 500 | $8 500 |
| **Carrocería** | 20 % | 1.2 h | 6.0 h | $4 000 | $14 000 |

Los tres tienen el mismo margen bruto, pero la carrocería **ocupa el triple de
tiempo de bahía** que la eléctrica. Por eso la mezcla de averías importa
mucho: un día con mucha carrocería te hunde, aunque el número de clientes sea
el mismo.

## Economía

| Concepto | Valor | Nota |
|---|---|---|
| **Ingreso** | Precio de venta del presupuesto que aceptó el cliente | Se cobra en la entrega |
| **Coste de refacciones** | $1 200 / $2 500 / $4 000 según tipo | Se paga al tomar la pieza del almacén |
| **Nómina** | $900 por empleado por día | Se paga al cerrar cada día |
| **Alquiler** | $2 500 por día | Fijo, se paga al cerrar cada día |
| **Precio de la pieza** | $1 500 por lote de 10 | Se paga al pedir el lote |
| **Plazo del proveedor** | 24 h | Las piezas tardan un día en llegar |
| **Saldo inicial** | $8 000 | Para arrancar |

**El flujo de caja diario es el que mata o salva la partida.** Con 15
trabajos entregados al día a $8 500 de media, entran $127 500; con 3
mecánicos y el alquiler salen $5 200. La partida se gana sobrada… **si llegas
a entregar 15 trabajos**. Si la cola se atasca y entregas 8, la nómina te
ahoga. Esa es la tensión: **el dinero no depende de facturar mucho, depende
de facturar a tiempo**.

## La cuota y la reputación

| Regla | Valor |
|---|---|
| **Cuota diaria** | 12 trabajos entregados por día (ajustable) |
| **Reputación** | 0 a 100, empieza en 70 |
| **Reputación +2** | por cada trabajo entregado |
| **Reputación −5** | por cada cliente que se va (rechaza o se cansa de esperar) |
| **Reputación +1** | por cada día con la cuota cumplida |

La reputación tiene un efecto **en el modelo**: con más reputación llegan más
clientes. Es realimentación positiva —un taller bien reputado crece— y
negativa cuando se deteriora: si cae, llegan menos clientes, y es más difícil
recuperarse. Esa espiral es el mecanismo de derrota más lento.

## Reglas de negocio codificadas

| Regla | Valor | Razón |
|---|---|---|
| Llegadas por día | 18 (× factor de reputación) | Demanda base del taller |
| Horario de atención | 09:00 – 18:00 (9 h) | Turno único, 6 días a la semana |
| Bahías de diagnóstico | 2 | Tamaño inicial del taller |
| Bahías de reparación | 3 | Tamaño inicial del taller |
| Inventario inicial | 40 unidades | ~1.5 días de consumo |
| Plazo del proveedor | 24 h | Las piezas no son instantáneas |
| Aceptación base del presupuesto | 85 % | Acepta si el precio es razonable |
| Tolerancia del cliente | ±25 % sobre el precio justo | Si te pasas, se va |
| Cuota diaria | 12 trabajos | Objetivo de la partida |
| Días de la partida | 30 | Un mes de operación |

## Supuestos del negocio

1. **El jugador es un empleado más**: hace el trabajo, no solo lo dirige.
   Es la decisión de diseño que define el juego.
2. **Los tiempos de servicio son fijos por tipo de avería.** No hay varianza
   de ejecución: el jugador sabe que una carrocería son 6 horas.
3. **El cliente que rechaza el presupuesto no vuelve.** No hay mercado de
   segunda mano.
4. **El precio justo es conocido por el jugador** (aparece en la ficha de la
   avería). No adivinar: el juego no es de adivinar números.
5. **Los proveedores no tienen stock infinito** y cobran por lote.
6. **La reputación afecta la demanda**, pero no los precios ni los plazos.
7. **Un empleado hace su trabajo a velocidad de empleado**: un poco más
   lento que el jugador, que es la razón por la que contratar tiene sentido
   (tú haces dos cosas a la vez, ellos no).

## Ver también

- [[01-definicion-del-simulador]] — objetivo y alcance
- [[03-modelo-matematico]] — las distribuciones
- [[04-parte-visual]] — la escena 3D
