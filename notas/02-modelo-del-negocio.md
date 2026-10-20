---
tipo: modelo-negocio
proyecto: Simulador de Taller de Reparación
estado: aprobado
---

# Modelo del negocio

## Actores

| Actor | Qué quiere | Qué hace en el modelo |
|---|---|---|
| **Cliente** | Salir rápido y pagar lo justo | Llega con el vehículo, aprueba o rechaza el presupuesto, se lleva el vehículo |
| **Recepcionista** | Registrar todo rápido | Genera la orden de trabajo (OT) |
| **Mecánico de diagnóstico** | Detectar la avería | Revisa el vehículo, arma el presupuesto, pide refacciones |
| **Mecánico de reparación** | Trabajar sin esperas | Ejecuta la reparación |
| **Almacén** | Que no falten refacciones | Repone inventario cada cierto tiempo |

En la simulación el recepcionista **no es un recurso con cola**: el registro de
la OT es inmediato. Los dos roles de mecánico **sí** son recursos con cola, y son
la parte interesante del modelo.

## Recursos (colas)

| Recurso | Cantidad | Se ocupa en | Cola que genera |
|---|---|---|---|
| Bahías de diagnóstico | configurable (por defecto 2) | Diagnóstico | `cola_diagnostico` |
| Mecánicos de reparación | configurable (por defecto 3) | Reparación | `cola_reparacion` |
| Inventario de refacciones | unidades (por defecto 12) | Al consumes | `cola_refacciones` |

## Proceso, paso a paso

```
LLEGADA
   │  (el cliente llega con el vehículo)
   ▼
RECEPCIÓN ──► se genera la OT, se elige el tipo de avería
   │            (probabilidades fijas: eléctrico 40 %,
   │             mecánico 40 %, carrocería 20 %)
   ▼
DIAGNÓSTICO ──► ocupa una bahía (cola si no hay bahía libre)
   │             duración ~ Exponencial(media del tipo)
   ▼
PRESUPUESTO ──► el cliente lo evalúa durante un tiempo
   │             y decide: acepta (p = 0.85) o rechaza
   │
   ├── RECHAZA ──► el vehículo se va. OT cerrada como "no atendido".
   │
   ▼ (acepta)
REPARACIÓN
   │
   ├── ¿Hay refacciones en inventario?
   │      NO ──► ESPERA DE REFACCIONES (cola)
   │              │  se consume inventario al reponer
   │              ▼
   │      SÍ ──► se consumen las refacciones
   ▼
OCUPAR MECÁNICO ──► cola si no hay mecánico libre
   │                 duración ~ Exponencial(media del tipo)
   ▼
ENTREGA ──► el vehículo sale. TPR registrado. OT cerrada como "atendida".
```

## Puntos donde el modelo puede "atascarse"

Estos son los tres cuellos de botella posibles, y son los que el tablero de
control tiene que hacer visibles:

1. **Bahías de diagnóstico saturadas** → el vehículo espera antes de que
   nadie lo revise. Empuja el TPR para arriba.
2. **Mecánicos saturados** → la OT espera "en el taller" sin avanzar.
3. **Falta de refacciones** → la OT está en el taller, ya pagada, pero no se
   puede avanzar. Es el atasco más caro: se ocupa una bahía o un lugar en la
   cola sin producir nada.

## Reglas de negocio codificadas

| Regla | Valor por defecto | Razón |
|---|---|---|
| Llegadas por día | 18 | Poisson no estacionario dentro del horario |
| Horario de atención | 09:00 – 18:00 (9 h) | Turno único de lunes a sábado |
| Bahías de diagnóstico | 2 | Un espacio por bayeta de inspección |
| Mecánicos | 3 | Tamaño pequeño de taller |
| Inventario de refacciones | 40 unidades | Reposición cada 48 h, lote de 40 |
| Aceptación de presupuesto | 85 % | El resto se va sin contratar el servicio |
| Periodo de calentamiento | 10 días | Se descartan: el taller arranca vacío |
| Distribución de averías | 40/40/20 | Tres tipos con tiempos distintos |
| Piezas por trabajo | 1 / 1 / 2 | Consumo ≈ 20 piezas/día |

## Métricas que produce

| Métrica | Definición | Por qué importa |
|---|---|---|
| `tpr_medio` | Promedio de permanencia (llegada → entrega) | La promesa que se le hace al cliente |
| `tpr_p90` | Percentil 90 del TPR | El cliente molesto, no el promedio |
| `throughput_diario` | OT entregadas por día | Capacidad real del taller |
| `util_diagnostico` | % del tiempo con bahías ocupadas | ¿Faltan bahías? |
| `util_reparacion` | % del tiempo con mecánicos ocupados | ¿Faltan mecánicos? |
| `pct_rechazo` | % de presupuestos rechazados | Demanda que el taller pierde |
| `pct_bloqueo_refaccion` | % de OT que esperan refacción | ¿El problema es el almacén? |
| `cola_diagnostico` / `cola_reparacion` | Longitud de cola en el tiempo | Señal temprana de saturación |
| `pct_ociosidad` | % del tiempo con el taller vacío | ¿Hay capacidad de sobra? |

## Supuestos del negocio

1. **El cliente siempre regresa si su vehículo se reparó bien.** No modelamos
   la recompra ni la fuga de clientes por mala calidad.
2. **El presupuesto se decide en un tiempo corto y el cliente no se negocia.**
   Se modela como una espera exponencial + una decisión binaria.
3. **Las refacciones se piden por unidades iguales.** No hay surtidos ni
   proveedores con plazo distinto.
4. **La reposición es automática y periódica**, no disparada por demanda. Es una
   simplificación: en un taller real alguien avisa cuando algo baja del mínimo.
5. **El horario solo restringe las llegadas.** Los trabajos que ya entraron
   siguen avanzando aunque cierre el taller. Esto evita un modelo con dos
   regímenes de reloj y mantiene la tasa de servicio constante.

## Ver también

- [[01-definicion-del-simulador]] — nombre, objetivo, alcance
- [[03-modelo-matematico]] — las distribuciones concretas
- [[04-parte-visual]] — cómo se ve esto en pantalla
