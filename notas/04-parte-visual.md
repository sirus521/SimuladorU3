---
tipo: parte-visual
proyecto: Simulador de Taller de Reparación
estado: borrador
---

# Parte visual

> **Cambio de enfoque.** La interfaz ya no es un panel de gráficas: es una
> **escena 3D en primera persona**. El jugador camina por el taller con WASD
> y el ratón, y hace su trabajo abriendo el puesto donde está. El HUD flota
> encima.

## La escena

Un local rectangular, vista en primera persona, con **geometría procedural**
(sin assets externos: todo se genera con código, así que el proyecto no
depende de descargar modelos ni de licencias de terceros).

```
                        ┌──────────────────────────────────────┐
                        │                                      │
   ┌──────────┐         │   [BAHÍA 1]        [BAHÍA 2]        │
   │ PUERTA   │         │    (coche)          (coche)          │
   │ (llegada)│         │                                      │
   └──────────┘         │                                      │
        │               │                                      │
        ▼               │                                      │
   ┌──────────┐         │                                      │
   │RECEPCIÓN │         │                                      │
   │ (mostrador)       │                                      │
   └──────────┘         │                                      │
                        │                                      │
   ┌──────────┐         │   [BAHÍA 3]        [BAHÍA 4]        │
   │ ALMACÉN  │         │    (coche)          (coche)          │
   │(estantes)│         │                                      │
   └──────────┘         │                                      │
        │               │                                      │
        ▼               │                                      │
   ┌──────────┐         │                                      │
   │COTIZACIÓN│        │                                      │
   │ (mostrador)       │                                      │
   └──────────┘         │                                      │
                        │                                      │
                        │   ┌──────────┐   ┌──────────┐        │
                        │   │BAHÍA 5   │   │BAHÍA 6   │        │
                        │   │ (coche)  │   │ (coche)  │        │
                        │   └──────────┘   └──────────┘        │
                        │                                      │
                        │   ┌────────────────────────────┐     │
                        │   │   SALIDA / DESPACHO         │     │
                        │   └────────────────────────────┘     │
                        └──────────────────────────────────────┘
```

**El plano del taller** tiene tres zonas claramente legibles:

| Zona | Puestos | Por qué está donde está |
|---|---|---|
| **Frente** (entrada) | Recepción, Cotización | El cliente llega y se va por aquí |
| **Lateral** (izquierda) | Almacén | Se supplies desde cualquier punto |
| **Fondo** | Bahías de diagnóstico y reparación | El trabajo largo, al fondo |
| **Salida** | Entrega / despacho | Al fondo derecho, junto a las bahías |

El jugador **ve las bahías desde la entrada**: al empezar el día ve
inmediatamente cuántos coches hay esperando. Esa es la información que
necesita para decidir, y por eso la disposición del plano importa.

## Los puestos y sus interacciones

Cada puesto se reconoce por su **forma y su color**, y tiene un **indicador
flotante** cuando hay algo que hacer:

| Puesto | Cómo se reconoce | Indicador |
|---|---|---|
| Recepción | Mostrador con caja registradora | Cliente esperando: cabeza parpadeante |
| Diagnóstico | Elevador con coche dentro | Vehículo sin revisar: icono de lupa |
| Cotización | Mostrador con una pantalla | Presupuesto sin presentar: icono de documento |
| Almacén | Estanterías con cajas | Faltan piezas: icono de caja roja parpadeante |
| Reparación | Bahía con coche elevado y herramientas | Vehículo sin reparar: icono de llave |
| Entrega | Mostrador de cobro | Vehículo listo: icono de billete |

**Colores con significado fijo**, en toda la interfaz:

| Color | Significado |
|---|---|
| Ámbar | Recurso ocupado (coche en la bahía, mecánico trabajando) |
| Rojo | Falta algo (sin piezas, cola crítica, dinero insuficiente) |
| Verde | Listo para cobrar / trabajo terminado |
| Gris | Libre / inactivo |
| Azul | El jugador está aquí |

## El HUD

Flota en las esquinas de la pantalla, con estilo sobrio (no tapa la acción):

```
┌────────────────────────────────────────────────────────────┐
│ DÍA 7/30   11:24        $ 4 820          ⭐ Rep 68          │  ← arriba
│ ┌──────────────┐                                           │
│ │ CUOTA  8/12  │                            ┌─────────────┐ │
│ │ ████████░░░░ │                            │ [E] Atender │ │  ← centro
│ └──────────────┘                            │  Cliente     │ │    (prompt)
│                                             └─────────────┘ │
│  RECEPCIÓN 2  DIAG 1  COTIZ 0  ALM 0  REP 3  ENT 1        │  ← abajo
└────────────────────────────────────────────────────────────┘
```

| Elemento | Posición | Qué muestra |
|---|---|---|
| Día y hora | arriba izquierda | Día de la partida y hora de juego (avanza en tiempo real) |
| Saldo | arriba centro | Dinero disponible, en rojo si no alcanza para la nómina |
| Reputación | arriba derecha | Barra y número; baja si se van clientes |
| Cuota | izquierda | Barra de progreso del día y cuánto falta |
| **Prompt de acción** | centro inferior | La acción disponible y su tecla: **[E] Atender**, **[E] Diagnosticar**, etc. |
| Colas por puesto | abajo | Un contador por puesto; en rojo si supera 3 |
| Velocidad | abajo derecha | Botones ×1 ×2 ×4 y pausa (Espacio) |

**Regla de la interfaz:** el HUD **nunca muestra un número sin su
contexto**. Si un cliente lleva 40 minutos esperando, el HUD dice "40 min",
no un color rojo sin más.

## La ficha del vehículo

Al acercarse a un vehículo en una bahía, aparece una **ficha flotante** con
la información que el jugador necesita para decidir:

```
┌─────────────────────────────┐
│  🚗  AVERÍA: CARROCERÍA     │
│                             │
│  Reparación:     6.0 h      │
│  Piezas:         3  ($4000) │
│  Precio justo:  $14 000      │
│  Disposición del cliente:    │
│  $15 100  (acepta hasta     │
│           $15 100)          │
│  Esperando:       1.2 h     │
└─────────────────────────────┘
```

Esa ficha es **el corazón de la decisión**: el jugador ve cuánto puede
cobrar, cuánto le cuesta y cuánto ha esperado ya el cliente. Los tres
números juntos son la decisión.

## Los paneles

### Panel de contratación

Se abre con **[T]** o desde un botón. Muestra los puestos disponibles, lo
que cuesta cada uno y lo que **ahorra** al jugador:

```
┌─ CONTRATAR ──────────────────────────────┐
│ Puesto          Nómina/día   Estado       │
│ Recepcionista     $900       1 activo    │
│ Mecánico          $900       2 activos   │
│ Almacenista       $900       —           │
│ Cotizador         $900       —           │
│                                        │
│ Saldo disponible: $4 820                 │
│ [Cancelar]              [Contratar ▸]   │
└────────────────────────────────────────┘
```

Cada puesto muestra **qué cola atendería** el nuevo empleado, que es la
información que de verdad importa: contratar un recepcionista cuando la cola
de recepción es 0 es tirar el dinero.

### Panel de almacén

Se abre con **[B]**. Permite **pedir un lote de refacciones** (10 piezas,
$1 500, llega en 24 h) y ver el stock actual por tipo.

### El parte del día

Al cerrar el día aparece un **resumen a pantalla completa**:

```
┌── DÍA 7 · CIERRE ────────────────────────┐
│                                           │
│  Entregas            9 / 12 cuota   ✗     │
│  Ingresos        $ 68 400                 │
│  Refacciones    −$ 21 000                 │
│  Nómina          −$ 2 700                 │
│  Alquiler        −$ 2 500                 │
│  ─────────────────────────                │
│  Balance del día  $ 42 200                │
│  Saldo            $ 4 820                 │
│                                           │
│  Reputación  68  (−2)                     │
│  Clientes perdidos: 1                     │
│  Espera media en reparación: 2.4 h        │
│                                           │
│  [Continuar ▸]                            │
└───────────────────────────────────────────┘
```

**Este panel es lo que hace que el juego sea simulación y no un juego de
reflejos**: después de cada jornada el jugador ve un desglose con el que
razonar. Si la cola de reparación crece, ve por qué.

## La partida terminada

Al llegar al día 30 (o al perder), una pantalla de resultado:

- **Victoria**: "El taller sobrevivió al mes" con el historial de los 30 días
  como gráfica (entregas por día, saldo acumulado, reputación).
- **Derrota**: el motivo claro (reputación, quiebra, inactividad) y el mismo
  historial, para ver **dónde se rompió**.

Ese historial es el entregable de la partida: es lo que el jugador puede
comparar entre partidas, y lo que alimenta el reporte.

## Estética

- **Iluminación cálida** de interior (luz hemisférica + focos por bahía).
- **Suelo de hormigón** con marcas de.box pintadas.
- **Coches de colores** por tipo de avería (eléctrico = azul, mecánico =
  naranja, carrocería = rojo), para que se distinguan de un vistazo.
- **Sin música ni efectos de sonido** en la primera versión: no hay assets de
  audio y sintetizarlos no aporta al modelo. Se puede añadir después con
  WebAudio.
- **Sin texturas externas**: todo es color plano y geometría. El objetivo es
  que el juego se entienda, no que parezca un simulador de automoción.

## Rendimiento

Objetivo: **60 fps** con el escenario completo (8 coches, 6 NPCs, HUD).

| Regla | Motivo |
|---|---|
| Geometría procedural y compartida | Una geometría por tipo, instancias |
| Sin sombras dinámicas | Una luz direccional basta; las sombras cuestan |
| Render a 60 fps con interpolación | El motor va a 20 Hz; la escena interpola |
| HUD en DOM, no en canvas | El texto se ve nítido y es accesible |

> **Nota técnica aprendida durante la prueba de viabilidad:** en
> React-Three-Fiber v9 hay que declarar `up: [0,1,0]` y `rotation: [0,0,0]`
> explícitamente en la prop `camera`. Si no, la cámara queda mirando al suelo
> y renderiza un cuadro vacío **sin dar ningún error**. Está documentado en
> [[06-bitacora-y-decisiones]].

## Ver también

- [[01-definicion-del-simulador]] — objetivo y alcance
- [[02-modelo-del-negocio]] — los puestos
- [[04-parte-visual]] — la escena 3D y la interfaz
