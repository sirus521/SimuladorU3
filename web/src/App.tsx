// App.tsx — Tablero de control del simulador.

import { useCallback, useEffect, useState } from 'react';

import { ErrorApi, obtenerEscenario, simular } from './api';
import {
  GraficaColas,
  GraficaHistograma,
  GraficaPorTipo,
  GraficaValidacion,
  Planta,
  Tarjeta,
} from './componentes';
import { esFirme, horasATexto, metrica, numero } from './formato';
import type { Parametros, RespuestaSimulacion } from './tipos';

const NOMBRES_TIPO = ['Eléctrico', 'Mecánico', 'Carrocería'];

export default function App() {
  const [parametros, setParametros] = useState<Parametros | null>(null);
  const [resultado, setResultado] = useState<RespuestaSimulacion | null>(null);
  const [cargando, setCargando] = useState(false);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    obtenerEscenario()
      .then(setParametros)
      .catch((e: unknown) =>
        setError(e instanceof ErrorApi ? e.message : 'No se pudo cargar el escenario.'),
      );
  }, []);

  const correr = useCallback(async () => {
    if (!parametros) return;
    setCargando(true);
    setError(null);
    try {
      setResultado(await simular(parametros));
    } catch (e: unknown) {
      setError(e instanceof ErrorApi ? e.message : 'Falló la simulación.');
      setResultado(null);
    } finally {
      setCargando(false);
    }
  }, [parametros]);

  const cambiar = useCallback(
    <K extends keyof Parametros>(clave: K, valor: Parametros[K]) => {
      setParametros((p) => (p ? { ...p, [clave]: valor } : p));
    },
    [],
  );

  if (error && !parametros) {
    return (
      <main className="mx-auto max-w-5xl p-6">
        <div className="rounded-lg border border-red-200 bg-red-50 p-4">
          <h1 className="font-semibold text-red-800">No se pudo conectar con el simulador</h1>
          <p className="mt-1 text-sm text-red-700">{error}</p>
          <p className="mt-3 text-sm text-slate-600">
            Arranca el servidor con <code className="rounded bg-slate-200 px-1">make run</code>{' '}
            y recarga la página.
          </p>
        </div>
      </main>
    );
  }

  const r = resultado;
  const resumen = r?.resumen;

  return (
    <main className="mx-auto max-w-6xl space-y-4 p-4 md:p-6">
      <header className="flex flex-wrap items-center justify-between gap-3">
        <div>
          <h1 className="text-xl font-bold text-slate-900">
            SOTR · Simulador de Taller de Reparación
          </h1>
          <p className="text-sm text-slate-500">
            Simulación de eventos discretos · backend en C · {r?.replicas ?? 30} réplicas
            {r ? ` · semilla ${r.semilla_base}` : ''}
          </p>
        </div>
        <button
          onClick={correr}
          disabled={cargando || !parametros}
          className="rounded-md bg-slate-900 px-4 py-2 text-sm font-medium text-white transition hover:bg-slate-700 disabled:opacity-50"
        >
          {cargando ? 'Simulando…' : 'Simular'}
        </button>
      </header>

      {error && (
        <div className="rounded-lg border border-red-200 bg-red-50 p-3 text-sm text-red-700">
          {error}
        </div>
      )}

      {r?.avisos && (
        <div className="rounded-lg border border-amber-200 bg-amber-50 p-3 text-sm text-amber-800">
          {r.avisos}
        </div>
      )}

      {parametros && (
        <PanelParametros p={parametros} cambiar={cambiar} />
      )}

      {!resultado && !cargando && (
        <p className="rounded-lg border border-dashed border-slate-300 p-10 text-center text-sm text-slate-500">
          Ajusta los parámetros y pulsa <strong>Simular</strong> para ver los resultados.
        </p>
      )}

      {r && resumen && (
        <>
          {r.validacion.en_estacionario === 0 && (
            <div className="rounded-lg border border-amber-300 bg-amber-50 p-3 text-sm text-amber-900">
              <strong>El taller está saturado.</strong> El TPR sigue creciendo con el
              horizonte (deriva de {numero(r.validacion.deriva_tpr_pct, 1)} % entre la
              primera y la segunda mitad del periodo). El número publicado depende de
              cuántos días se simularon: no describe un régimen estable.
            </div>
          )}

          <section className="grid grid-cols-2 gap-3 md:grid-cols-3 lg:grid-cols-6">
            <Tarjeta
              titulo="TPR medio"
              valor={numero(resumen.tpr_medio.valor, 2)}
              unidad="h"
              detalle={`IC 95 %: ${numero(resumen.tpr_medio.ic95, 2)} h`}
              firme={esFirme(resumen.tpr_medio)}
            />
            <Tarjeta
              titulo="TPR percentil 90"
              valor={numero(resumen.tpr_p90.valor, 1)}
              unidad="h"
              detalle={horasATexto(resumen.tpr_p90.valor)}
              firme={esFirme(resumen.tpr_p90)}
            />
            <Tarjeta
              titulo="OT por día"
              valor={numero(resumen.throughput_diario.valor, 2)}
              detalle={`IC 95 %: ${numero(resumen.throughput_diario.ic95, 2)}`}
              firme={esFirme(resumen.throughput_diario)}
            />
            <Tarjeta
              titulo="Rechazo"
              valor={numero(resumen.pct_rechazo.valor, 1)}
              unidad="%"
              detalle="presupuestos no aceptados"
              firme={esFirme(resumen.pct_rechazo)}
            />
            <Tarjeta
              titulo="Bloqueo refacciones"
              valor={numero(resumen.pct_bloqueo_refaccion.valor, 1)}
              unidad="%"
              detalle="OTs esperando pieza"
              firme={esFirme(resumen.pct_bloqueo_refaccion)}
            />
            <Tarjeta
              titulo="Ociosidad"
              valor={numero(resumen.pct_ociosidad.valor, 1)}
              unidad="%"
              detalle="tiempo con el taller vacío"
              firme={esFirme(resumen.pct_ociosidad)}
            />
          </section>

          <section className="grid gap-3 lg:grid-cols-2">
            <Planta
              bahias={r.parametros.bahias_diagnostico}
              mecanicos={r.parametros.mecanicos}
              ocupacionBahi={Math.round(
                (resumen.util_diagnostico.valor / 100) * r.parametros.bahias_diagnostico,
              )}
              ocupacionMec={Math.round(
                (resumen.util_reparacion.valor / 100) * r.parametros.mecanicos,
              )}
              stockRefacciones={r.parametros.refacciones_stock}
              capacidadRefacciones={r.parametros.refacciones_stock}
              esperas={{
                diagnostico: Math.round(resumen.cola_media_diagnostico.valor),
                reparacion: Math.round(resumen.cola_media_reparacion.valor),
                refacciones: 0,
              }}
            />
            <GraficaColas series={r.series} />
          </section>

          <section className="grid gap-3 lg:grid-cols-2">
            <GraficaPorTipo porTipo={r.por_tipo} nombres={NOMBRES_TIPO} />
            <GraficaValidacion validacion={r.validacion} />
          </section>

          <section>
            <GraficaHistograma histograma={r.histograma} />
          </section>

          <section className="rounded-lg border border-slate-200 bg-white p-4 shadow-sm">
            <h3 className="text-sm font-semibold text-slate-700">Todas las métricas</h3>
            <table className="mt-2 w-full text-sm">
              <thead>
                <tr className="border-b border-slate-200 text-left text-xs uppercase text-slate-500">
                  <th className="py-1">Métrica</th>
                  <th className="py-1 text-right">Valor</th>
                  <th className="py-1 text-right">IC 95 %</th>
                </tr>
              </thead>
              <tbody className="tabular-nums">
                {(
                  [
                    ['TPR medio (h)', resumen.tpr_medio],
                    ['TPR p90 (h)', resumen.tpr_p90],
                    ['TPR mínimo (h)', resumen.tpr_min],
                    ['TPR máximo (h)', resumen.tpr_max],
                    ['OT entregadas por día', resumen.throughput_diario],
                    ['Presupuestos rechazados (%)', resumen.pct_rechazo],
                    ['Bloqueo por refacción (%)', resumen.pct_bloqueo_refaccion],
                    ['Ociosidad (%)', resumen.pct_ociosidad],
                    ['Utilización diagnóstico (%)', resumen.util_diagnostico],
                    ['Utilización reparación (%)', resumen.util_reparacion],
                    ['Cola media diagnóstico', resumen.cola_media_diagnostico],
                    ['Cola media reparación', resumen.cola_media_reparacion],
                    ['WIP diagnóstico', resumen.wip_diagnostico],
                    ['WIP reparación', resumen.wip_reparacion],
                  ] as const
                ).map(([nombre, m]) => (
                  <tr key={nombre} className="border-b border-slate-100">
                    <td className="py-1 text-slate-700">{nombre}</td>
                    <td className="py-1 text-right">{numero(m.valor, 3)}</td>
                    <td className="py-1 text-right text-slate-500">{numero(m.ic95, 3)}</td>
                  </tr>
                ))}
              </tbody>
            </table>
            <p className="mt-2 text-xs text-slate-500">
              TPR completo: {metrica(resumen.tpr_medio)} h
            </p>
          </section>
        </>
      )}
    </main>
  );
}

/* ------------------------------------------------------------------ */

function PanelParametros({
  p,
  cambiar,
}: {
  p: Parametros;
  cambiar: <K extends keyof Parametros>(clave: K, valor: Parametros[K]) => void;
}) {
  return (
    <section className="rounded-lg border border-slate-200 bg-white p-4 shadow-sm">
      <h2 className="text-sm font-semibold text-slate-700">Parámetros</h2>
      <div className="mt-3 grid grid-cols-2 gap-3 md:grid-cols-4 lg:grid-cols-6">
        <Campo etiqueta="Llegadas por día" tipo="number" valor={p.llegadas_por_dia}
          min={1} max={120} onChange={(v) => cambiar('llegadas_por_dia', v)} />
        <Campo etiqueta="Bahías" tipo="number" valor={p.bahias_diagnostico}
          min={1} max={8} onChange={(v) => cambiar('bahias_diagnostico', v)} />
        <Campo etiqueta="Mecánicos" tipo="number" valor={p.mecanicos}
          min={1} max={12} onChange={(v) => cambiar('mecanicos', v)} />
        <Campo etiqueta="Refacciones" tipo="number" valor={p.refacciones_stock}
          min={0} max={200} onChange={(v) => cambiar('refacciones_stock', v)} />
        <Campo etiqueta="Días" tipo="number" valor={p.dias}
          min={1} max={365} onChange={(v) => cambiar('dias', v)} />
        <Campo etiqueta="Réplicas" tipo="number" valor={p.replicas}
          min={1} max={200} onChange={(v) => cambiar('replicas', v)} />
        <Campo etiqueta="Semilla" tipo="number" valor={p.semilla}
          min={1} max={65535} onChange={(v) => cambiar('semilla', v)} />
        <Campo etiqueta="Días de calentamiento" tipo="number" valor={p.dias_calentamiento}
          min={0} max={90} onChange={(v) => cambiar('dias_calentamiento', v)} />
        <Campo etiqueta="Aceptación (%)" tipo="number" valor={p.presupuesto_aceptacion * 100}
          min={0} max={100} onChange={(v) => cambiar('presupuesto_aceptacion', v / 100)} />
        <Campo etiqueta="Apertura (h)" tipo="number" valor={p.hora_apertura}
          min={0} max={23} onChange={(v) => cambiar('hora_apertura', v)} />
        <Campo etiqueta="Cierre (h)" tipo="number" valor={p.hora_cierre}
          min={1} max={24} onChange={(v) => cambiar('hora_cierre', v)} />
        <Campo etiqueta="Reposoición (h)" tipo="number" valor={p.refacciones_periodo_h}
          min={1} max={240} onChange={(v) => cambiar('refacciones_periodo_h', v)} />
      </div>
      <p className="mt-3 text-xs text-slate-500">
        Misma semilla y mismos parámetros ⇒ mismos números. El periodo de
        calentamiento descarta los primeros días, cuando el taller arranca vacío.
      </p>
    </section>
  );
}

function Campo({
  etiqueta,
  tipo,
  valor,
  min,
  max,
  onChange,
}: {
  etiqueta: string;
  tipo: 'number' | 'text';
  valor: number | string;
  min?: number;
  max?: number;
  onChange: (v: number) => void;
}) {
  return (
    <label className="block">
      <span className="block text-xs font-medium text-slate-600">{etiqueta}</span>
      <input
        type={tipo}
        value={valor}
        min={min}
        max={max}
        onChange={(e) => onChange(Number(e.target.value))}
        className="mt-1 w-full rounded border border-slate-300 px-2 py-1 text-sm tabular-nums focus:border-slate-900 focus:outline-none"
      />
    </label>
  );
}
