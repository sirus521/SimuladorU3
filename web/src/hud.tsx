// Hud.tsx — Interfaz sobre la escena.
//
// Regla de la interfaz: el HUD nunca muestra un número sin su contexto.
// Si un cliente lleva 40 minutos esperando, dice "40 min", no sólo un color
// rojo. Ver notas/04-parte-visual.md.

import { useCallback, useEffect, useState } from 'react';

import { ACCION_PUESTO, NOMBRE_PUESTO } from './escena';
import type { PuestoId } from './escena';
import type { EstadoPartida } from './tipos';

/* ------------------------------------------------------------------ */
/* Formato                                                              */
/* ------------------------------------------------------------------ */

/** La hora de juego (horas desde medianoche) como "11:24". */
export function horaDeJuego(horas: number): string {
  const dentro = ((horas % 24) + 24) % 24;
  const h = Math.floor(dentro);
  const m = Math.floor((dentro - h) * 60);
  return `${String(h).padStart(2, '0')}:${String(m).padStart(2, '0')}`;
}

/** Horas de juego como "2 h 15 min". */
export function horasLargas(horas: number): string {
  if (!Number.isFinite(horas)) return '—';
  if (horas < 1) return `${Math.round(horas * 60)} min`;
  const h = Math.floor(horas);
  const m = Math.round((horas - h) * 60);
  return m === 0 ? `${h} h` : `${h} h ${m} min`;
}

/** Dinero sin decimales, con separador de miles. */
export function dinero(cantidad: number): string {
  return `$ ${Math.round(cantidad).toLocaleString('es-MX')}`;
}

/* ------------------------------------------------------------------ */
/* Piezas                                                               */
/* ------------------------------------------------------------------ */

function Barra({
  valor,
  total,
  color,
  etiqueta,
  extra,
}: {
  valor: number;
  total: number;
  color: string;
  etiqueta: string;
  extra?: string;
}) {
  const pct = total > 0 ? Math.min(100, (valor / total) * 100) : 0;
  return (
    <div className="rounded bg-slate-900/70 px-3 py-2 backdrop-blur">
      <div className="flex justify-between text-xs text-slate-300">
        <span>{etiqueta}</span>
        <span className="tabular-nums">
          {valor} / {total}
          {extra ? ` · ${extra}` : ''}
        </span>
      </div>
      <div className="mt-1 h-1.5 w-full overflow-hidden rounded bg-slate-700">
        <div
          className="h-full rounded transition-all"
          style={{ width: `${pct}%`, background: color }}
        />
      </div>
    </div>
  );
}

function ContadorCola({
  nombre,
  n,
  critico = false,
}: {
  nombre: string;
  n: number;
  critico?: boolean;
}) {
  /* Una cola de 3 o más es el aviso normal; para las piezas el umbral es
   * más bajo, porque quedarse sin material paraliza el taller entero. */
  const alarma = critico ? n < 12 : n >= 3;
  return (
    <span className="flex items-center gap-1 text-xs">
      <span className="text-slate-400">{nombre}</span>
      <span
        className={`rounded px-1.5 py-0.5 font-semibold tabular-nums ${
          alarma ? 'bg-red-500/80 text-white' : 'bg-slate-700/80 text-slate-200'
        }`}
      >
        {n}
      </span>
    </span>
  );
}

/* ------------------------------------------------------------------ */
/* Paneles                                                              */
/* ------------------------------------------------------------------ */

function PanelContratar({
  estado,
  onContratar,
  onCerrar,
}: {
  estado: EstadoPartida | null;
  onContratar: (tipo: number) => void;
  onCerrar: () => void;
}) {
  const puestos = [
    { tipo: 0, nombre: 'Recepcionista', ayuda: 'Atiende clientes en recepción.' },
    { tipo: 1, nombre: 'Mecánico', ayuda: 'Diagnostica y repara vehículos.' },
    { tipo: 2, nombre: 'Almacenista', ayuda: 'Toma piezas del almacén.' },
    { tipo: 3, nombre: 'Cotizador', ayuda: 'Presenta presupuestos.' },
  ];

  return (
    <div className="absolute inset-0 z-20 flex items-center justify-center bg-slate-950/70">
      <div className="w-full max-w-lg rounded-xl border border-slate-700 bg-slate-900 p-5 shadow-2xl">
        <h2 className="text-lg font-bold text-white">Contratar personal</h2>
        <p className="mt-1 text-xs text-slate-400">
          Cada empleado cuesta {dinero(estado?.nomina_diaria ?? 0)} de nómina al
          día. Atiende un puesto por ti.
        </p>
        <div className="mt-3 space-y-2">
          {puestos.map((p) => {
            const activos =
              estado?.empleados.filter((e) => e.tipo === p.tipo).length ?? 0;
            return (
              <div
                key={p.tipo}
                className="flex items-center justify-between rounded-lg border border-slate-700 bg-slate-800/60 px-3 py-2"
              >
                <div>
                  <div className="text-sm font-medium text-slate-100">
                    {p.nombre}
                    {activos > 0 && (
                      <span className="ml-2 text-xs text-sky-400">
                        {activos} activo{activos > 1 ? 's' : ''}
                      </span>
                    )}
                  </div>
                  <div className="text-xs text-slate-400">{p.ayuda}</div>
                </div>
                <button
                  onClick={() => onContratar(p.tipo)}
                  className="rounded bg-slate-100 px-3 py-1.5 text-xs font-semibold text-slate-900 hover:bg-white"
                >
                  Contratar
                </button>
              </div>
            );
          })}
        </div>
        <div className="mt-4 flex items-center justify-between">
          <span className="text-sm text-slate-300">
            Saldo:{' '}
            <span className="font-semibold text-white">
              {dinero(estado?.saldo ?? 0)}
            </span>
          </span>
          <button
            onClick={onCerrar}
            className="rounded border border-slate-600 px-3 py-1.5 text-xs text-slate-300 hover:bg-slate-800"
          >
            Cerrar (Esc)
          </button>
        </div>
      </div>
    </div>
  );
}

function PanelAlmacen({
  estado,
  onLote,
  onCerrar,
}: {
  estado: EstadoPartida | null;
  onLote: (urgente: boolean) => void;
  onCerrar: () => void;
}) {
  return (
    <div className="absolute inset-0 z-20 flex items-center justify-center bg-slate-950/70">
      <div className="w-full max-w-md rounded-xl border border-slate-700 bg-slate-900 p-5 shadow-2xl">
        <h2 className="text-lg font-bold text-white">Almacén de refacciones</h2>
        <div className="mt-3 space-y-2 text-sm">
          <div className="flex justify-between text-slate-300">
            <span>Stock actual</span>
            <span className="tabular-nums text-white">
              {estado?.stock_refacciones ?? 0} unidades
            </span>
          </div>
          <div className="flex justify-between text-slate-300">
            <span>Lote en camino</span>
            <span className="tabular-nums text-white">
              {estado?.lote_en_camino ?? 0} unidades
            </span>
          </div>
          <div className="flex justify-between text-slate-300">
            <span>
              Lote de {estado?.piezas_por_lote ?? 20} piezas
            </span>
            <span className="tabular-nums text-white">
              {dinero(estado?.precio_lote ?? 0)}
            </span>
            <span>Urgente (llega en 1 h)</span>
            <span className="tabular-nums text-white">
              {dinero(estado?.precio_lote_urgente ?? 0)}
            </span>
          </div>
          <div className="flex justify-between text-slate-300">
            <span>Plazo del proveedor</span>
            <span className="tabular-nums text-white">
              {estado?.plazo_lote_h ?? 24} h
            </span>
          </div>
        </div>
        <div className="mt-4 flex items-center justify-between">
          <div className="flex gap-2">
            <button
              onClick={() => onLote(false)}
              className="rounded bg-slate-100 px-3 py-1.5 text-xs font-semibold text-slate-900 hover:bg-white"
            >
              Pedir lote
            </button>
            <button
              onClick={() => onLote(true)}
              className="rounded bg-amber-400 px-3 py-1.5 text-xs font-semibold text-slate-900 hover:bg-amber-300"
              title="Llega en 1 hora, pero cuesta el doble"
            >
              Urgente
            </button>
          </div>
          <button
            onClick={onCerrar}
            className="rounded border border-slate-600 px-3 py-1.5 text-xs text-slate-300 hover:bg-slate-800"
          >
            Cerrar (Esc)
          </button>
        </div>
      </div>
    </div>
  );
}

function PanelCotizar({
  estado,
  precio,
  onPrecio,
  onCotizar,
  onCerrar,
}: {
  estado: EstadoPartida | null;
  precio: number;
  onPrecio: (p: number) => void;
  onCotizar: (p: number) => void;
  onCerrar: () => void;
}) {
  const vehiculo = estado?.vehiculos.find((v) => v.id === estado.jugador_vehiculo);
  const tipo = vehiculo ? ['Eléctrico', 'Mecánico', 'Carrocería'][vehiculo.tipo] : '';
  /* El precio justo lo manda el servidor: el cliente no duplica la regla. */
  const justo = vehiculo ? vehiculo.precio_justo : 0;
  const disposicion = vehiculo?.disposicion ?? 0;

  return (
    <div className="absolute inset-0 z-20 flex items-center justify-center bg-slate-950/70">
      <div className="w-full max-w-md rounded-xl border border-slate-700 bg-slate-900 p-5 shadow-2xl">
        <h2 className="text-lg font-bold text-white">Presupuesto</h2>
        {vehiculo && (
          <div className="mt-2 space-y-1 rounded-lg bg-slate-800/60 p-3 text-sm">
            <div className="flex justify-between text-slate-300">
              <span>Avería</span>
              <span className="text-white">{tipo}</span>
            </div>
            <div className="flex justify-between text-slate-300">
              <span>Precio justo</span>
              <span className="tabular-nums text-white">{dinero(justo)}</span>
            </div>
            <div className="flex justify-between text-slate-300">
              <span>El cliente paga hasta</span>
              <span className="tabular-nums text-emerald-400">
                {dinero(disposicion)}
              </span>
            </div>
            <div className="flex justify-between text-slate-300">
              <span>Espera</span>
              <span className="tabular-nums text-amber-400">
                {horasLargas(vehiculo.espera)}
              </span>
            </div>
          </div>
        )}
        <label className="mt-4 block text-xs text-slate-400">
          Tu precio
        </label>
        <div className="mt-1 flex items-center gap-2">
          <input
            type="range"
            min={Math.round(justo * 0.4)}
            max={Math.round(justo * 1.6)}
            step={100}
            value={precio}
            onChange={(e) => onPrecio(Number(e.target.value))}
            className="flex-1"
          />
          <span className="w-24 text-right text-sm font-semibold tabular-nums text-white">
            {dinero(precio)}
          </span>
        </div>
        {precio > disposicion && (
          <p className="mt-2 text-xs text-red-400">
            Con este precio el cliente se va.
          </p>
        )}
        <div className="mt-4 flex items-center justify-between">
          <button
            onClick={() => onCotizar(precio)}
            className="rounded bg-slate-100 px-3 py-1.5 text-xs font-semibold text-slate-900 hover:bg-white"
          >
            Presentar (E)
          </button>
          <button
            onClick={onCerrar}
            className="rounded border border-slate-600 px-3 py-1.5 text-xs text-slate-300 hover:bg-slate-800"
          >
            Cancelar
          </button>
        </div>
      </div>
    </div>
  );
}

/* ------------------------------------------------------------------ */
/* Parte del día                                                        */
/* ------------------------------------------------------------------ */

function ParteDelDia({ estado, onCerrar }: { estado: EstadoPartida; onCerrar: () => void }) {
  const ultimo = estado.historial[estado.historial.length - 1];
  if (!ultimo) return null;

  const cumple = ultimo.cumplio_cuota === 1;

  return (
    <div className="absolute inset-0 z-20 flex items-center justify-center bg-slate-950/80">
      <div className="w-full max-w-md rounded-xl border border-slate-700 bg-slate-900 p-5 shadow-2xl">
        <h2 className="text-lg font-bold text-white">
          Día {ultimo.dia} · Cierre
        </h2>
        <div className="mt-3 space-y-1.5 text-sm">
          <div className="flex justify-between">
            <span className="text-slate-300">Entregas</span>
            <span className={`tabular-nums ${cumple ? 'text-emerald-400' : 'text-red-400'}`}>
              {ultimo.entregas} / {ultimo.cuota} cuota {cumple ? '✓' : '✗'}
            </span>
          </div>
          <div className="flex justify-between">
            <span className="text-slate-300">Ingresos</span>
            <span className="tabular-nums text-emerald-400">
              {dinero(ultimo.ingresos)}
            </span>
          </div>
          <div className="flex justify-between">
            <span className="text-slate-300">Gastos</span>
            <span className="tabular-nums text-red-400">
              −{dinero(ultimo.gastos)}
            </span>
          </div>
          <div className="flex justify-between border-t border-slate-700 pt-1.5">
            <span className="text-slate-200">Balance del día</span>
            <span className="tabular-nums font-semibold text-white">
              {dinero(ultimo.ingresos - ultimo.gastos)}
            </span>
          </div>
          <div className="flex justify-between">
            <span className="text-slate-300">Saldo</span>
            <span className="tabular-nums text-white">{dinero(ultimo.saldo)}</span>
          </div>
          <div className="flex justify-between">
            <span className="text-slate-300">Reputación</span>
            <span className="tabular-nums text-white">{ultimo.reputacion}</span>
          </div>
          <div className="flex justify-between">
            <span className="text-slate-300">Clientes perdidos</span>
            <span className="tabular-nums text-white">
              {ultimo.clientes_perdidos}
            </span>
          </div>
          <div className="flex justify-between">
            <span className="text-slate-300">Espera media en reparación</span>
            <span className="tabular-nums text-white">
              {horasLargas(ultimo.espera_media_rep)}
            </span>
          </div>
        </div>
        <button
          onClick={onCerrar}
          className="mt-4 w-full rounded bg-slate-100 px-3 py-2 text-sm font-semibold text-slate-900 hover:bg-white"
        >
          Continuar
        </button>
      </div>
    </div>
  );
}

/* ------------------------------------------------------------------ */
/* Fin de partida                                                       */
/* ------------------------------------------------------------------ */

function FinDePartida({ estado }: { estado: EstadoPartida }) {
  return (
    <div className="absolute inset-0 z-30 flex items-center justify-center bg-slate-950/90">
      <div className="w-full max-w-md rounded-xl border border-slate-700 bg-slate-900 p-6 text-center shadow-2xl">
        <h2
          className={`text-2xl font-bold ${estado.ganada ? 'text-emerald-400' : 'text-red-400'}`}
        >
          {estado.ganada ? 'El taller sobrevivió al mes' : estado.motivo}
        </h2>
        <div className="mt-4 space-y-1 text-sm text-slate-300">
          <div className="flex justify-between">
            <span>Días jugados</span>
            <span className="tabular-nums text-white">{estado.dia}</span>
          </div>
          <div className="flex justify-between">
            <span>Saldo final</span>
            <span className="tabular-nums text-white">{dinero(estado.saldo)}</span>
          </div>
          <div className="flex justify-between">
            <span>Reputación final</span>
            <span className="tabular-nums text-white">{estado.reputacion}</span>
          </div>
          <div className="flex justify-between">
            <span>Empleados</span>
            <span className="tabular-nums text-white">
              {estado.empleados.length}
            </span>
          </div>
        </div>
        <p className="mt-4 text-xs text-slate-500">
          Recarga la página para empezar otra partida.
        </p>
      </div>
    </div>
  );
}

/* ------------------------------------------------------------------ */
/* HUD                                                                  */
/* ------------------------------------------------------------------ */

export function Hud({
  estado,
  cerca,
  pausado,
  velocidad,
  error,
  panel,
  precio,
  onPrecio,
  onAccion: _onAccion,
  onCotizar,
  onContratar,
  onLote,
  onCerrarPanel,
  onLimpiarError,
  onVelocidad,
  onPausa,
}: {
  estado: EstadoPartida | null;
  cerca: { puesto: PuestoId; indice: number } | null;
  pausado: boolean;
  velocidad: number;
  error: string | null;
  panel: 'ninguno' | 'contratar' | 'almacen' | 'cotizar';
  precio: number;
  onPrecio: (p: number) => void;
  onAccion: () => void;
  onCotizar: (p: number) => void;
  onContratar: (tipo: number) => void;
  onLote: (urgente: boolean) => void;
  onCerrarPanel: () => void;
  onLimpiarError: () => void;
  onVelocidad: (v: number) => void;
  onPausa: () => void;
}) {
  /* El parte del día aparece al terminar el día. */
  const [parteVisible, cerrarParte] = useParteDelDia(estado);

  if (!estado) {
    return (
      <div className="absolute inset-0 z-40 flex items-center justify-center bg-slate-950 text-slate-300">
        <div className="text-center">
          <p className="text-lg">SOTR · Taller de Reparación</p>
          <p className="mt-2 text-sm text-slate-500">Conectando con el servidor…</p>
        </div>
      </div>
    );
  }

  const faltante = Math.max(0, estado.cuota - estado.entregas_dia);
  const saldoBajo = estado.saldo < 5000;

  return (
    <div className="pointer-events-none absolute inset-0 z-10 select-none">
      {/* Franja superior */}
      <div className="absolute left-0 right-0 top-0 flex items-start justify-between p-3 pt-14">
        <div className="rounded bg-slate-900/70 px-3 py-2 backdrop-blur">
          <div className="text-sm font-bold text-white">
            Día {estado.dia} / 30
          </div>
          <div className="text-xs tabular-nums text-slate-300">
            {horaDeJuego(estado.t_actual)}
            {pausado && <span className="ml-2 text-amber-400">PAUSA</span>}
          </div>
        </div>

        {/* El saldo va a la derecha, junto a la reputación: el centro de la
         * franja superior lo ocupa el selector de vistas. */}
        <div className="flex items-start gap-2">
          <div className="rounded bg-slate-900/70 px-4 py-2 text-right backdrop-blur">
            <div
              className={`text-lg font-bold tabular-nums ${saldoBajo ? 'text-red-400' : 'text-white'}`}
            >
              {dinero(estado.saldo)}
            </div>
            <div className="text-xs text-slate-400">
              {dinero(estado.ingresos_dia)} · −{dinero(estado.gastos_dia)}
            </div>
          </div>

          <div className="rounded bg-slate-900/70 px-3 py-2 text-right backdrop-blur">
            <div className="text-sm font-bold text-white">Rep {estado.reputacion}</div>
            <div className="mt-1 h-1 w-20 overflow-hidden rounded bg-slate-700">
              <div
                className="h-full bg-sky-400"
                style={{ width: `${estado.reputacion}%` }}
              />
            </div>
          </div>
        </div>
      </div>

      {/* Cuota */}
      <div className="absolute left-3 top-28 w-48">
        <Barra
          valor={estado.entregas_dia}
          total={estado.cuota}
          color={faltante === 0 ? '#22c55e' : '#f59e0b'}
          etiqueta="Cuota del día"
          extra={faltante === 0 ? 'completa' : `faltan ${faltante}`}
        />
      </div>

      {/* Prompt de acción */}
      {cerca && panel === 'ninguno' && !estado.terminada && (
        <div className="absolute bottom-28 left-1/2 -translate-x-1/2">
          <div className="rounded-lg border border-slate-600 bg-slate-900/90 px-4 py-2 text-center backdrop-blur">
            <span className="rounded bg-white px-1.5 py-0.5 text-xs font-bold text-slate-900">
              E
            </span>
            <span className="ml-2 text-sm text-slate-100">
              {ACCION_PUESTO[cerca.puesto]}
            </span>
            <span className="ml-2 text-xs text-slate-500">
              ({NOMBRE_PUESTO[cerca.puesto]})
            </span>
          </div>
        </div>
      )}

      {/* Colas por puesto */}
      <div className="absolute bottom-3 left-3 flex flex-wrap gap-2 rounded bg-slate-900/70 px-3 py-2 backdrop-blur">
        <ContadorCola nombre="Recep" n={estado.colas.recepcion} />
        <ContadorCola nombre="Diag" n={estado.colas.diagnostico} />
        <ContadorCola nombre="Cot" n={estado.colas.cotizacion} />
        <ContadorCola nombre="Alm" n={estado.colas.almacen} />
        <ContadorCola nombre="Rep" n={estado.colas.reparacion} />
        <ContadorCola nombre="Ent" n={estado.colas.entrega} />
        <ContadorCola
          nombre="Piezas"
          n={estado.stock_refacciones}
          critico={estado.stock_refacciones < 12}
        />
      </div>

      {/* Velocidad y ayuda */}
      <div className="pointer-events-auto absolute bottom-3 right-3 flex items-center gap-2 rounded bg-slate-900/70 px-3 py-2 backdrop-blur">
        <button
          onClick={onPausa}
          className="rounded bg-slate-700 px-2 py-1 text-xs text-slate-100 hover:bg-slate-600"
        >
          {pausado ? '▶' : '⏸'}
        </button>
        {[1, 2, 4].map((v) => (
          <button
            key={v}
            onClick={() => onVelocidad(v)}
            className={`rounded px-2 py-1 text-xs ${
              velocidad === v
                ? 'bg-white font-bold text-slate-900'
                : 'bg-slate-700 text-slate-100 hover:bg-slate-600'
            }`}
          >
            ×{v}
          </button>
        ))}
        <span className="ml-2 text-[11px] text-slate-500">
          WASD · ratón · E actuar · B almacén · T contratar
        </span>
      </div>

      {/* Aviso de falta de material: es la información que obliga a
          * actuar, y no aparece en ningún otro sitio. */}
      {estado.stock_refacciones === 0 && !estado.terminada && (
        <div className="pointer-events-auto absolute left-1/2 top-28 -translate-x-1/2 rounded border border-red-500 bg-red-900/90 px-4 py-2 text-sm text-red-100">
          <strong>Sin refacciones.</strong> El taller no puede trabajar.
          Pulsa <kbd className="rounded bg-red-800 px-1">B</kbd> para pedir un
          lote.
        </div>
      )}
      {estado.stock_refacciones > 0 && estado.stock_refacciones < 12 && !estado.terminada && (
        <div className="pointer-events-auto absolute left-1/2 top-28 -translate-x-1/2 rounded border border-amber-500 bg-amber-900/90 px-4 py-2 text-sm text-amber-100">
          Quedan {estado.stock_refacciones} piezas. Pulsa{' '}
          <kbd className="rounded bg-amber-800 px-1">B</kbd> para pedir más.
        </div>
      )}

      {/* Aviso de error */}
      {error && (
        <div
          className="pointer-events-auto absolute left-1/2 top-24 -translate-x-1/2 rounded border border-red-500 bg-red-900/90 px-4 py-2 text-sm text-red-100"
          onClick={onLimpiarError}
        >
          {error}
        </div>
      )}

      {/* Paneles */}
      {panel === 'contratar' && (
        <div className="pointer-events-auto">
          <PanelContratar
            estado={estado}
            onContratar={onContratar}
            onCerrar={onCerrarPanel}
          />
        </div>
      )}
      {panel === 'almacen' && (
        <div className="pointer-events-auto">
          <PanelAlmacen
            estado={estado}
            onLote={onLote}
            onCerrar={onCerrarPanel}
          />
        </div>
      )}
      {panel === 'cotizar' && (
        <div className="pointer-events-auto">
          <PanelCotizar
            estado={estado}
            precio={precio}
            onPrecio={onPrecio}
            onCotizar={onCotizar}
            onCerrar={onCerrarPanel}
          />
        </div>
      )}

      {/* Parte del día. No se muestra si la partida ya terminó: la
          pantalla final va por encima y se solapaban. */}
      {parteVisible && estado.terminada === 0 && (
        <div className="pointer-events-auto">
          <ParteDelDia estado={parteVisible} onCerrar={cerrarParte} />
        </div>
      )}

      {/* Fin de partida */}
      {estado.terminada === 1 && <FinDePartida estado={estado} />}
    </div>
  );
}

/**
 * Detecta el cierre de un día y muestra el parte.
 * Devuelve el parte a mostrar (o null) y la función para cerrarlo.
 */
function useParteDelDia(
  estado: EstadoPartida | null,
): [EstadoPartida | null, () => void] {
  const [parte, setParte] = useState<EstadoPartida | null>(null);
  const [longitudPrevia, setLongitudPrevia] = useState(0);

  useEffect(() => {
    if (!estado) return;
    if (estado.historial.length > longitudPrevia) {
      setParte(estado);
      setLongitudPrevia(estado.historial.length);
    }
  }, [estado, longitudPrevia]);

  const cerrar = useCallback(() => setParte(null), []);
  return [parte, cerrar];
}
