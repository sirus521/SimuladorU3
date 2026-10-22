---
tipo: plan
proyecto: Simulador de Taller de Reparación
estado: vigente
---

# Plan de trabajo

## Principio: por qué no es vibecoding

El riesgo real de un proyecto de simulación no es que el código no compile. Es
que **el código compile y el modelo esté mal**. Un simulador con un error en
una suposición produce números muy precisos que no significan nada, y eso es
peor que no tener simulador.

Por eso el trabajo va en fases, y **una fase no empieza hasta que la anterior
cumple su criterio de aceptación**. El orden no es negociable.

## Las ocho reglas

1. **Especificación antes de código.** Una función no se escribe hasta que su
   comportamiento está escrito en `notas/` y tiene criterio de aceptación.
2. **Contrato de datos primero.** El JSON de request/response y los tipos de
   TypeScript se escriben y se congelan antes del primer endpoint.
3. **Prueba primero.** Cada módulo tiene una prueba que falla antes de que la
   implementación exista. El caso áureo del PRNG sale de
   [[09-prng-generador]]: las tablas de 12 filas de las semillas 1 y 2026.
4. **Una fase = un commit.** El mensaje del commit referencia la nota y el
   criterio que se cumplió.
5. **"Hecho" significa:** compila con `-Wall -Wextra -Wpedantic -Wshadow
   -Wconversion` sin warnings, la prueba pasa, y la misma semilla reproduce
   los mismos números.
6. **Sin dependencias nuevas sin ADR.** Cada librería entra con una nota que
   dice por qué y qué alternativa se descartó ([[06-bitacora-y-decisiones]]).
7. **Si el modelo no cuadra con la teoría, se corrige el modelo.** Nunca se
   ajusta un resultado para que se vea bien.
8. **El modelo de negocio vive en `notas/02`.** El código lo implementa. Si
   cambian las reglas, cambia la nota primero.

## Fases

### G0 — Documento de diseño del juego

**Trabajo:** reescribir las notas 01–04 como documento de diseño: nombre,
objetivo y condiciones de victoria/derrota, el proceso de negocio, el modelo
matemático, y la parte visual.

**Aceptación:** las cuatro notas describen **el juego**, no un panel de
análisis.

### G1 — Motor extraíble

**Trabajo:** extraer las colas (min-heap de prioridad y FIFO) a un módulo
compartido, para que el motor de análisis y el de juego no dupliquen esa
lógica.

**Aceptación:** ambas capas compilan contra las mismas colas; las pruebas
previas siguen en verde.

### G2 — Capa de juego

**Trabajo:** el motor de partida. El cambio de fondo: **el trabajo ya no
empieza solo**. Un puesto sólo avanza si el jugador o un empleado lo
inician. Con economía completa (precios, nómina, alquiler, refacciones),
cuota diaria y reputación que realimenta la demanda.

**Aceptación:** sin jugador ni empleados, las colas crecen y no se entrega
nada. Con empleados, se entrega y se factura.

### G3 — API de partida

**Trabajo:** `POST /api/partida/{iniciar,accion,avanzar}` y
`GET /api/partida`. El servidor es autoritativo.

**Aceptación:** contrato verificado con pruebas; partida iniciable y
accionable por HTTP; el estado de la partida no se corrompe bajo peticiones
concurrentes.

### G4 — Escena 3D

**Trabajo:** la escena del taller en primera persona, con geometría
procedural (sin assets externos), controles WASD + ratón, y las piezas del
taller: mostradores, estantería, bahías, coches por tipo de avería, NPCs.

**Aceptación:** renderiza a 60 fps; se camina, se mira y se interactúa.

### G5 — Bucle de juego completo

**Trabajo:** el recorrido recepción → diagnóstico → cotización → almacén →
reparación → entrega, con el HUD (día, hora, saldo, reputación, cuota,
colas, prompt de acción) y los paneles de contratación, almacén y
cotización.

**Aceptación:** una partida se juega de principio a fin.

### G6 — NPCs, contratación y balance

**Trabajo:** los empleados ocupan los puestos con su propia IA, y la
contratación es una decisión de juego con coste.

**Aceptación:** la partida es **ganable** con un equipo razonable y
**perdible** sin él. Las dos propiedades, con sus pruebas.

### G7 — Reporte y entrega

**Trabajo:** el reporte con modelo, supuestos, validación y resultados, y el
README reproducible.

**Aceptación:** alguien con el README y una semilla reproduce los mismos
números.

## Fuera de alcance, por ahora

- Exportar a PDF desde la propia interfaz.
- Comparación lado a lado de dos partidas.
- Guardar la partida entre sesiones.
- Sonido y efectos.
- Personalización del local.
