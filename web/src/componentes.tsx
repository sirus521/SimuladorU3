// Componentes de la interfaz.
//
// Cada componente tiene una sola responsabilidad y recibe sus datos por
// props. La lógica de formato vive en formato.ts, que sí se prueba.

import { useMemo } from 'react';
import { Chart } from 'chart.js';
import { Bar, Line } from 'react-chartjs-2';
import { BarElement, CategoryScale, LinearScale, LineElement, PointElement, Tooltip, Legend } from 'chart.js';

import type { Histograma, PuntoSerie, ResultadoTipo, Validacion } from './tipos';
import { comparacionAceptable, instanteATexto, numero } from './formato';

Chart.register(CategoryScale, LinearScale, PointElement, LineElement, BarElement, Tooltip, Legend);

const COLORES = {
  diagnostico: '#f59e0b',
  reparacion: '#0ea5e9',
  refacciones: '#ef4444',
  teorico: '#94a3b8',
  histograma: '#6366f1',
};

/* ------------------------------------------------------------------ */
/* Tarjeta de KPI                                                       */
/* ------------------------------------------------------------------ */

export function Tarjeta({
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

/* ------------------------------------------------------------------ */
/* Planta del taller                                                    */
/* ------------------------------------------------------------------ */

export function Planta({
  bahias,
  mecanicos,
  ocupacionBahi,
  ocupacionMec,
  stockRefacciones,
  capacidadRefacciones,
  esperas,
}: {
  bahias: number;
  mecanicos: number;
  ocupacionBahi: number;
  ocupacionMec: number;
  stockRefacciones: number;
  capacidadRefacciones: number;
  esperas: { diagnostico: number; reparacion: number; refacciones: number };
}) {
  const ancho = 640;
  const alto = 260;

  const casillaBahi = (i: number) => {
    const x = 20 + (i % 4) * 92;
    const y = 30 + Math.floor(i / 4) * 60;
    const ocupada = i < ocupacionBahi;
    return { x, y, ocupada };
  };

  const casillaMec = (i: number) => {
    const x = 400 + (i % 4) * 58;
    const y = 30 + Math.floor(i / 4) * 60;
    const ocupada = i < ocupacionMec;
    return { x, y, ocupada };
  };

  const proporcionStock =
    capacidadRefacciones > 0 ? stockRefacciones / capacidadRefacciones : 0;

  return (
    <div className="rounded-lg border border-slate-200 bg-white p-4 shadow-sm">
      <h3 className="text-sm font-semibold text-slate-700">Planta del taller</h3>
      <svg
        viewBox={`0 0 ${ancho} ${alto}`}
        className="mt-2 w-full"
        role="img"
        aria-label="Planta del taller con bahías, mecánicos y almacén"
      >
        <rect x="0" y="0" width={ancho} height={alto} fill="#f8fafc" rx="8" />

        <text x="20" y="20" fontSize="11" fill="#64748b" fontWeight="600">
          BAHÍAS DE DIAGNÓSTICO
        </text>
        {Array.from({ length: bahias }, (_, i) => {
          const c = casillaBahi(i);
          return (
            <g key={`bahi-${i}`}>
              <rect
                x={c.x}
                y={c.y}
                width="76"
                height="42"
                rx="6"
                fill={c.ocupada ? COLORES.diagnostico : '#e2e8f0'}
                stroke="#cbd5e1"
              />
              <text
                x={c.x + 38}
                y={c.y + 26}
                fontSize="11"
                textAnchor="middle"
                fill={c.ocupada ? '#ffffff' : '#64748b'}
                fontWeight="600"
              >
                B{i + 1}
              </text>
            </g>
          );
        })}

        <text x="400" y="20" fontSize="11" fill="#64748b" fontWeight="600">
          MECÁNICOS
        </text>
        {Array.from({ length: mecanicos }, (_, i) => {
          const c = casillaMec(i);
          return (
            <g key={`mec-${i}`}>
              <rect
                x={c.x}
                y={c.y}
                width="46"
                height="42"
                rx="6"
                fill={c.ocupada ? COLORES.reparacion : '#e2e8f0'}
                stroke="#cbd5e1"
              />
              <text
                x={c.x + 23}
                y={c.y + 26}
                fontSize="11"
                textAnchor="middle"
                fill={c.ocupada ? '#ffffff' : '#64748b'}
                fontWeight="600"
              >
                M{i + 1}
              </text>
            </g>
          );
        })}

        <text x="20" y="196" fontSize="11" fill="#64748b" fontWeight="600">
          ALMACÉN DE REFACCIONES
        </text>
        <rect x="20" y="206" width="360" height="22" rx="4" fill="#e2e8f0" />
        <rect
          x="20"
          y="206"
          width={360 * Math.max(0, Math.min(1, proporcionStock))}
          height="22"
          rx="4"
          fill={proporcionStock < 0.25 ? COLORES.refacciones : '#22c55e'}
        />
        <text x="390" y="222" fontSize="11" fill="#475569">
          {stockRefacciones} / {capacidadRefacciones}
        </text>

        <g transform="translate(20, 170)">
          <text x="0" y="0" fontSize="11" fill="#475569">
            Colas → diagnóstico: <tspan fontWeight="700">{esperas.diagnostico}</tspan> ·
            reparación: <tspan fontWeight="700">{esperas.reparacion}</tspan> ·
            refacciones:{' '}
            <tspan fontWeight="700" fill={esperas.refacciones > 0 ? COLORES.refacciones : '#475569'}>
              {esperas.refacciones}
            </tspan>
          </text>
        </g>
      </svg>

      <div className="mt-2 flex flex-wrap gap-4 text-xs text-slate-600">
        <span className="flex items-center gap-1">
          <span className="inline-block h-3 w-3 rounded" style={{ background: COLORES.diagnostico }} />
          Ocupado
        </span>
        <span className="flex items-center gap-1">
          <span className="inline-block h-3 w-3 rounded bg-slate-300" />
          Libre
        </span>
        <span className="flex items-center gap-1">
          <span className="inline-block h-3 w-3 rounded" style={{ background: COLORES.reparacion }} />
          Mecánico ocupado
        </span>
      </div>
    </div>
  );
}

/* ------------------------------------------------------------------ */
/* Gráfica de las colas en el tiempo                                     */
/* ------------------------------------------------------------------ */

export function GraficaColas({ series }: { series: PuntoSerie[] }) {
  const datos = useMemo(
    () => ({
      labels: series.map((p) => instanteATexto(p.t)),
      datasets: [
        {
          label: 'Cola diagnóstico',
          data: series.map((p) => p.cola_diagnostico),
          borderColor: COLORES.diagnostico,
          backgroundColor: 'rgba(245,158,11,0.12)',
          tension: 0.25,
          pointRadius: 0,
          borderWidth: 1.5,
        },
        {
          label: 'Cola reparación',
          data: series.map((p) => p.cola_reparacion),
          borderColor: COLORES.reparacion,
          backgroundColor: 'rgba(14,165,233,0.12)',
          tension: 0.25,
          pointRadius: 0,
          borderWidth: 1.5,
        },
        {
          label: 'Cola refacciones',
          data: series.map((p) => p.cola_refacciones),
          borderColor: COLORES.refacciones,
          backgroundColor: 'rgba(239,68,68,0.12)',
          tension: 0.25,
          pointRadius: 0,
          borderWidth: 1.5,
        },
      ],
    }),
    [series],
  );

  return (
    <div className="rounded-lg border border-slate-200 bg-white p-4 shadow-sm">
      <h3 className="text-sm font-semibold text-slate-700">Colas en el tiempo</h3>
      <p className="text-xs text-slate-500">
        Promedio de las réplicas. La ventana de atención aparece como un patrón
        diario: la cola sube en el turno y se drena de noche.
      </p>
      {series.length === 0 ? (
        <p className="py-12 text-center text-sm text-slate-400">Sin datos de series.</p>
      ) : (
        <div className="mt-3 h-64">
          <Line
            data={datos}
            options={{
              responsive: true,
              maintainAspectRatio: false,
              interaction: { mode: 'index', intersect: false },
              plugins: { legend: { position: 'bottom' } },
              scales: {
                y: { beginAtZero: true, title: { display: true, text: 'vehículos' } },
                x: {
                  ticks: {
                    maxTicksLimit: 10,
                    autoSkip: true,
                    maxRotation: 0,
                  },
                },
              },
            }}
          />
        </div>
      )}
    </div>
  );
}

/* ------------------------------------------------------------------ */
/* Histograma del TPR                                                    */
/* ------------------------------------------------------------------ */

export function GraficaHistograma({ histograma }: { histograma: Histograma }) {
  const datos = useMemo(
    () => ({
      labels: histograma.clases.map((c) => numero(c, 1)),
      datasets: [
        {
          label: 'Vehículos entregados',
          data: histograma.conteos,
          backgroundColor: COLORES.histograma,
          borderRadius: 3,
        },
      ],
    }),
    [histograma],
  );

  return (
    <div className="rounded-lg border border-slate-200 bg-white p-4 shadow-sm">
      <h3 className="text-sm font-semibold text-slate-700">Distribución del TPR</h3>
      <p className="text-xs text-slate-500">
        Tiempo de permanencia, en 20 clases. La asimetría a la derecha es
        normal: unos pocos vehículos se quedan mucho más tiempo que el
        promedio.
      </p>
      <div className="mt-3 h-56">
        <Bar
          data={datos}
          options={{
            responsive: true,
            maintainAspectRatio: false,
            plugins: { legend: { display: false } },
            scales: {
              y: { beginAtZero: true, title: { display: true, text: 'vehículos' } },
              x: { title: { display: true, text: 'horas' } },
            },
          }}
        />
      </div>
    </div>
  );
}

/* ------------------------------------------------------------------ */
/* TPR por tipo de avería                                               */
/* ------------------------------------------------------------------ */

export function GraficaPorTipo({
  porTipo,
  nombres,
}: {
  porTipo: ResultadoTipo[];
  nombres: string[];
}) {
  const datos = useMemo(
    () => ({
      labels: porTipo.map((_, i) => nombres[i] ?? `Tipo ${i + 1}`),
      datasets: [
        {
          label: 'TPR medio (h)',
          data: porTipo.map((t) => t.media),
          backgroundColor: COLORES.histograma,
          borderRadius: 3,
        },
        {
          label: 'IC 95 %',
          data: porTipo.map((t) => t.ic95),
          backgroundColor: 'rgba(99,102,241,0.28)',
          borderRadius: 3,
        },
      ],
    }),
    [porTipo, nombres],
  );

  return (
    <div className="rounded-lg border border-slate-200 bg-white p-4 shadow-sm">
      <h3 className="text-sm font-semibold text-slate-700">TPR por tipo de avería</h3>
      <p className="text-xs text-slate-500">
        La barra clara es el margen de confianza de cada estimación.
      </p>
      <div className="mt-3 h-56">
        <Bar
          data={datos}
          options={{
            responsive: true,
            maintainAspectRatio: false,
            plugins: { legend: { position: 'bottom' } },
            scales: {
              y: { beginAtZero: true, title: { display: true, text: 'horas' } },
            },
          }}
        />
      </div>
    </div>
  );
}

/* ------------------------------------------------------------------ */
/* Simulado contra teórico                                              */
/* ------------------------------------------------------------------ */

export function GraficaValidacion({ validacion }: { validacion: Validacion }) {
  const datos = useMemo(
    () => ({
      labels: ['Cola (Lq)', 'En el sistema (L)', 'Permanencia (W, h)'],
      datasets: [
        {
          label: 'Simulado',
          data: [validacion.lq_simulado, validacion.l_simulado, validacion.w_simulado],
          backgroundColor: COLORES.reparacion,
          borderRadius: 3,
        },
        {
          label: 'Teórico M/M/c',
          data: [validacion.lq_teorico, validacion.l_teorico, validacion.w_teorico],
          backgroundColor: COLORES.teorico,
          borderRadius: 3,
        },
      ],
    }),
    [validacion],
  );

  const errorW = validacion.error_w_pct;
  const ok = comparacionAceptable(errorW, 10);

  return (
    <div className="rounded-lg border border-slate-200 bg-white p-4 shadow-sm">
      <h3 className="text-sm font-semibold text-slate-700">Simulado contra teórico</h3>
      <p className="text-xs text-slate-500">
        {validacion.comparacion_exacta
          ? 'Escenario M/M/c exacto: el servicio es una exponencial pura y las llegadas son estacionarias.'
          : 'Comparación aproximada: las llegadas se concentran en el horario de atención y hay varios tipos de avería, así que el servicio es una mezcla de exponenciales.'}
      </p>

      <div className="mt-3 h-56">
        <Bar
          data={datos}
          options={{
            responsive: true,
            maintainAspectRatio: false,
            plugins: { legend: { position: 'bottom' } },
          }}
        />
      </div>

      <dl className="mt-3 grid grid-cols-2 gap-x-4 gap-y-1 text-xs text-slate-600">
        <dt>λ (llegadas/h)</dt>
        <dd className="tabular-nums">{numero(validacion.lambda_diagnostico, 4)}</dd>
        <dt>μ (servicio/h)</dt>
        <dd className="tabular-nums">{numero(validacion.mu_diagnostico, 4)}</dd>
        <dt>Error en W</dt>
        <dd className={`tabular-nums font-semibold ${ok ? 'text-emerald-600' : 'text-amber-600'}`}>
          {numero(errorW, 2)} %
        </dd>
      </dl>
    </div>
  );
}
