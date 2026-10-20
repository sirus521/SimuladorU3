# SOTR — Simulador de Taller de Reparación

Simulador de eventos discretos de un taller de reparación. El motor de
simulación está escrito en **C** y la interfaz es una aplicación **web**
(React + Vite + Tailwind) que consume la API del propio servidor en C.

> **Materia:** Simulación · **Unidad:** 3
> **Documentación:** las notas de Obsidian están en [`notas/`](notas/00-index.md)

---

## Qué responde

Para un taller de tamaño configurable, estima:

| Pregunta | Métrica |
|---|---|
| ¿Cuánto tarda un vehículo en salir? | TPR medio y percentil 90 |
| ¿Cuánto cierro al día? | OT entregadas por día |
| ¿Estoy saturado? | Utilización de bahías y mecánicos |
| ¿El cuello de botella es la gente o el almacén? | % de bloqueo por refacciones |

Cada cifra se publica **con su intervalo de confianza al 95 %**, sobre 30
réplicas independientes. Una simulación sin incertidumbre no es un resultado,
es una cifra con suerte.

---

## Correr

```bash
make            # compila el motor en C y el servidor
make web        # instala dependencias y compila la interfaz
make run        # levanta todo en http://127.0.0.1:8080
```

Para desarrollar la interfaz con recarga en caliente:

```bash
make dev        # terminal 1: servidor C en :8080
cd web && npm run dev   # terminal 2: Vite en :5173 (hace proxy a /8080)
```

## Probar

```bash
make test       # pruebas en C (PRNG, distribuciones, modelo, analítico, HTTP)
make test-web   # pruebas de la interfaz (node:test)
```

Salida esperada de `make test`:

```
== Pruebas del simulador ==
-- build/test_analitico    OK  analitico (25 comprobaciones)
-- build/test_distrib      OK  distrib (400011 comprobaciones)
-- build/test_http         OK  http (40 comprobaciones)
-- build/test_modelo       OK  modelo (35 comprobaciones)
-- build/test_prng         OK  prng (85684 comprobaciones)
== Todas las pruebas en C pasaron ==
```

## Usar el CLI

```bash
./bin/simulador --escenario datos/escenarios/base.json --tabla
./bin/simulador --defecto --replicas 100 --semilla 42 --tabla
./bin/simulador --escenario datos/escenarios/base.json --salida resultado.json
```

## Escenarios incluidos

| Archivo | Para qué sirve |
|---|---|
| `base.json` | El escenario de referencia: 18 vehículos/día, 2 bahías, 3 mecánicos |
| `validacion-mmc.json` | **Verificación del motor**: abre 24 h y usa un solo tipo de avería, así que el subsistema de diagnóstico es un M/M/c exacto y se puede comparar con la fórmula |
| `saturado.json` | Taller sobrecargado: sirve para ver qué pasa cuando la demanda supera la capacidad |
| `atasco-refacciones.json` | El almacén es el cuello de botella |

---

## Estructura

```
notas/            Documentación en Obsidian (especificación, modelo, reporte)
include/sim/      Cabeceras públicas del motor
src/              Motor de simulación en C (sin I/O, sin dependencias)
servidor/         Servidor HTTP en C (sockets POSIX) + rutas de la API
cli/              Interfaz de línea de comandos
tests/            Pruebas en C
datos/escenarios/ Escenarios en JSON
web/              Interfaz (React + Vite + Tailwind + Chart.js)
tercer-partes/    cJSON (MIT), vendorizado
scripts/          Utilidades (barrido paramétrico)
```

## La API

| Método | Ruta | Qué hace |
|---|---|---|
| GET | `/api/salud` | Estado del servicio |
| GET | `/api/escenario` | Escenario por defecto |
| GET | `/api/escenarios` | Lista los escenarios disponibles |
| POST | `/api/simular` | Corre la simulación y devuelve los resultados |

El cuerpo de `POST /api/simular` acepta los mismos campos que produce
`GET /api/escenario`. Los campos ausentes conservan su valor por defecto.

Cada métrica se devuelve como un objeto `{valor, ic95}`, de modo que es
imposible leer un número sin ver su incertidumbre al lado.

---

## Reproducibilidad

Misma semilla + mismos parámetros ⇒ **los mismos números**, bit a bit. El
generador es propio (LCG de 16 bits con periodo máximo 65536, documentado en
[`notas/09-prng-generador.md`](notas/09-prng-generador.md)) y las réplicas usan
semillas distintas pero derivadas de la base, de forma que el resultado
completo es auditable.

El PRNG se verifica contra las tablas de la unidad anterior: las 12 primeras
iteraciones de las semillas 1 y 2026 se reproducen exactamente.

## Dependencias

- **C:** compilador C17 y `libm`. Nada más. cJSON va vendorizado en
  `tercer-partes/`.
- **Web:** Node 20+ y npm. `npm install` en `web/`.

No hace falta instalar nada del sistema: el proyecto compila y corre sin
`sudo`.

## Documentación

Empieza por [`notas/00-index.md`](notas/00-index.md). Ahí está el mapa de todo
el proyecto, con el índice de notas y el estado de cada fase.
