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
| F0 | Cimientos, repo, vault de Obsidian, notas 00–04 | ✅ |
| F1 | PRNG: 12 filas de cada tabla áurea, periodo 65536, dado uniforme | ✅ |
| F2 | Distribuciones: chi-cuadrado y Kolmogorov-Smirnov | ✅ |
| F3 | Motor de eventos discretos: reproducibilidad e invariantes | ✅ |
| F4 | Estadísticas y validación contra M/M/c (error 0.20 %) | ✅ |
| F5 | Servidor HTTP: contrato de la API verificado con `curl` | ✅ |
| F6 | Interfaz web: build limpio, 8 pruebas, smoke en navegador | ✅ |
| F7 | Reporte y entrega | ✅ |

**Pruebas:** 1 356 797 comprobaciones en C, todas en verde.
**Resultados:** ver [[08-reporte]].

## Cómo se trabaja aquí

- Las notas son la **especificación**: el código las implementa, no al revés.
- Cada fase tiene un criterio de aceptación y termina en un commit.
- Los números del reporte se citan de `make test` y del CLI, no de memoria.
- Ver [[05-plan-de-trabajo]] para el método y [[06-bitacora-y-decisiones]]
  para las decisiones.
