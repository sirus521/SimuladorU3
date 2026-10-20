// api.ts — Cliente de la API del servidor C.
//
// El servidor corre en el mismo origen (lo sirve el propio binario), así
// que no hay CORS ni URLs absolutas en el resto de la app.

import type { Parametros, RespuestaSimulacion } from './tipos';

const BASE = '/api';

export class ErrorApi extends Error {
  constructor(mensaje: string, readonly codigo: number) {
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

/** Escenario por defecto del servidor. */
export function obtenerEscenario(): Promise<Parametros> {
  return pedir<Parametros>('/escenario');
}

/** Corre la simulación con los parámetros dados. */
export function simular(parametros: Parametros): Promise<RespuestaSimulacion> {
  return pedir<RespuestaSimulacion>('/simular', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(parametros),
  });
}
