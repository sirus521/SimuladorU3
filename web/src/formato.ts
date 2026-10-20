// formato.ts — Utilidades de formato, puras y testeables.
//
// Aquí vive toda la lógica que no necesita React, para poder probarla con
// node:test sin montar componentes.

import type { Metrica } from './tipos';

/** Formatea un número con los decimales indicados. */
export function numero(valor: number, decimales = 2): string {
  if (!Number.isFinite(valor)) return '—';
  return valor.toLocaleString('es-MX', {
    minimumFractionDigits: decimales,
    maximumFractionDigits: decimales,
  });
}

/** Formatea una métrica con su intervalo de confianza: "9.29 ± 0.82". */
export function metrica(m: Metrica, decimales = 2): string {
  if (!Number.isFinite(m.valor)) return '—';
  return `${numero(m.valor, decimales)} ± ${numero(m.ic95, decimales)}`;
}

/**
 * El margen de confianza en porcentaje respecto al valor. Sirve para
 * marcar en ámbar las métricas cuya replicated no fija bien el número.
 */
export function margenRelativo(m: Metrica): number {
  if (!Number.isFinite(m.valor) || m.valor === 0) return 0;
  return (m.ic95 / Math.abs(m.valor)) * 100;
}

/** Una métrica es "firme" si su margen relativo no pasa del 20 %. */
export function esFirme(m: Metrica, limite = 20): boolean {
  return margenRelativo(m) <= limite;
}

/** Convierte horas a un texto de "3 días 4 h". */
export function horasATexto(horas: number): string {
  if (!Number.isFinite(horas)) return '—';
  const dias = Math.floor(horas / 24);
  const resto = horas - dias * 24;
  if (dias === 0) return `${numero(resto, 1)} h`;
  return `${dias} d ${numero(resto, 1)} h`;
}

/** Convierte la hora de simulación (horas desde el inicio) a "día 3, 14:00". */
export function instanteATexto(horas: number): string {
  if (!Number.isFinite(horas)) return '—';
  const dia = Math.floor(horas / 24) + 1;
  const dentro = horas % 24;
  const h = Math.floor(dentro);
  const m = Math.floor((dentro - h) * 60);
  return `día ${dia}, ${String(h).padStart(2, '0')}:${String(m).padStart(2, '0')}`;
}

/**
 * Una comparación contra la fórmula analítica se considera correcta si el
 * error relativo no supera el margen dado.
 */
export function comparacionAceptable(errorPct: number, margen = 10): boolean {
  return Math.abs(errorPct) <= margen;
}
