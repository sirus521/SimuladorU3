---
tipo: parte-visual
proyecto: Simulador de Taller de Reparación
estado: aprobado
---

# Parte visual

La interfaz es un **tablero de control**, no un juego. No hay puntaje, ni
avatar, ni niveles. Hay cinco zonas pensadas para responder una pregunta cada
una, en el mismo orden en que se las hace un jefe de taller.

## Zonas de la pantalla

```
┌────────────────────────────────────────────────────────────────────┐
│  SOTR · Simulador de Taller de Reparación      [Escenario ▾] [▶]   │
├────────────────────────────────────────────────────────────────────┤
│  PARÁMETROS                                                        │
│  llegadas/día │ bahías │ mecánicos │ refacciones │ días │ réplicas │
│  semilla                                                             │
├────────────────────────────────────────────────────────────────────┤
│  TPR medio │ TPR p90 │ OT/día │ % rechazo │ % bloqueo │ ociosidad  │
│  (tarjetas de KPI con su intervalo de confianza al 95 %)             │
├──────────────────────────────┬─────────────────────────────────────┤
│  PLANTA DEL TALLER (SVG)      │  COLAS EN EL TIEMPO                 │
│  bahías, mecánicos, almacén   │  línea: diag / rep / refacciones    │
│  con el estado en vivo         │                                     │
├──────────────────────────────┴─────────────────────────────────────┤
│  TPR POR TIPO DE AVERÍA (barras)  │  SIMULADO vs TEÓRICO (barras)    │
├────────────────────────────────────────────────────────────────────┤
│  DISTRIBUCIÓN DEL TPR (histograma)                                  │
└────────────────────────────────────────────────────────────────────┘
```

## Zona por zona

### 1. Barra superior

- **Título y escenario:** muestra qué escenario está cargado. Si el escenario
  viene del JSON de `datos/`, muestra su nombre.
- **Botón ▶ Simular:** dispara la corrida. Se deshabilita mientras corre y
  muestra un estado ("simulando… 30 réplicas").

### 2. Panel de parámetros

Todos los parámetros del modelo, editables antes de correr:

| Control | Rango | Default |
|---|---|---|
| Llegadas por día | 1 – 120 | 24 |
| Bahías de diagnóstico | 1 – 8 | 2 |
| Mecánicos | 1 – 12 | 3 |
| Inventario de refacciones | 0 – 60 | 12 |
| Días de simulación | 1 – 365 | 30 |
| Réplicas | 1 – 200 | 30 |
| Semilla | entero | 2026 |
| Aceptación de presupuesto | 0 – 100 % | 85 % |

Debajo, un botón **"Restablecer"** que vuelve a los valores por defecto, y la
semilla a la vista siempre (para que el resultado sea reproducible).

### 3. Tarjetas de KPI

Seis tarjetas. Cada una muestra el valor grande (media entre réplicas) y debajo
el **intervalo de confianza al 95 %** con formato `valor ± margen`. Si el margen
es grande respecto al valor, se marca en ámbar: significa que la réplicas no
alcanzan a fijar el número y hay que correr más.

| Tarjeta | Formato | Interpretación |
|---|---|---|
| TPR medio | `4.2 h ± 0.1` | Tiempo Promedio de Permanencia |
| TPR p90 | `7.8 h ± 0.4` | El cliente molesto, no el promedio |
| OT/día | `18.4 ± 0.5` | Capacidad real |
| Rechazo | `15.0 %` | Demanda que se pierde |
| Bloqueo refacción | `6.2 %` | ¿El cuello de botella es el almacén? |
| Ociosidad | `23 %` | Capacidad de sobra |

### 4. Planta del taller (SVG)

Un diagrama plano, estilo plano de planta arquitectónica, **no** un juego de
puntaje. Muestra:

- **Bahías de diagnóstico** (2–8 rectángulos): en gris si están libres, en
  ámbar si están ocupadas, con la etiqueta del tipo de avería que se está
  diagnosticando.
- **Mecánicos** (1–12 rectángulos): misma codificación, con el tipo de
  reparación que hacen.
- **Almacén**: una barra que muestra el nivel de inventario y marca en rojo
  cuando hay OTs esperando refacción.
- **Colas**: tres conteos (`diagnóstico: 3 · reparación: 2 · refacciones: 1`).

La planta **no se anima en tiempo real** (no hay streaming de la simulación).
Muestra el **estado promedio** del horizonte y los valores finales. Es una
decisión: animar en vivo es un efecto visual que no aporta información que las
gráficas no den, y complica el contrato de la API.

### 5. Colas en el tiempo

Gráfica de líneas con tres series (cola de diagnóstico, cola de reparación,
cola de refacciones) sobre el eje de tiempo en días. Es la gráfica que revela
**cuándo** se satura el taller, no solo cuánto.

### 6. TPR por tipo de avería

Barras agrupadas: un grupo por tipo (eléctrico / mecánico / carrocería) con la
media y el intervalo de confianza. Responde: ¿el problema es un tipo de trabajo
en particular?

### 7. Simulado vs. teórico

Barras agrupadas, dos por métrica: `TPR` y `longitud media de la cola`,
mostrando el valor de la simulación y el de la fórmula M/M/c. Con el **error
relativo** en percentage entre ambos.

> Si el error relativo se sale del intervalo de confianza, hay un problema en el
> motor o en el modelo. Es la gráfica de control de la validación.

### 8. Histograma del TPR

Distribución del tiempo de permanencia en 20 clases, con la media marcada. Da
la forma de la distribución, que es lo que el supuesto S1 (servicio exponencial)
puede no reproducir bien en la realidad.

## Estados de la interfaz

| Estado | Qué se ve |
|---|---|
| Inicial | Parámetros con defaults, sin datos, un mensaje de "corre la simulación" |
| Simulando | Botón deshabilitado, spinner, texto "simulando 30 réplicas" |
| Con resultados | Todo lo de arriba |
| Error | Mensaje del servidor con el detalle (por ejemplo, parámetros inválidos) |

## Decisiones de diseño

1. **Una sola pantalla.** No hay navegación. Todo lo que se necesita está a la
   vista o a un scroll.
2. **Las cifras siempre con su incertidumbre.** Un número sin IC en simulación
   es propaganda, no un resultado.
3. **El costo de la simulación se muestra**: cuántas réplicas y qué semilla.
4. **Colores con significado fijo:** ámbar = recurso ocupado, rojo = falta
   refacción, gris = libre. Se usan en toda la interfaz.
5. **La interfaz no inventa datos.** Si el servidor no devolvió una serie, el
   gráfico muestra "sin datos" en vez de una línea en cero.
6. **Responsive mínimo:** se usa en una laptop de la escuela. Se adapta a
   columnas en pantallas grandes, no se optimizes para móvil.

## Ver también

- [[01-definicion-del-simulador]] — objetivo y alcance
- [[02-modelo-del-negocio]] — los procesos que se muestran
- [[05-plan-de-trabajo]] — cómo se construye
