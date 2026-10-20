---
tipo: bitacora
proyecto: Simulador de Taller de Reparación
---

# Bitácora y decisiones

Registro de decisiones de diseño (ADR). Cada una dice **qué** se decidió, **por
qué**, y **qué alternativa se descartó**.

---

## ADR-001 — Motor de simulación en C, sin dependencias

**Decisión:** el motor (`src/`) es C17 puro. Solo usa la biblioteca estándar.
Sus únicas dependencias son `libm` (para `log`, `sqrt`, `pow`) y `pthread` no se
usa: el motor es de un solo hilo.

**Por qué:** la materia pide C, y un motor sin I/O ni dependencias se puede
probar con `make test` en cualquier máquina, sin instalar nada. Además el motor
se puede compilar aparte y usar desde el CLI o desde el servidor sin cambios.

**Descartado:** Python + NumPy (más rápido de escribir, pero no es C y no
instala NumPy en esta máquina sin `sudo`); Rust (no es lo que pide la unidad).

---

## ADR-002 — PRNG propio en lugar de `random()` de libc

**Decisión:** se usa el LCG de 16 bits documentado en [[09-prng-generador]],
con su etapa de mezcla xorshift y su método de multiplicación con hueco.

**Por qué:** (a) es trabajo previo de la misma unidad y reutilizarlo es lo que
pide la asignatura; (b) es **verificable**: periodo máximo 65536 comprobable
en microsegundos, uniformidad exacta de un dado comprobable sobre el ciclo
completo; (c) `random()` de libc no garantiza periodo ni uniformidad, y su
semántica de `rand()` está definida de forma distinta en cada implementación.

**Descartado:** `random()` de glibc; xoshiro/PCG (más rápidos, pero rompen la
continuidad con el trabajo previo y no aportan a la nota).

---

## ADR-003 — Servidor HTTP propio en C con sockets POSIX

**Decisión:** el servidor (`servidor/`) implementa HTTP/1.1 sobre sockets POSIX
directly, sin framework.

**Por qué:** en esta máquina **no se puede instalar nada del sistema**
(`sudo` pide password). Las alternativas del sistema (`libmicrohttpd`,
`libcjson`) existen en los repos de Arch pero no se pueden instalar. Escribir
unas 300 líneas de HTTP es más barato que pelear con el sistema.

**Descartado:** `libmicrohttpd` (requiere `sudo`); Flask/Express en Python o
Node (mueve el backend fuera de C, y la unidad pide C).

**Consecuencia asumida:** el parseo HTTP es el punto más frágil del proyecto.
Se mitiga con: parseo estricto y mínimo, límite de tamaño de body, y pruebas de
contrato con `curl` **antes** de tocar la interfaz.

---

## ADR-004 — cJSON vendorizado

**Decisión:** se usa cJSON (MIT) copiado en `tercer-partes/cjson/`, no
instalado del sistema.

**Por qué:** escribir un parser JSON a mano son ~400 líneas donde un error
silencioso produce resultados raros. cJSON es código probado y usado en miles
de proyectos, son dos archivos, y
con la licencia incluida no hay problema de atribución.

**Descartado:** parser propio (riesgo alto); `pacman -S cjson` (requiere
`sudo`).

---

## ADR-005 — Vite + React, no Next.js

**Decisión:** la interfaz es una SPA con Vite 8 + React 19 + Tailwind 4 +
Chart.js.

**Por qué:** el backend ya es C. El SSR de Next.js no aporta nada aquí y
añade un servidor de Node que no queremos. Vite instala en segundos y su
servidor de desarrollo hace proxy al backend C sin configurar CORS.

**Descartado:** Next.js (capa de servidor innecesaria); Svelte/Vue (menos
familiar para este proyecto); React sin Vite (sin bundler ni HMR).

---

## ADR-006 — Un solo proceso sirve API y frontend

**Decisión:** el servidor C sirve también los archivos estáticos de `web/dist/`.

**Por qué:** un comando (`make run`) levanta todo, en un puerto, sin CORS y sin
"¿y ahora cómo lo pruebo?". Para desarrollo, `make dev` levanta Vite (con
proxy) y el servidor C por separado.

**Descartado:** dos servidores siempre (más piezas que mover); servir el
frontend desde otro servidor (CORS y dos procesos).

---

## ADR-007 — El dominio es un taller de reparación

**Decisión:** el proceso modelado es un taller de reparación (recepción →
diagnóstico → presupuesto → refacciones → reparación → entrega).

**Por qué:** es una **red de colas con contraparte analítica exacta** (M/M/c y
Little). Eso permite *validar* el simulador, que es lo que distingue un
trabajo de Simulación de un ejercicio de programación. Con un solo tipo de
avería el subsistema de diagnóstico es M/M/c exacto y la comparación analítica
es concluyente.

**Descartado:** agronomía (riego, clima, estrés hídrico). Es visualmente
atractivo pero sus variables dependen de datos reales que no se tienen, así que
no se puede validar contra una fórmula cerrada: quedaría en "dibujar
intuitivamente". La arquitectura es agnóstica al dominio, así que esta decisión
es reversible mientras no se escriba `src/modelo.c`.

---

## ADR-008 — Métricas siempre con intervalo de confianza

**Decisión:** ninguna cifra se publica sola. Toda métrica va con su IC 95 %,
calculado sobre 30 réplicas con semillas distintas.

**Por qué:** una sola corrida es **una muestra** de un proceso estocástico, no
un resultado. Publicar un número sin incertidumbre es lo primero que un
profesor de Simulación objeta.

**Descartado:** correr una vez y reportar (es más rápido, y es incorrecto).

---

## ADR-009 — Métricas en horas, no en días ni en minutos

**Decisión:** todo el motor trabaja en horas con punto flotante.

**Por qué:** los datos del negocio (diagnóstico de 30 min, reparación de 3.5 h)
están en horas y minutos; convertir a días obligaría a multiplicar por 24 en
cada línea. La conversión a días se hace solo al presentar.

**Descartado:** minutos (obliga a dividir entre 60 en las medias); días (no
divide bien).

---

## ADR-010 — Un solo PRNG por réplica, sembrado al inicio

**Decisión:** cada réplica crea su propio generador con `semilla_base + i`, y
toda la aleatoriedad de esa réplica sale de ahí.

**Por qué:** garantiza que las réplicas sean **independientes** (requisito para
el intervalo de confianza) y que una réplica sea reproducible por separado.

**Descartado:** un único generador para todas las réplicas (las réplicas
quedan correlacionadas y el IC es incorrecto); un generador por tipo de
distribución (rompe la secuencia, y con un LCG de periodo 65536 eso acortaría el
periodo efectivo de forma difícil de auditar).

---

## ADR-011 — Periodo de calentamiento configurable

**Decisión:** el motor descarta los primeros `dias_calentamiento` días (10 por
defecto) antes de acumular estadísticas, series y conteos.

**Por qué:** el taller arranca vacío y la cola se llena durante los primeros
días. Sin este descarte, el TPR medido **crece con el horizonte** (16.6 h a
30 días, 26.7 h a 60, 38.1 h a 90, 62.9 h a 180), no porque el taller
empeore sino porque cada horizonte más largo incluye más días de cola
acumulada. Publicar un número que depende del horizonte, sin decirlo, es
publicar un número que no significa nada.

**Descartado:** no medir nada hasta el final y reportar sólo la última
semana (complica el contrato y no explica el por qué); aumentar el horizonte
por defecto para "diluir" el transitorio (no lo elimina, sólo lo esconde).

---

## ADR-012 — Diagnóstico de estado estacionario

**Decisión:** cada corrida compara el TPR de la primera y la segunda mitad del
periodo medido. Si difieren más de 15 %, se marca `en_estacionario = 0` y la
interfaz avisa de que el TPR sigue creciendo con el horizonte.

**Por qué:** es la diferencia entre "el taller está saturado" y "el taller
tiene una cola larga". En el escenario saturado la deriva es de 56.8 % y en
el de atasco de refacciones de 62.3 %: en los dos casos, un número de TPR
publicado sin más contexto es engañoso. En el escenario base es de 1.0 %.

**Descartado:** cortarlo todo cuando `ρ ≥ 1` (la detección es del subsistema
de diagnóstico, pero el atasco puede estar en el almacén o en la reparación, y
ahí ρ del diagnóstico es bajo).

---

## ADR-013 — Las métricas se devuelven como `{valor, ic95}`

**Decisión:** la API devuelve cada métrica como un objeto con `valor` e
`ic95`, en lugar de dos campos planos (`tpr_medio` y `tpr_medio_ic95`).

**Por qué:** con campos planos es fácil leer un número sin ver su intervalo al
lado, y el frontend terminó esperando una forma que el servidor no enviaba
(los valores salían como `—` y `NaN`). Con el objeto, es imposible separar
la cifra de su incertidumbre, y el contrato queda auto-descriptivo.

**Descartado:** campos planos con sufijo `_ic95` (era lo que había, y
exactamente por eso se rompió); dejar la transformación en el frontend
(oculta la discrepancia en vez de eliminarla).

---

## ADR-014 — Escenario de validación aparte

**Decisión:** el paquete incluye `datos/escenarios/validacion-mmc.json`, que
abre las 24 h y usa un solo tipo de avería, para que el subsistema de
diagnóstico sea un M/M/c exacto.

**Por qué:** en el escenario base la comparación contra la fórmula tiene un
error aparente de 52 % en W, y eso **no es un error del motor**: las llegadas
se concentran en 9 h (no son estacionarias) y el servicio es una mezcla de
exponenciales. Sin un escenario donde la comparación sea exacta, no hay forma
de separar "el modelo se parece a un M/M/c" de "el motor está bien". En el
escenario de validación el error es de 0.20 %.

**Descartado:** quitar la comparación analítica del escenario base (se pierde
la única señal de sanity check que se tiene a diario); reportar el error de
52 % sin explicar por qué (inducIRía a pensar que el motor está mal).
