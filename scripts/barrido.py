#!/usr/bin/env python3
"""Barrido paramétrico para elegir el escenario por defecto.

Usa el CLI (que ya está probado) en vez de código de un solo uso, para que
los números salgan por el mismo camino que usa el usuario.
"""
import json
import re
import subprocess
import sys
from pathlib import Path

RAIZ = Path(__file__).resolve().parent.parent
CLI = RAIZ / "bin" / "simulador"
TMP = Path("/tmp/opencode/sotr-barrido")

BASE = {
    "nombre": "barrido",
    "semilla": 2026,
    "dias": 120,
    "dias_calentamiento": 30,
    "replicas": 15,
    "llegadas_por_dia": 20,
    "bahias_diagnostico": 2,
    "mecanicos": 3,
    "refacciones_stock": 40,
    "refacciones_lote": 40,
    "refacciones_periodo_h": 48,
    "presupuesto_aceptacion": 0.85,
    "presupuesto_decision_h": 0.5,
    "hora_apertura": 8,
    "hora_cierre": 18,
    "tipos": [
        {"peso": 0.40, "media_diagnostico_h": 0.5, "media_reparacion_h": 2.0, "refacciones": 1},
        {"peso": 0.40, "media_diagnostico_h": 0.8, "media_reparacion_h": 3.5, "refacciones": 1},
        {"peso": 0.20, "media_diagnostico_h": 1.2, "media_reparacion_h": 6.0, "refacciones": 2},
    ],
}

RE = re.compile(r"^\s{2}(.+?)\s{2,}(-?[\d.]+)\s+(-?[\d.]+)\s*$")


def correr(cfg):
    TMP.mkdir(parents=True, exist_ok=True)
    ruta = TMP / "esc.json"
    ruta.write_text(json.dumps(cfg), encoding="utf-8")
    salida = subprocess.run(
        [str(CLI), "--escenario", str(ruta), "--tabla"],
        capture_output=True, text=True, check=True).stdout
    datos = {}
    for linea in salida.splitlines():
        m = RE.match(linea)
        if m:
            datos[m.group(1).strip()] = (float(m.group(2)), float(m.group(3)))
    return datos


def main():
    print(f"{'llega':>6} {'vent':>5} {'bahi':>5} {'mec':>4} | "
          f"{'TPR':>7} {'IC':>6} {'ent/d':>7} {'utilR':>7} {'colaR':>7} {'wip':>6}")
    print("-" * 88)
    for llegadas in (10, 14, 18, 22):
        for ventana in (10, 14, 24):
            cfg = dict(BASE)
            cfg.update({
                "llegadas_por_dia": llegadas,
                "hora_apertura": 8,
                "hora_cierre": 8 + ventana,
            })
            d = correr(cfg)
            try:
                tpr, ic = d["TPR medio (h)"]
                ent, _ = d["OT entregadas por día"]
                util, _ = d["Utilización reparación (%)"]
                cola, _ = d["Cola media reparación"]
            except KeyError:
                continue
            print(f"{llegadas:>6} {ventana:>5} {cfg['bahias_diagnostico']:>5} "
                  f"{cfg['mecanicos']:>4} | {tpr:>7.2f} {ic:>6.2f} {ent:>7.2f} "
                  f"{util:>7.1f} {cola:>7.2f}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
