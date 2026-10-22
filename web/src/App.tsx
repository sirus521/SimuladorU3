// App.tsx — Punto de entrada.
//
// Dos vistas:
//   - Juego: el simulador en primera persona (la entrega de la asignatura)
//   - Análisis: el motor de simulación clásico por lotes, con sus gráficas
//     y la validación contra la teoría. Sirve para el reporte y para
//     comprobar que el modelo de fondo es correcto.

import { useState } from 'react';

import { Juego } from './Juego';
import { PanelAnalisis } from './PanelAnalisis';

type Vista = 'juego' | 'analisis';

export default function App() {
  const [vista, setVista] = useState<Vista>('juego');

  return (
    <div className="relative h-screen w-screen">
      <nav className="absolute left-1/2 top-0 z-50 -translate-x-1/2">
        <div className="mt-2 flex overflow-hidden rounded-lg border border-slate-700 bg-slate-900/90 text-sm backdrop-blur">
          <button
            onClick={() => setVista('juego')}
            className={`px-4 py-1.5 font-medium ${
              vista === 'juego'
                ? 'bg-slate-100 text-slate-900'
                : 'text-slate-300 hover:bg-slate-800'
            }`}
          >
            Juego
          </button>
          <button
            onClick={() => setVista('analisis')}
            className={`px-4 py-1.5 font-medium ${
              vista === 'analisis'
                ? 'bg-slate-100 text-slate-900'
                : 'text-slate-300 hover:bg-slate-800'
            }`}
          >
            Análisis
          </button>
        </div>
      </nav>

      {vista === 'juego' ? <Juego /> : <PanelAnalisis />}
    </div>
  );
}
