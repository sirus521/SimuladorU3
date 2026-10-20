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
   implementación exista. El caso áureo del PRNG sale de [[09-prng-generador]]:
   las tablas de 12 filas de las semillas 1 y 2026.
4. **Una fase = un commit.** El mensaje del commit referencia la nota y el
   criterio que se cumplió.
5. **"Hecho" significa:** compila con `-Wall -Wextra` sin warnings, la prueba
   pasa, y la misma semilla reproduce los mismos números.
6. **Sin dependencias nuevas sin ADR.** Cada librería entra con una nota que
   dice por qué y qué alternativa se descartó ([[06-bitacora-y-decisiones]]).
7. **Si el modelo no cuadra con la teoría, se corrige el modelo.** Nunca se
   ajusta un resultado para que se vea bien.
8. **El modelo de negocio vive en `notas/02`.** El código lo implementa. Si
   cambian las reglas, cambia la nota primero.

## Fases

### F0 — Cimientos

**Trabajo:** repo nuevo, `.gitignore`, estructura de carpetas, vault de Obsidian,
notas 00 a 04 (lo que pidió el profesor).

**Aceptación:** las cuatro notas existen y contestan lo pedido; `git status`
limpio tras el commit.

### F1 — PRNG

**Trabajo:** portar el LCG de [[09-prng-generador]] a C, con sus pruebas.

**Aceptación:**
- Las 12 primeras filas de las tablas de las semillas 1 y 2026 se reproducen
  **exactamente**.
- El periodo es 65536 (65536 estados distintos antes de volver al inicio).
- Sobre el ciclo completo: 10922 apariciones de cada cara del dado, 4 rechazos.
- 65536 celdas 2D distintas sobre el ciclo de salidas mezcladas.

### F2 — Distribuciones

**Trabajo:** uniforme, exponencial, Poisson, categórica, Bernoulli, sobre el
PRNG propio.

**Aceptación:** chi-cuadrado p > 0.05 contra la teórica; Kolmogorov-Smirnov
p > 0.05 sobre uniformes; el inverso de la exponencial reproduce la tabla del
PRNG.

### F3 — Motor de eventos discretos

**Trabajo:** cola de prioridad con desempate determinista, y el modelo del
taller ([[02-modelo-del-negocio]]).

**Aceptación:** una corrida de 1 día con **traza manual esperada**, evento por
evento, coincide exactamente. Esa traza es el caso áureo del motor.

### F4 — Estadísticas y validación

**Trabajo:** acumuladores, réplicas, IC 95 %, comparación contra M/M/c.

**Aceptación:** con un escenario de una sola avería (que sí es M/M/c exacto), la
media entre réplicas del TPR y de la longitud de cola caen dentro del intervalo
de confianza de la solución analítica.

### F5 — Servidor HTTP en C

**Trabajo:** sockets POSIX, parseo de la petición, router, servir estáticos.

**Aceptación:** `curl` a cada endpoint devuelve el status y el esquema
esperados; 404 en ruta inexistente; 400 en JSON inválido; body grande no
rompe el servidor.

### F6 — Interfaz web

**Trabajo:** React + Vite + Tailwind, las cinco zonas de [[04-parte-visual]].

**Aceptación:** `npm run build` sin errores; smoke en navegador sin errores de
consola; la interfaz consume el contrato de F5 sin cambios.

### F7 — Reporte y entrega

**Trabajo:** [[08-reporte]], README reproducible, PDF.

**Aceptación:** alguien con el README y una semilla reproduce los mismos
números. El reporte cita los resultados que salen de `make test`.

## Fuera de alcance, por ahora

- Exportar a PDF desde la propia interfaz (se hace con el navegador).
- Comparación entre dos escenarios lado a lado (se hace corriendo dos veces).
- Guardar escenarios del usuario (se edita el JSON).
- Cualquier tipo de persistencia o usuarios.
