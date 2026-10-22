---
tipo: definicion
proyecto: Simulador de Taller de Reparación
estado: borrador
---

# Definición del simulador

> **Nota de alcance.** Este proyecto empezó como un panel de análisis y se
> reorientó como **videojuego de simulación**, que es lo que pidió la
> asignatura. Esta nota y las tres siguientes son el documento de diseño del
> juego. El historial del cambio está en [[06-bitacora-y-decisiones]].

## Nombre

**SOTR — Taller de Reparación**

Nombre corto para menciones: **SOTR**. Para la portada del reporte:
*"SOTR: Simulador, videojuego de un taller de reparación"*.

## Qué es

Un **videojuego de simulación en primera persona** donde el jugador dirige un
taller de reparación. El jugador **ocupa los puestos**: recibe a los clientes,
diagnostica averías, presenta presupuestos, compra refacciones, repara y
entrega los vehículos. Contratar personal permite delegar puestos y pasar a
ocuparse de lo que más apriete.

El reloj corre en tiempo real (un día de juego dura unos minutos reales) y se
puede pausar. La simulación que hay debajo no es un adorno: **es un modelo de
eventos discretos en C** que decide cuándo llega un cliente, cuánto tarda una
reparación y qué clientes se cansan de esperar. El jugador no compite contra
el azar, compite contra el modelo.

## Objetivo

**Sobrevivir 30 días de taller cumpliendo la cuota diaria de trabajos
entregados, sin quebrar y sin perder la reputación del negocio.**

En concreto, el juego pide tres cosas a la vez:

| Objetivo | Cómo se mide | Qué pasa si falla |
|---|---|---|
| **Cumplir la cuota** | Trabajos entregados por día ≥ cuota | Cada día incumplido baja la reputación |
| **No quebrar** | El saldo del taller sigue en positivo | Si el saldo queda negativo, se acabó |
| **Mantener la reputación** | Reputación > 0 | Si llega a cero, se acabou |

La tensión está en que los tres compiten entre sí: contratar personal ayuda
a la cuota pero cuesta nómina, y subir el precio de los presupuestos mejora
el saldo pero espanta clientes.

## Condiciones de victoria y derrota

| | Condición | Aviso en pantalla |
|---|---|---|
| **Victoria** | Llegar al día 30 con la cuota cumplida, saldo positivo y reputación > 0 | "El taller sobrevive al mes" |
| **Derrota por reputación** | La reputación llega a 0 | "El taller perdió su fama" |
| **Derrota por quiebra** | El saldo queda negativo al cerrar un día | "El taller quebró" |
| **Derrota por inactividad** | 3 días seguidos sin cumplir la cuota | "Los clientes se fueron" |

## Alcance

### Dentro del alcance

- **Los 6 puestos del taller**, jugables en primera persona: recepción,
  diagnóstico, cotización, almacén, reparación y entrega.
- **Reloj de juego en tiempo real** con pausa, y turnos de día y noche.
- **Economía completa**: precios de reparación, coste de refacciones, nómina
  diaria, alquiler del local, saldo y flujo de caja.
- **Contratación de personal** que ocupa los puestos por ti, con IA por
  puesto.
- **Cuota diaria** y **reputación** que reaccionan a tu desempeño.
- **Tres tipos de avería** con tiempos, costes y precios distintos.
- **Estadísticas de partida**: al final de cada día se ve el parte (entregas,
  ingresos, tiempos de espera, clientes perdidos).
- **El modelo de eventos discretos en C** como motor autoritativo, con su
  PRNG propio y su validación analítica.

### Fuera del alcance

- **No** es un juego de acción: no hay disparos, ni enemigos, ni combate. La
  presión viene del reloj y de las colas.
- **No** hay modo multijugador ni multijugador online.
- **No** hay guardado de partida en la nube: la partida vive en memoria del
  servidor mientras dura la sesión.
- **No** se modela la **calidad de la reparación**: el trabajo se entrega
  bien siempre. La consecuencia de un mal trabajo (cliente que no vuelve) no
  está modelada.
- **No** se modela la **reputación como recurso de la cadena de suministro**:
  los proveedores no te tratan distinto por tu fama.
- **No** hay personalización de taller (paredes, colores, música) más allá de
  lo funcional.
- **No** se modelan **modelos de vehículo ni marcas**: todos los clientes
  llegan con una avería de un tipo, sin preferencia de marca.

### Criterio de aceptación del alcance

> Si una función del juego no se puede sentir en menos de 30 segundos de
> juego —porque el jugador ve la consecuencia en la cola, en el dinero o en
> la reputación— no entra.

## Cómo se juega (resumen)

```
   CLIENTE LLEGA
        │
        ▼
   [TÚ · RECEPCIÓN]  registrar la entrada
        │
        ▼
   [TÚ · DIAGNÓSTICO]  revisar y determinar la avería
        │
        ▼
   [TÚ · COTIZACIÓN]  poner precio → el cliente acepta o se va
        │
        ├── no acepta ──► el cliente se va (reputación −)
        ▼
   [TÚ · ALMACÉN]  tomar o comprar refacciones
        │
        ▼
   [TÚ · REPARACIÓN]  reparar
        │
        ▼
   [TÚ · ENTREGA]  cobrar y despachar
```

Cuando contratas gente, cada NPC ejecuta ese mismo flujo en su puesto. Tú
liberas ese puesto y puedes atender otro.

## Ver también

- [[02-modelo-del-negocio]] — el proceso y los puestos
- [[03-modelo-matematico]] — las distribuciones y el modelo
- [[04-parte-visual]] — la escena 3D y la interfaz
- [[05-plan-de-trabajo]] — las fases
