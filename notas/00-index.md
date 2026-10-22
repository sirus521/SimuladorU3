---
tipo: indice
proyecto: Simulador de Taller de Reparación
materia: Simulación
unidad: 3
repositorio: https://github.com/sirus521/SimuladorU3
---

# Simulador de Taller de Reparación

Tablero de control del proyecto. Cada nota responde una pregunta concreta y
está numerada para leerse en orden.

**Repositorio:** <https://github.com/sirus521/SimuladorU3>
**En el vault:** `TecNM/SimuladorU3/` (enlace simbólico al repo)

## Documentos que pide el profesor

| Nota | Qué responde |
|---|---|
| [[01-definicion-del-simulador]] | Nombre, objetivo y alcance del simulador |
| [[02-modelo-del-negocio]] | Procesos, actores, recursos y reglas del taller |
| [[03-modelo-matematico]] | Distribuciones, eventos, transiciones y supuestos |
| [[04-parte-visual]] | Qué se ve en pantalla y cómo se ve |

## Método y decisiones

| Nota | Qué contiene |
|---|---|
| [[05-plan-de-trabajo]] | Fases, criterios de aceptación y protocolo anti-vibecoding |
| [[06-bitacora-y-decisiones]] | Registro de decisiones (ADR) con sus alternativas descartadas |
| [[07-flujo-con-obsidian]] | Cómo se trabaja este repo desde Obsidian |
| [[08-reporte]] | Reporte final: modelo, supuestos, validación y resultados |

## Referencia técnica

| Nota | Qué contiene |
|---|---|
| [[09-prng-generador]] | Especificación del LCG propio que usa el motor |

## Código

| Ruta | Qué hay |
|---|---|
| `src/` | Motor de simulación en C (sin I/O) |
| `servidor/` | Servidor HTTP en C |
| `cli/` | Binario de línea de comandos |
| `tests/` | Pruebas en C |
| `datos/` | Escenarios de ejemplo en JSON |
| `web/` | Interfaz (React + Vite + Tailwind) |

## Estado actual

Todas las fases cerradas. Criterios cumplidos:

| Fase | Criterio | Estado |
|---|---|---|
| G0 | Documento de diseño del juego (notas 01–04) | ✅ |
| G1 | Motor extraíble: las colas compartidas por ambos motores | ✅ |
| G2 | Capa de juego: el trabajo lo inicia alguien, con economía | ✅ |
| G3 | API de partida autoritativa, con candado | ✅ |
| G4 | Escena 3D en primera persona, 60 fps | ✅ |
| G5 | Bucle de juego completo y parte del día | ✅ |
| G6 | NPCs, contratación y balance del juego | ✅ |
| G7 | Reporte y entrega | ✅ |

**Pruebas:** 8 suites, 485 904 comprobaciones, todas en verde.
**Validación:** error del 0.20 % contra la solución M/M/c.
**Resultados:** ver [[08-reporte]].

## Las dos vistas

| Vista | Qué es | Para qué |
|---|---|---|
| **Juego** | Simulación en primera persona, con el jugador en los puestos | La entrega de la asignatura |
| **Análisis** | El mismo motor por lotes, con Intervalos de confianza | Validar el modelo contra la teoría y alimentar el reporte |

Las dos comparten el motor de eventos discretos en C: el del juego es el
mismo con las decisiones del jugador, y el del análisis es el mismo sin
jugador.

## Cómo se trabaja aquí

- Las notas son la **especificación**: el código las implementa, no al revés.
- Cada fase tiene un criterio de aceptación y termina en un commit.
- Los números del reporte se citan de `make test` y del CLI, no de memoria.
- Ver [[05-plan-de-trabajo]] para el método y [[06-bitacora-y-decisiones]]
  para las decisiones.
