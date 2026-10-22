---
tipo: reporte
proyecto: Simulador de Taller de Reparación
estado: aprobado
---

# Reporte: SOTR — Simulador de Taller de Reparación

**Materia:** Simulación · **Unidad:** 3
**Autor:** Sirus521
**Código:** <https://github.com/sirus521/SimuladorU3>

---

## 1. Resumen

Se construyó un **videojuego de simulación en primera persona** de un taller
de reparación. El motor de eventos discretos está escrito en **C** y es
autoritativo: el navegador no simula nada, sólo manda acciones y dibuja el
estado que le devuelve el servidor.

El jugador **ocupa los puestos** del taller —recibe, diagnostica, cotiza,
compone refacciones, repara y entrega— y puede contratar personal que haga lo
mismo. El objetivo es **sobrevivir 30 días cumpliendo una cuota diaria de
trabajos entregados**, sin quebrar y sin perder la reputación.

**El resultado del modelo de fondo** (el motor de simulación clásico, que es
la parte que se valida contra la teoría) para el escenario de referencia:

| Métrica | Valor | IC 95 % |
|---|---|---|
| TPR medio | 9.285 h | ± 0.819 |
| TPR percentil 90 | 18.237 h | ± 1.673 |
| OT entregadas por día | 15.042 | ± 0.258 |
| Presupuestos rechazados | 14.97 % | ± 0.80 |
| Bloqueo por refacciones | 6.77 % | ± 2.21 |
| Utilización diagnóstico | 28.02 % | ± 0.82 |
| Utilización reparación | **69.84 %** | ± 1.92 |

La **reparación** es el cuello de botella, con casi el 70 % de utilización
contra el 28 % del diagnóstico.

**Verificación contra la solución analítica.** En el escenario donde el
subsistema de diagnóstico es un M/M/c exacto, el motor coincide con
Pollaczek-Khinchine con un **error relativo del 0.20 %** en la permanencia
media, 0.7 % en la longitud de la cola y 0.2 % en el número en el sistema.

**El juego es ganable y perdible.** Con el equipo completo (un recepcionista,
tres mecánicos, un almacenista y un cotizador) se ganan los 30 días con 26 a
29 jornadas de cuota cumplida. Sin contratar a nadie se pierde en el día 3, y
con el equipo mínimo en el día 6.

---

## 2. Objetivos y alcance

Detallado en [[01-definicion-del-simulador]]. En resumen: el objetivo es
**sobrevivir 30 días con la cuota diaria cumplida**, y la pregunta que ordena
el juego es **si el cuello de botella del taller son las personas o el
almacén**.

**Dentro del alcance:** los seis puestos jugables en primera persona, el reloj
en tiempo real con pausa, la economía completa (precios, nómina, alquiler,
refacciones), la cuota y la reputación, la contratación de personal con IA por
puesto, el parte diario y la pantalla final, y el panel de análisis con el
motor por lotes y su validación analítica.

**Fuera del alcance:** calidad de la reparación, recompra de clientes,
rotación de personal, llegadas estacionales por hora, dinero del proveedor,
personalización del local, guardado de partida y multijugador.

---

## 3. El modelo

El proceso de negocio está en [[02-modelo-del-negocio]] y el modelo
matemático en [[03-modelo-matematico]].

### 3.1 El cambio de fondo: el trabajo lo inicia alguien

En el motor de análisis, un recurso se ocupa solo en cuanto queda libre. En
el juego **no**: un puesto sólo avanza si el jugador o un empleado lo
inician. De ahí salen la cola, la espera y la presión.

| Puesto | Quién lo inicia | Duración (eléctrico / mecánico / carrocería) |
|---|---|---|
| Recepción | jugador o recepcionista | 0.10 h |
| Diagnóstico | jugador o mecánico | 0.20 / 0.30 / 0.45 h |
| Cotización | jugador o cotizador | 0.10 h |
| Almacén | jugador o almacenista | 0.10 h |
| Reparación | jugador o mecánico | 0.80 / 1.30 / 2.20 h |
| Entrega | jugador o recepcionista/cotizador | 0.10 h |

### 3.2 Aleatoriedad

Todo sale del **PRNG propio**: LCG de 16 bits con periodo máximo 65536 y
mezcla xorshift biyectiva ([[09-prng-generador]]). Cada partida tiene su
semilla, así que **la misma semilla y las mismas acciones reproducen la
partida exactamente**.

El PRNG se verifica contra las tablas de la unidad anterior: las 12 primeras
iteraciones de las semillas 1 y 2026 se reproducen exactamente, y sobre el
ciclo completo cada cara del dado aparece 10 922 veces con 4 rechazos.

### 3.3 La economía

| Concepto | Valor |
|---|---|
| Precios de venta | $6 000 / $11 000 / $18 000 |
| Coste del material | $1 500 / $3 000 / $5 000 |
| Nómina | $2 000 por empleado al día |
| Alquiler | $4 000 al día |
| Lote de refacciones | 20 piezas por $1 500 (llega al abrir del día siguiente) |
| Lote urgente | 20 piezas por $3 000 (llega en 1 hora) |
| Saldo inicial | $25 000 |
| Cuota diaria | 10 trabajos |
| Reputación | 0 a 100; +3 por entrega, −2 por cliente perdido |

El coste del material se imputa **al entregar**, no al tomar la pieza: en una
empresa el material se compra antes y el ingreso se reconoce al entregar, y
cargar la pieza al principio hacía que un día en el que se empezaba mucho
trabajo pero no se entregaba nada dejara al taller sin caja.

La reputación realimenta la demanda: el factor va de 0.60 (reputación 0) a
1.40 (reputación 100). Un taller con mala fama recibe menos clientes, y eso
es lo que hace que perder sea una espiral y no un simple contador.

### 3.4 La escala de tiempos del juego

El juego usa una escala de tiempos **más corta** que el modelo analítico
([[03-modelo-matematico]] § 6). No es una decisión estética: un turno de
9 h de juego dura unos minutos reales, y con los tiempos del modelo
analítico (2 a 6 h de reparación) la cuota de 10 trabajos era inalcanzable
por construcción. La **estructura** del modelo no cambia.

---

## 4. Verificación y validación

### 4.1 Verificación del motor contra la solución analítica

Escenario `validacion-mmc`: abre las 24 h y usa un solo tipo de avería, así
que el subsistema de diagnóstico es un M/M/c exacto.

| Magnitud | Simulado | Teórico M/M/c | Error |
|---|---|---|---|
| Lq (cola) | 0.0711 | 0.0706 | 0.7 % |
| L (sistema) | 0.7053 | 0.7039 | 0.2 % |
| W (permanencia) | 0.8464 h | 0.8447 h | **0.20 %** |

El error está muy por debajo del margen de ruido de la simulación, lo que
confirma que el motor de eventos discretos, la cola de prioridad y los
acumuladores funcionan como deben.

### 4.2 El alcance de la comparación

En el escenario base la comparación contra M/M/c es **aproximada**, y la
interfaz lo dice explícitamente. Hay dos razones: las llegadas se concentran
en la ventana de atención (no son estacionarias) y el servicio es una mezcla
de exponenciales (tres tipos de avería), no una exponencial pura. Por eso el
error aparente en W es de 52 % en el escenario base y de 0.20 % en el de
validación.

### 4.3 Otras pruebas

| Prueba | Resultado |
|---|---|
| PRNG contra las tablas de la unidad anterior | 85 684 comprobaciones, todas en verde |
| Chi-cuadrado y Kolmogorov-Smirnov de las distribuciones | 400 011 comprobaciones |
| Reproducibilidad de la partida | misma semilla y acciones ⇒ partida idéntica |
| Contrato de la API de partida | 22 comprobaciones |
| Balance del juego | ganable con equipo completo, perdible sin él |

**Total: 8 suites, 485 904 comprobaciones, todas en verde.**

---

## 5. Resultados

### 5.1 El modelo por lotes (escenario base)

| Métrica | Valor | IC 95 % |
|---|---|---|
| TPR medio | 9.285 h | ± 0.819 |
| TPR percentil 90 | 18.237 h | ± 1.673 |
| OT entregadas por día | 15.042 | ± 0.258 |
| Presupuestos rechazados | 14.97 % | ± 0.80 |
| Bloqueo por refacciones | 6.77 % | ± 2.21 |
| Ociosidad del taller | 12.74 % | ± 1.34 |
| Utilización diagnóstico | 28.02 % | ± 0.82 |
| Utilización reparación | 69.84 % | ± 1.92 |
| Cola media diagnóstico | 0.365 | ± 0.037 |
| Cola media reparación | 2.315 | ± 0.395 |
| Estado estacionario | sí (deriva 1.00 %) | |

### 5.2 El juego, con partidas completas

| Equipo | Resultado | Días con cuota | Reputación | Saldo final |
|---|---|---|---|---|
| Ninguno | **derrota**: "los clientes se fueron" | 0 | 64 | — |
| Mínimo (1 mecánico) | **derrota**: "los clientes se fueron" | — | 62 | — |
| Base (3 mecánicos) | **victoria**: "sobrevivió al mes" | 26–28 de 30 | 96–100 | ~$2.07 M |
| Ampliado (5 mecánicos) | victoria | 28 de 29 | 100 | ~$1.49 M |

La pérdida con el equipo mínimo no es por falta de producción, sino por
**material**: la demanda de piezas escala con las entregas, y sin reponer el
almacén el taller se para a mitad de partida. Es exactamente la decisión que
el juego quiere que el jugador tome.

### 5.3 El escenario de atasco de refacciones

| Métrica | Valor |
|---|---|
| Bloqueo por refacción | 85.03 % |
| Utilización reparación | 2.21 % |
| TPR medio | 408.7 h |

El resultado más ilustrativo: **el taller está ocioso el 97.8 % del tiempo y
aun así no entrega vehículos**. Mecánicos sin trabajo, vehículos parados y el
cuello de botella íntegramente en el almacén.

---

## 6. La interfaz

Descripción completa en [[04-parte-visual]]. La escena es 3D en primera
persona, con geometría **totalmente procedural**: no hay modelos ni texturas
externas, así que el proyecto no depende de descargas ni tiene temas de
licencias. Se dibuja a 60 fps.

El HUD lleva día y hora, saldo, reputación, cuota, contadores de cola por
puesto, el prompt de acción contextual (`[E] Atender`) y los controles de
velocidad y pausa. Los paneles de contratación, almacén y cotización se
abren con **T**, **B** y al cotizar.

**El servidor manda las reglas económicas** (precios, nómina, plazos) y el
cliente las consume: el navegador no duplica la lógica del negocio, así que
no pueden desincronizarse.

---

## 7. Lo que se encontró por el camino

Tres fallos que sólo aparecieron al construir el juego, y que están
documentados porque son la parte que más cuesta ver:

1. **La fórmula de `Lq` del módulo analítico estaba mal** (le faltaba el
   factor `(c·ρ)^c`). Daba 0.1050 donde Erlang-C da 0.1751: un 74 % de
   diferencia. Lo detectó la comparación con la solución analítica, que es
   la puerta de la fase F4. El motor estaba bien; la fórmula no.
2. **El reparto del trabajo de los mecánicos causaba inanición de cola**:
   con la regla de "atender la cola más larga", los dos mecánicos se iban
   siempre al diagnóstico (su cola siempre era la más larga) y la
   reparación no recibía a nadie.
3. **El día se cerraba dos veces**: al re-encolar un evento futuro se le
   daba un orden nuevo, y el cierre de día ya agendado se duplicaba. La
   nómina se cobraba por duplicado y el taller quebraba el primer día.

Cada uno tiene ahora su prueba en `tests/test_juego_npc.c` y
`tests/test_analitico.c`.

---

## 8. Trabajo futuro

1. **Modelar el precio de forma más fina**: la disposición a pagar del
   cliente es un número; un modelo por tipo de avería y antigüedad del
   vehículo daría más juego.
2. **Servicios de Erlang** en lugar de tiempos deterministas: con ruido
   real, el percentil 90 dejaría de ser tan fácil de acotar.
3. **Audio y realimentación visual** de las esperas: el jugador debería
   *notar* que una cola crece antes de mirar el HUD.
4. **Guardar la partida** en el servidor, para poder continuar al día
   siguiente.
5. **Más tipos de avería** y un modo de juego en el que el jugador elige
   el tipo de taller, con sus consequences en la economía.

---

## 9. Cómo reproducir estos resultados

```bash
git clone https://github.com/sirus521/SimuladorU3
cd SimuladorU3
make && make web && make test

# El modelo por lotes:
./bin/simulador --escenario datos/escenarios/base.json --tabla
./bin/simulador --escenario datos/escenarios/validacion-mmc.json --tabla

# El juego:
make run      # http://127.0.0.1:8080
```

Con la semilla 2026 y 30 réplicas, los números de la sección 5.1 se
reproducen exactamente.

---

## Ver también

- [[01-definicion-del-simulador]] — objetivo y alcance
- [[02-modelo-del-negocio]] — el proceso y la economía
- [[03-modelo-matematico]] — distribuciones, eventos y supuestos
- [[04-parte-visual]] — la escena 3D y el HUD
- [[05-plan-de-trabajo]] — el método y las fases
- [[06-bitacora-y-decisiones]] — las decisiones y los bugs
- [[09-prng-generador]] — el generador de números
