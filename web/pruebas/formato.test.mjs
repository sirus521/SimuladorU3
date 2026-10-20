// pruebas/formato.test.mjs — Pruebas de las utilidades de formato.
//
// Son funciones puras, sin React: por eso se pueden probar con node:test
// sin montar nada. node:test viene con Node, así que no hace falta instalar
// ningún framework de pruebas.

import assert from 'node:assert/strict';
import { test } from 'node:test';

import {
  comparacionAceptable,
  esFirme,
  horasATexto,
  instanteATexto,
  margenRelativo,
  metrica,
  numero,
} from '../src/formato.ts';

test('numero formatea con los decimales pedidos', () => {
  assert.equal(numero(9.285, 2), '9.29');
  assert.equal(numero(9.285, 0), '9');
  assert.equal(numero(0, 2), '0.00');
});

test('numero devuelve un guion para valores no finitos', () => {
  assert.equal(numero(NaN), '—');
  assert.equal(numero(Infinity), '—');
});

test('metrica muestra valor e intervalo de confianza', () => {
  const m = { valor: 9.285, ic95: 0.819 };
  const texto = metrica(m, 2);
  assert.ok(texto.includes('±'), 'debe incluir el separador del intervalo');
  assert.ok(texto.includes('9.29'), 'debe incluir el valor');
  assert.ok(texto.includes('0.82'), 'debe incluir el margen');
});

test('margenRelativo es el margen sobre el valor', () => {
  assert.equal(margenRelativo({ valor: 100, ic95: 10 }), 10);
  assert.equal(margenRelativo({ valor: 0, ic95: 10 }), 0);
  assert.equal(margenRelativo({ valor: -50, ic95: 10 }), 20);
});

test('esFirme marca las métricas con mucho margen como inseguras', () => {
  assert.equal(esFirme({ valor: 10, ic95: 1 }), true);
  assert.equal(esFirme({ valor: 10, ic95: 5 }), false);
});

test('horasATexto separa días y horas', () => {
  assert.equal(horasATexto(4.5), '4.5 h');
  assert.equal(horasATexto(50), '2 d 2.0 h');
});

test('instanteATexto convierte a día y hora', () => {
  assert.equal(instanteATexto(0), 'día 1, 00:00');
  assert.equal(instanteATexto(13.5), 'día 1, 13:30');
  assert.equal(instanteATexto(37), 'día 2, 13:00');
});

test('comparacionAceptable usa el margen absoluto', () => {
  assert.equal(comparacionAceptable(0.2, 10), true);
  assert.equal(comparacionAceptable(-4.5, 10), true);
  assert.equal(comparacionAceptable(52, 10), false);
});
