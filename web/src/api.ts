// api.ts — Cliente de la API.
//
// Dos APIs distintas con el mismo servidor:
//   - /api/partida/*  : el juego (tiempo real, el servidor es autoritativo)
//   - /api/simular    : el motor de simulación clásico (análisis por lotes)
//
// El servidor corre en el mismo origen, así que no hay CORS ni URLs
// absolutas.

import type {
  EstadoPartida,
  Parametros,
  RespuestaAccion,
  RespuestaSimulacion,
} from './tipos';

const BASE = '/api';

export class ErrorApi extends Error {
  constructor(
    mensaje: string,
    readonly codigo: number,
  ) {
    super(mensaje);
    this.name = 'ErrorApi';
  }
}

async function pedir<T>(ruta: string, opciones?: RequestInit): Promise<T> {
  let respuesta: Response;
  try {
    respuesta = await fetch(`${BASE}${ruta}`, opciones);
  } catch {
    throw new ErrorApi(
      'No se pudo conectar con el servidor. ¿Está corriendo `make run`?',
      0,
    );
  }

  const texto = await respuesta.text();
  let datos: unknown = null;
  if (texto) {
    try {
      datos = JSON.parse(texto);
    } catch {
      throw new ErrorApi(
        `El servidor devolvió una respuesta que no es JSON (código ${respuesta.status}).`,
        respuesta.status,
      );
    }
  }

  if (!respuesta.ok) {
    const mensaje =
      datos && typeof datos === 'object' && 'error' in datos
        ? String((datos as { error: unknown }).error)
        : `Error ${respuesta.status}`;
    throw new ErrorApi(mensaje, respuesta.status);
  }

  return datos as T;
}

/* ------------------------------------------------------------------ */
/* API de partida                                                       */
/* ------------------------------------------------------------------ */

/** Crea una partida nueva. */
export function iniciarPartida(semilla?: number): Promise<EstadoPartida> {
  return pedir<EstadoPartida>('/partida/iniciar', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(semilla === undefined ? {} : { semilla }),
  });
}

/** Avanza el reloj de la partida. `segundos` son segundos reales. */
export function avanzarPartida(segundos: number): Promise<EstadoPartida> {
  return pedir<EstadoPartida>('/partida/avanzar', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ segundos }),
  });
}

/** Atender el puesto indicado (0..5). */
export function atenderPuesto(puesto: number): Promise<RespuestaAccion> {
  return pedir<RespuestaAccion>('/partida/accion', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ accion: 'atender', puesto }),
  });
}

/** Fija el precio del presupuesto. */
export function cotizar(precio: number): Promise<RespuestaAccion> {
  return pedir<RespuestaAccion>('/partida/accion', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ accion: 'cotizar', precio }),
  });
}

/** Pide un lote de refacciones (llega al abrir del día siguiente). */
export function pedirLote(urgente = false): Promise<RespuestaAccion> {
  return pedir<RespuestaAccion>('/partida/accion', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ accion: 'pedir_lote', urgente }),
  });
}

/** Contrata un empleado (0 = recepcionista, 1 = mecánico, 2 = almacenista, 3 = cotizador). */
export function contratar(tipo: number): Promise<RespuestaAccion> {
  return pedir<RespuestaAccion>('/partida/accion', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ accion: 'contratar', tipo }),
  });
}

/* ------------------------------------------------------------------ */
/* API de análisis                                                      */
/* ------------------------------------------------------------------ */

export function obtenerEscenario(): Promise<Parametros> {
  return pedir<Parametros>('/escenario');
}

export function simular(parametros: Parametros): Promise<RespuestaSimulacion> {
  return pedir<RespuestaSimulacion>('/simular', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(parametros),
  });
}
