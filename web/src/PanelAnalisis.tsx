// PanelAnalisis.tsx — El motor de simulación clásico, por lotes.
//
// Esta vista es la que valida el modelo: corre el mismo proceso de negocio
// sin jugador, con muchas réplicas, y compara el resultado con la
// solución analítica M/M/c. Es el insumo del reporte.
//
// El juego y esta vista comparten el motor de eventos discretos en C.

import { useCallback, useEffect, useMemo, useState } from 'react';
import { Chart } from 'chart.js';
import { Bar, Line } from 'react-chartjs-2';
import {
  BarElement,
  CategoryScale,
  LinearScale,
  LineElement,
  PointElement,
  Tooltip,
  Legend,
} from 'chart.js';

import { ErrorApi, obtenerEscenario, simular } from './api';
import { esFirme, horasATexto, numero } from './formato';
import type { Parametros, RespuestaSimulacion } from './tipos';

Chart.register(
  CategoryScale,
  LinearScale,
  PointElement,
  LineElement,
  BarElement,
  Tooltip,
  Legend,
);

function Tarjeta({
  titulo,
  valor,
  unidad,
  detalle,
  firme = true,
}: {
  titulo: string;
  valor: string;
  unidad?: string;
  detalle?: string;
  firme?: boolean;
}) {
  return (
    <div className="rounded-lg border border-slate-200 bg-white p-4 shadow-sm">
      <div className="text-xs font-medium uppercase tracking-wide text-slate-500">
        {titulo}
      </div>
      <div className="mt-1 flex items-baseline gap-1">
        <span
          className={`text-2xl font-semibold tabular-nums ${
            firme ? 'text-slate-900' : 'text-amber-600'
          }`}
        >
          {valor}
        </span>
        {unidad && <span className="text-sm text-slate-500">{unidad}</span>}
      </div>
      {detalle && <div className="mt-1 text-xs text-slate-500">{detalle}</div>}
    </div>
  );
}

export function PanelAnalisis() {
  const [parametros, setParametros] = useState<Parametros | null>(null);
  const [resultado, setResultado] = useState<RespuestaSimulacion | null>(null);
  const [cargando, setCargando] = useState(false);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    obtenerEscenario()
      .then(setParametros)
      .catch((e: unknown) =>
        setError(
          e instanceof ErrorApi
            ? e.message
            : 'No se pudo cargar el escenario.',
        ),
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

  const resumen = resultado?.resumen;
  const validacion = resultado?.validacion;

  const datosSeries = useMemo(
    () => ({
      labels: (resultado?.series ?? []).map((p) =>
        `${Math.floor(p.t / 24) + 1}`,
      ),
      datasets: [
        {
          label: 'Cola diagnóstico',
          data: (resultado?.series ?? []).map((p) => p.cola_diagnostico),
          borderColor: '#f59e0b',
          backgroundColor: 'rgba(245,158,11,0.12)',
          tension: 0.25,
          pointRadius: 0,
          borderWidth: 1.5,
        },
        {
          label: 'Cola reparación',
          data: (resultado?.series ?? []).map((p) => p.cola_reparacion),
          borderColor: '#0ea5e9',
          backgroundColor: 'rgba(14,165,233,0.12)',
          tension: 0.25,
          pointRadius: 0,
          borderWidth: 1.5,
        },
        {
          label: 'Cola refacciones',
          data: (resultado?.series ?? []).map((p) => p.cola_refacciones),
          borderColor: '#ef4444',
          backgroundColor: 'rgba(239,68,68,0.12)',
          tension: 0.25,
          pointRadius: 0,
          borderWidth: 1.5,
        },
      ],
    }),
    [resultado],
  );

  const datosTeorico = useMemo(
    () => ({
      labels: ['Cola (Lq)', 'En el sistema (L)', 'Permanencia (W, h)'],
      datasets: [
        {
          label: 'Simulado',
          data: validacion
            ? [
                validacion.lq_simulado,
                validacion.l_simulado,
                validacion.w_simulado,
              ]
            : [],
          backgroundColor: '#0ea5e9',
          borderRadius: 3,
        },
        {
          label: 'Teórico M/M/c',
          data: validacion
            ? [validacion.lq_teorico, validacion.l_teorico, validacion.w_teorico]
            : [],
          backgroundColor: '#94a3b8',
          borderRadius: 3,
        },
      ],
    }),
    [validacion],
  );

  return (
    <main className="mx-auto max-w-6xl space-y-4 overflow-y-auto p-4 md:p-6">
      <header className="flex flex-wrap items-center justify-between gap-3 pt-12">
        <div>
          <h1 className="text-xl font-bold text-slate-900">
            Análisis del modelo · SOTR
          </h1>
          <p className="text-sm text-slate-500">
            El motor de simulación por lotes:{' '}
            {resultado?.replicas ?? 30} réplicas
            {resultado ? ` · semilla ${resultado.semilla_base}` : ''}
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

      {parametros && (
        <section className="rounded-lg border border-slate-200 bg-white p-4 shadow-sm">
          <h2 className="text-sm font-semibold text-slate-700">Parámetros</h2>
          <div className="mt-3 grid grid-cols-2 gap-3 md:grid-cols-4 lg:grid-cols-6">
            <Campo
              etiqueta="Llegadas por día"
              valor={parametros.llegadas_por_dia}
              min={1}
              max={120}
              onChange={(v) => cambiar('llegadas_por_dia', v)}
            />
            <Campo
              etiqueta="Bahías"
              valor={parametros.bahias_diagnostico}
              min={1}
              max={8}
              onChange={(v) => cambiar('bahias_diagnostico', v)}
            />
            <Campo
              etiqueta="Mecánicos"
              valor={parametros.mecanicos}
              min={1}
              max={12}
              onChange={(v) => cambiar('mecanicos', v)}
            />
            <Campo
              etiqueta="Refacciones"
              valor={parametros.refacciones_stock}
              min={0}
              max={200}
              onChange={(v) => cambiar('refacciones_stock', v)}
            />
            <Campo
              etiqueta="Días"
              valor={parametros.dias}
              min={1}
              max={365}
              onChange={(v) => cambiar('dias', v)}
            />
            <Campo
              etiqueta="Réplicas"
              valor={parametros.replicas}
              min={1}
              max={200}
              onChange={(v) => cambiar('replicas', v)}
            />
            <Campo
              etiqueta="Semilla"
              valor={parametros.semilla}
              min={1}
              max={65535}
              onChange={(v) => cambiar('semilla', v)}
            />
            <Campo
              etiqueta="Calentamiento"
              valor={parametros.dias_calentamiento}
              min={0}
              max={90}
              onChange={(v) => cambiar('dias_calentamiento', v)}
            />
          </div>
        </section>
      )}

      {!resultado && !cargando && (
        <p className="rounded-lg border border-dashed border-slate-300 p-10 text-center text-sm text-slate-500">
          Pulsa <strong>Simular</strong> para ver los resultados del modelo.
        </p>
      )}

      {resultado && resumen && validacion && (
        <>
          {validacion.en_estacionario === 0 && (
            <div className="rounded-lg border border-amber-300 bg-amber-50 p-3 text-sm text-amber-900">
              <strong>El taller está saturado.</strong> El TPR sigue creciendo con
              el horizonte (deriva de{' '}
              {numero(validacion.deriva_tpr_pct, 1)} % entre la primera y la
              segunda mitad del periodo). El número publicado depende de
              cuántos días se simularon.
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
              titulo="TPR p90"
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
            <div className="rounded-lg border border-slate-200 bg-white p-4 shadow-sm">
              <h3 className="text-sm font-semibold text-slate-700">
                Colas en el tiempo
              </h3>
              <div className="mt-3 h-64">
                <Line
                  data={datosSeries}
                  options={{
                    responsive: true,
                    maintainAspectRatio: false,
                    plugins: { legend: { position: 'bottom' } },
                    scales: { y: { beginAtZero: true } },
                  }}
                />
              </div>
            </div>
            <div className="rounded-lg border border-slate-200 bg-white p-4 shadow-sm">
              <h3 className="text-sm font-semibold text-slate-700">
                Simulado contra teórico
              </h3>
              <p className="text-xs text-slate-500">
                {validacion.comparacion_exacta
                  ? 'Escenario M/M/c exacto: el servicio es una exponencial pura y las llegadas son estacionarias.'
                  : 'Comparación aproximada: las llegadas se concentran en el horario de atención y hay varios tipos de avería.'}
              </p>
              <div className="mt-3 h-64">
                <Bar
                  data={datosTeorico}
                  options={{
                    responsive: true,
                    maintainAspectRatio: false,
                    plugins: { legend: { position: 'bottom' } },
                  }}
                />
              </div>
              <p className="mt-2 text-xs text-slate-600">
                Error relativo en W:{' '}
                <span
                  className={`font-semibold ${
                    Math.abs(validacion.error_w_pct) <= 10
                      ? 'text-emerald-600'
                      : 'text-amber-600'
                  }`}
                >
                  {numero(validacion.error_w_pct, 2)} %
                </span>
              </p>
            </div>
          </section>

          <section className="rounded-lg border border-slate-200 bg-white p-4 shadow-sm">
            <h3 className="text-sm font-semibold text-slate-700">
              Todas las métricas
            </h3>
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
                    ['OT entregadas por día', resumen.throughput_diario],
                    ['Presupuestos rechazados (%)', resumen.pct_rechazo],
                    ['Bloqueo por refacción (%)', resumen.pct_bloqueo_refaccion],
                    ['Ociosidad (%)', resumen.pct_ociosidad],
                    ['Utilización diagnóstico (%)', resumen.util_diagnostico],
                    ['Utilización reparación (%)', resumen.util_reparacion],
                    ['Cola media diagnóstico', resumen.cola_media_diagnostico],
                    ['Cola media reparación', resumen.cola_media_reparacion],
                  ] as const
                ).map(([nombre, m]) => (
                  <tr key={nombre} className="border-b border-slate-100">
                    <td className="py-1 text-slate-700">{nombre}</td>
                    <td className="py-1 text-right">{numero(m.valor, 3)}</td>
                    <td className="py-1 text-right text-slate-500">
                      {numero(m.ic95, 3)}
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          </section>
        </>
      )}
    </main>
  );
}

function Campo({
  etiqueta,
  valor,
  min,
  max,
  onChange,
}: {
  etiqueta: string;
  valor: number;
  min?: number;
  max?: number;
  onChange: (v: number) => void;
}) {
  return (
    <label className="block">
      <span className="block text-xs font-medium text-slate-600">{etiqueta}</span>
      <input
        type="number"
        value={valor}
        min={min}
        max={max}
        onChange={(e) => onChange(Number(e.target.value))}
        className="mt-1 w-full rounded border border-slate-300 px-2 py-1 text-sm tabular-nums focus:border-slate-900 focus:outline-none"
      />
    </label>
  );
}
