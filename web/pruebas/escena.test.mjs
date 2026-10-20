// pruebas/escena.test.mjs — Pruebas de la geometría y las constantes de la
// escena.
//
// Son funciones puras: se prueban sin montar React ni abrir un navegador.

import assert from 'node:assert/strict';
import { test } from 'node:test';

import {
  COLOR,
  COLOR_COCHE,
  COLA_PUESTO,
  LOCAL,
  NOMBRE_PUESTO,
  POSICION_BAHIA,
  POSICION_PUESTO,
  PUESTOS,
  ACCION_PUESTO,
} from '../src/escena.ts';

test('los seis puestos están completos', () => {
  assert.equal(PUESTOS.length, 6);
  for (const p of PUESTOS) {
    assert.ok(NOMBRE_PUESTO[p], `falta el nombre de ${p}`);
    assert.ok(ACCION_PUESTO[p], `falta la acción de ${p}`);
    assert.ok(POSICION_PUESTO[p], `falta la posición de ${p}`);
    assert.ok(COLA_PUESTO[p], `falta la cola de ${p}`);
  }
});

test('los puestos están dentro del local', () => {
  const limiteX = LOCAL.ancho / 2 - 0.6;
  const limiteZ = LOCAL.fondo / 2 - 0.6;
  for (const p of PUESTOS) {
    const pos = POSICION_PUESTO[p];
    assert.ok(Math.abs(pos.x) <= limiteX, `${p} está fuera por x`);
    assert.ok(Math.abs(pos.z) <= limiteZ, `${p} está fuera por z`);
  }
});

test('las bahías no se superponen entre sí', () => {
  for (const esDiag of [true, false]) {
    const puntos = [];
    for (let i = 0; i < 4; i++) puntos.push(POSICION_BAHIA(i, esDiag));
    for (let i = 0; i < puntos.length; i++) {
      for (let j = i + 1; j < puntos.length; j++) {
        const d = Math.hypot(puntos[i].x - puntos[j].x, puntos[i].z - puntos[j].z);
        assert.ok(d > 3, `dos bahías se pisan (distancia ${d.toFixed(2)})`);
      }
    }
  }
});

test('el diagnóstico y la reparación están en zonas distintas', () => {
  const d = POSICION_BAHIA(0, true);
  const r = POSICION_BAHIA(0, false);
  const distancia = Math.hypot(d.x - r.x, d.z - r.z);
  assert.ok(distancia > 3, 'las bahías de diagnóstico y reparación se pisan');
});

test('hay un color por cada tipo de avería', () => {
  assert.equal(COLOR_COCHE.length, 3);
  const distintos = new Set(COLOR_COCHE);
  assert.equal(distintos.size, 3, 'dos averías comparten color');
});

test('los colores con significado son distintos entre sí', () => {
  const clave = [
    COLOR.ocupado,
    COLOR.libre,
    COLOR.mecanico,
    COLOR.falta,
    COLOR.listo,
    COLOR.jugador,
  ];
  assert.equal(new Set(clave).size, clave.length, 'dos estados comparten color');
});
