---
tipo: definicion
proyecto: Simulador de Taller de Reparación
estado: aprobado
---

# Definición del simulador

## Nombre

**Simulador de Operaciones de un Taller de Reparación Automotriz (SOTR)**

Alternativa corta para el título del reporte: **SOTR — Simulador de Taller de Reparación**.

## Objetivo

Estimar, para un taller de reparación de tamaño configurable, **cuánto tarda un
vehículo en salir** y **qué tan saturado está el taller**, de modo que se puedan
evaluar decisiones de capacidad sin tener que contratar más personal ni ampliar
el edificio para cuando la demanda crezca.

En concreto, el simulador responde cuatro preguntas:

1. **Tiempo de permanencia (TPR).** ¿Cuánto tiempo pasa un vehículo dentro del
   taller desde que entra hasta que se lo entrega? (media y percentil 90)
2. **Capacidad.** ¿Cuántas órdenes de trabajo cierra el taller por día?
3. **Saturación.** ¿Qué porcentaje del tiempo están ocupadas las bahías de
   diagnóstico y los mecánicos?
4. **Fugas.** ¿Qué porcentaje de los presupuestos se rechaza y qué porcentaje de
   trabajos se atasca esperando una refacción?

La pregunta que ordena todo lo demás es la cuarta: **¿el cuello de botella es
la gente o son los refacciones?** Esa es la decisión real de un taller, y es la
que el simulador tiene que ayudar a tomar.

## Alcance

### Dentro del alcance

- Proceso de negocio completo de un taller: recepción, diagnóstico, presupuesto,
  espera de refacciones, reparación y entrega.
- Tamaño del taller variable: bahías de diagnóstico, mecánicos, inventario de
  refacciones.
- Demanda variable: llegadas por día y horario de atención.
- Decisión de presupuesto del cliente: acepta o rechaza.
- Métricas de operación con intervalo de confianza al 95 % y **réplicas
  independientes** con semillas distintas.
- Comparación contra la solución analítica de un modelo M/M/c equivalente, como
  **verificación** del motor.
- Interfaz web con tablero de control: planta del taller en vivo, tarjetas de
  KPI, series temporales de colas, histograma de TPR y barras de utilización.

### Fuera del alcance

- **No** modela la calidad del trabajo ni la recompra del cliente. El taller
  siempre entrega bien el trabajo, así que el cliente siempre regresa. Es una
  simplificación deliberada y está anotada en [[03-modelo-matematico]].
- **No** modela la rotación de personal ni turnos. El taller se supone abierto
  un número fijo de horas al día, con los mismos mecánicos.
- **No** modela dinero: ni cotizaciones en pesos, ni facturación, ni utilidad.
  Las métricas son de tiempo y de carga, no económicas.
- **No** modela la llegada estacional por hora. Las llegadas dentro del horario
  son estacionarias; entre días cambia la tasa.
- **No** tiene persistencia en base de datos ni usuarios. Es una herramienta de
  análisis, no un sistema de producción.
- **No** es un juego. No hay puntaje, ni niveles, ni avatar. La parte visual es
  un tablero de control para tomar decisiones, no algo jugable.

### Criterio de aceptación del alcance

> Si una pregunta no se puede responder con las métricas listadas arriba, o si
> necesita dinero, calidad o multiplayer, queda fuera.

## Cómo se usa

1. Se elige un escenario (un archivo JSON en `datos/` o el que trae la interfaz).
2. Se corre la simulación con N réplicas y una semilla explícita.
3. El motor devuelve métricas con su intervalo de confianza.
4. Se comparan escenarios entre sí (por ejemplo, 2 vs 3 mecánicos) para ver qué
   cambio mejora el TPR y a costa de qué.

La misma semilla y el mismo escenario **siempre** dan los mismos números. Eso es
lo que hace que el resultado sea auditable: si un compañero lo corre, obtiene lo
mismo.

## Ver también

- [[02-modelo-del-negocio]] — el proceso paso a paso
- [[03-modelo-matematico]] — las distribuciones y supuestos
- [[04-parte-visual]] — qué se ve en pantalla
- [[05-plan-de-trabajo]] — cómo se construye
