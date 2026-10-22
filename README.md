# SOTR — Simulador de Taller de Reparación

**Videojuego de simulación en primera persona** de un taller de reparación.
El motor de eventos discretos está escrito en **C** y es autoritativo: el
navegador no simula nada, sólo manda acciones y dibuja el estado que le
devuelve el servidor.

> **Materia:** Simulación · **Unidad:** 3
> **Documentación:** las notas de Obsidian están en [`notas/`](notas/00-index.md)

---

## Qué se hace aquí

El jugador **ocupa los puestos** del taller —recibe, diagnostica, cotiza,
compone refacciones, repara y entrega— y puede contratar personal que haga lo
mismo. El objetivo es **sobrevivir 30 días cumpliendo una cuota diaria de
trabajos entregados**, sin quebrar y sin perder la reputación.

| | |
|---|---|
| **Motor** | C17, eventos discretos, sin dependencias |
| **Servidor** | HTTP/1.1 sobre sockets POSIX, un hilo por conexión |
| **Interfaz** | React 19 + Three.js (R3F) + Tailwind 4, 60 fps |
| **Aleatoriedad** | PRNG propio: LCG de 16 bits, periodo máximo 65536 |

## Dos vistas

| Vista | Qué es | Para qué |
|---|---|---|
| **Juego** | Primera persona, con el jugador en los puestos | La entrega |
| **Análisis** | El mismo motor por lotes, con intervalos de confianza | Validar el modelo contra la teoría |

Ambas comparten el motor de eventos discretos: el del juego es el mismo con
las decisiones del jugador; el del análisis es el mismo sin jugador.

## Correr

```bash
make            # compila el motor en C y el servidor
make web        # instala dependencias y compila la interfaz
make run        # levanta todo en http://127.0.0.1:8080
```

Para desarrollar la interfaz con recarga en caliente:

```bash
make dev                          # terminal 1: servidor C en :8080
cd web && npm run dev             # terminal 2: Vite en :5173 (proxy a :8080)
```

## Probar

```bash
make test       # pruebas en C: PRNG, distribuciones, motor, analítico, HTTP, juego
make test-web   # pruebas de la interfaz (node:test)
```

| Suite | Qué cubre |
|---|---|
| `test_prng` | Las tablas áurea del PRNG, el periodo y la uniformidad del dado |
| `test_distrib` | Chi-cuadrado y Kolmogorov-Smirnov de las distribuciones |
| `test_analitico` | La solución M/M/c contra Erlang-C y contra M/M/1 |
| `test_modelo` | Reproducibilidad e invariantes del motor de análisis |
| `test_http` | El contrato de la API y los casos borde del servidor |
| `test_juego` | El motor de partida: reloj, trabajo, economía |
| `test_juego_npc` | Los NPCs, el reparto del trabajo y el balance del juego |
| `test_juego_http` | El contrato de la API de partida |

Salida de `make test`:

```
== Pruebas del simulador ==
-- build/test_analitico   OK  analitico (25 comprobaciones)
-- build/test_distrib     OK  distrib (400011 comprobaciones)
-- build/test_http        OK  http (42 comprobaciones)
-- build/test_juego       OK  juego (53 comprobaciones)
-- build/test_juego_http  OK  juego-http (22 comprobaciones)
-- build/test_juego_npc   OK  juego-npc (32 comprobaciones)
-- build/test_modelo      OK  modelo (35 comprobaciones)
-- build/test_prng        OK  prng (85684 comprobaciones)
== Todas las pruebas en C pasaron ==
```

## Controles del juego

| Tecla | Qué hace |
|---|---|
| **W A S D** | Caminar |
| **Ratón** | Mirar (clic en la escena para capturar el puntero) |
| **E** | Atender el puesto donde estás · cotizar |
| **B** | Panel del almacén (pedir refacciones) |
| **T** | Panel de contratación |
| **Espacio** | Pausar |
| **1 2 4** | Velocidad ×1, ×2, ×4 |

## El CLI

```bash
./bin/simulador --escenario datos/escenarios/base.json --tabla
./bin/simulador --defecto --replicas 100 --semilla 42 --tabla
./bin/simulador --escenario datos/escenarios/base.json --salida resultado.json
```

## Escenarios incluidos

| Archivo | Para qué sirve |
|---|---|
| `base.json` | El escenario de referencia del modelo por lotes |
| `validacion-mmc.json` | **Verificación**: abre 24 h y usa un solo tipo de avería, así que el diagnóstico es un M/M/c exacto y se compara con la fórmula (error 0.20 %) |
| `saturado.json` | Taller sobrecargado: la cola crece sin límite |
| `atasco-refacciones.json` | El almacén es el cuello de botella: el taller está ocioso y aun así no entrega |

## Estructura

```
notas/            Documentación en Obsidian (diseño, modelo, reporte)
include/sim/      Cabeceras públicas del motor
src/              Motor en C: PRNG, distribuciones, analítico, modelo, juego
servidor/         Servidor HTTP en C y rutas de la API
cli/              Interfaz de línea de comandos
tests/            Pruebas en C
datos/escenarios/ Escenarios en JSON
web/              Interfaz (React + R3F + Tailwind)
tercer-partes/    cJSON (MIT), vendorizado
```

## Reproducibilidad

Misma semilla + mismas acciones ⇒ **los mismos números**, bit a bit. El
generador es propio (LCG de 16 bits con periodo máximo 65536, documentado en
[`notas/09-prng-generador.md`](notas/09-prng-generador.md)) y se verifica
contra las tablas de la unidad anterior.

## Dependencias

- **C:** compilador C17 y `libm`. Nada más. cJSON va vendorizado.
- **Web:** Node 20+ y npm. `npm install` en `web/`.

No hace falta instalar nada del sistema: el proyecto compila y corre sin
`sudo`.

## Documentación

Empieza por [`notas/00-index.md`](notas/00-index.md). Ahí está el mapa del
proyecto y el estado de cada fase.
