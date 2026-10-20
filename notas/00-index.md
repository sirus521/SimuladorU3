---
tipo: indice
proyecto: Simulador de Taller de Reparación
materia: Simulación
unidad: 3
---

# Simulador de Taller de Reparación

Tablero de control del proyecto. Cada nota responde una pregunta concreta y
está numerada para leerse en orden.

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

- [x] F0 — Cimientos, repo, vault de Obsidian
- [x] F1 — PRNG (LCG propio) con prueba áurea
- [x] F2 — Distribuciones de probabilidad
- [x] F3 — Motor de eventos discretos + modelo del taller
- [x] F4 — Estadísticas, réplicas y validación contra teoría
- [x] F5 — Servidor HTTP en C
- [x] F6 — Interfaz web
- [x] F7 — Reporte y entrega
