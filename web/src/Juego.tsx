// Juego.tsx — El juego: escena 3D, controles de primera persona y HUD.
//
// El servidor es autoritativo: aquí no se simula nada. El cliente:
//   1. pide el estado al servidor a ritmo fijo,
//   2. envía acciones (atender, cotizar, contratar),
//   3. dibuja lo que el servidor devuelve.
//
// El movimiento del jugador es local (para que responder sea inmediato) y
// sólo las *acciones sobre el taller* van al servidor.

import { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import { Canvas, useFrame, useThree } from '@react-three/fiber';

import {
  avanzarPartida,
  atenderPuesto,
  contratar,
  cotizar,
  ErrorApi,
  iniciarPartida,
  pedirLote,
} from './api';
import { Escena } from './escena3d';
import { INICIO_JUGADOR, POSICION_PUESTO, ROTACION_INICIAL } from './escena';
import type { PuestoId } from './escena';
import { EstadoPartida } from './tipos';
import { Hud } from './hud';

/* Velocidad de movimiento (unidades de mundo por segundo). */
const VELOCIDAD = 6.5;

/* ------------------------------------------------------------------ */
/* Controles de primera persona                                        */
/* ------------------------------------------------------------------ */

function Controles({
  posicion,
  rotacion,
  onMover,
  activo,
}: {
  posicion: { x: number; z: number };
  rotacion: number;
  onMover: (x: number, z: number) => void;
  activo: boolean;
}) {
  const teclas = useRef<Record<string, boolean>>({});
  const { camera } = useThree();

  useEffect(() => {
    const abajo = (e: KeyboardEvent) => {
      teclas.current[e.code] = true;
    };
    const arriba = (e: KeyboardEvent) => {
      teclas.current[e.code] = false;
    };
    window.addEventListener('keydown', abajo);
    window.addEventListener('keyup', arriba);
    return () => {
      window.removeEventListener('keydown', abajo);
      window.removeEventListener('keyup', arriba);
    };
  }, []);

  useFrame((_, delta) => {
    if (!activo) return;
    const k = teclas.current;
    let dx = 0;
    let dz = 0;
    if (k['KeyW'] || k['ArrowUp']) dz -= 1;
    if (k['KeyS'] || k['ArrowDown']) dz += 1;
    if (k['KeyA'] || k['ArrowLeft']) dx -= 1;
    if (k['KeyD'] || k['ArrowRight']) dx += 1;
    if (dx === 0 && dz === 0) return;

    const largo = Math.hypot(dx, dz);
    dx /= largo;
    dz /= largo;

    // Rotación en torno a Y: el jugador mira a rotacion radianes.
    const cos = Math.cos(rotacion);
    const sin = Math.sin(rotacion);
    const nx = (dx * cos + dz * sin) * VELOCIDAD * delta;
    const nz = (-dx * sin + dz * cos) * VELOCIDAD * delta;

    const nuevo = {
      x: Math.max(-14.2, Math.min(14.2, posicion.x + nx)),
      z: Math.max(-10.2, Math.min(10.2, posicion.z + nz)),
    };
    onMover(nuevo.x, nuevo.z);
    camera.position.set(nuevo.x, 1.65, nuevo.z);
  });

  return null;
}

/** Cámara que sigue al jugador y gestiona la mirada con el ratón. */
function CamaraJugador({
  posicion,
  rotacion,
  onRotar,
}: {
  posicion: { x: number; z: number };
  rotacion: number;
  onRotar: (r: number) => void;
}) {
  const { camera, gl } = useThree();
  const arrastrando = useRef(false);

  useEffect(() => {
    const elemento = gl.domElement;

    const alBajar = () => {
      arrastrando.current = true;
      elemento.requestPointerLock?.();
    };
    const alSubir = () => {
      arrastrando.current = false;
    };
    const alMover = (e: MouseEvent) => {
      if (document.pointerLockElement !== elemento) return;
      onRotar(rotacion + e.movementX * 0.0035);
    };

    elemento.addEventListener('mousedown', alBajar);
    window.addEventListener('mouseup', alSubir);
    document.addEventListener('mousemove', alMover);
    return () => {
      elemento.removeEventListener('mousedown', alBajar);
      window.removeEventListener('mouseup', alSubir);
      document.removeEventListener('mousemove', alMover);
    };
  }, [gl, rotacion, onRotar]);

  useFrame(() => {
    camera.position.set(posicion.x, 1.65, posicion.z);
    camera.rotation.set(0, rotacion, 0);
    camera.rotation.order = 'YXZ';
  });

  return null;
}

/* ------------------------------------------------------------------ */
/* Distancia a los puestos                                              */
/* ------------------------------------------------------------------ */

const RADIO_ACCION = 3.2;

/** ¿A qué puesto está el jugador? El más cercano dentro del radio. */
function puestoCercano(
  posicion: { x: number; z: number },
  estado: EstadoPartida | null,
): { puesto: PuestoId; indice: number } | null {
  if (!estado) return null;
  const candidatos: { puesto: PuestoId; indice: number }[] = [
    { puesto: 'recepcion', indice: 0 },
    { puesto: 'diagnostico', indice: 1 },
    { puesto: 'cotizacion', indice: 2 },
    { puesto: 'almacen', indice: 3 },
    { puesto: 'reparacion', indice: 4 },
    { puesto: 'entrega', indice: 5 },
  ];

  let mejor: { puesto: PuestoId; indice: number } | null = null;
  let mejorDistancia = RADIO_ACCION;
  for (const c of candidatos) {
    const p = POSICION_PUESTO[c.puesto];
    const d = Math.hypot(posicion.x - p.x, posicion.z - p.z);
    if (d < mejorDistancia) {
      mejorDistancia = d;
      mejor = c;
    }
  }
  return mejor;
}

/* ------------------------------------------------------------------ */
/* Componente principal                                                 */
/* ------------------------------------------------------------------ */

export function Juego() {
  const [estado, setEstado] = useState<EstadoPartida | null>(null);
  const [posicion, setPosicion] = useState(INICIO_JUGADOR);
  const [rotacion, setRotacion] = useState(ROTACION_INICIAL);
  const [error, setError] = useState<string | null>(null);
  const [pausado, setPausado] = useState(false);
  const [velocidad, setVelocidad] = useState(1);
  const [panel, setPanel] = useState<'ninguno' | 'contratar' | 'almacen' | 'cotizar'>('ninguno');
  const [precio, setPrecio] = useState(8500);
  const iniciado = useRef(false);

  /* --- Conexión con el servidor --- */
  useEffect(() => {
    if (iniciado.current) return;
    iniciado.current = true;
    iniciarPartida(2026)
      .then(setEstado)
      .catch((e: unknown) =>
        setError(e instanceof ErrorApi ? e.message : 'No se pudo iniciar la partida.'),
      );
  }, []);

  /* --- Bucle de juego: pide el estado a ritmo fijo --- */
  useEffect(() => {
    let vivo = true;
    let ultimo = performance.now();

    const tic = async () => {
      if (!vivo) return;
      const ahora = performance.now();
      const delta = (ahora - ultimo) / 1000;
      ultimo = ahora;

      if (!pausado && estado && !estado.terminada) {
        try {
          // Se pide el avance en tiempo real del servidor; la escala la
          // aplica el cliente multiplicando los segundos.
          const nuevo = await avanzarPartida(Math.min(delta * velocidad, 0.5));
          if (vivo) setEstado(nuevo);
        } catch {
          /* si falla la conexión, se reintenta en el siguiente tic */
        }
      }
      if (vivo) {
        window.setTimeout(tic, 50);
      }
    };

    const id = window.setTimeout(tic, 50);
    return () => {
      vivo = false;
      window.clearTimeout(id);
    };
  }, [pausado, velocidad, estado]);

  /* --- Acciones --- */
  const cerca = useMemo(() => puestoCercano(posicion, estado), [posicion, estado]);

  const accionAtender = useCallback(async () => {
    if (!cerca) return;
    try {
      const r = await atenderPuesto(cerca.indice);
      setEstado(r.estado);
      if (cerca.puesto === 'cotizacion') {
        // Al llegar al mostrador se abre el precio: el cliente tiene su
        // disposición a pagar, así que la cuota se puede ver.
        const v = r.estado.vehiculos.find((x) => x.id === r.estado.jugador_vehiculo);
        setPrecio(v ? Math.round(v.disposicion * 0.85) : 8500);
        setPanel('cotizar');
      }
    } catch (e) {
      setError(e instanceof ErrorApi ? e.message : 'Acción fallida.');
    }
  }, [cerca]);

  const accionCotizar = useCallback(
    async (valor: number) => {
      try {
        const r = await cotizar(valor);
        setEstado(r.estado);
        setPanel('ninguno');
      } catch (e) {
        setError(e instanceof ErrorApi ? e.message : 'No se pudo cotizar.');
      }
    },
    [],
  );

  const accionContratar = useCallback(
    async (tipo: number) => {
      try {
        const r = await contratar(tipo);
        setEstado(r.estado);
        if (!r.aceptada) setError('No se pudo contratar (¿saldo insuficiente?).');
      } catch (e) {
        setError(e instanceof ErrorApi ? e.message : 'No se pudo contratar.');
      }
    },
    [],
  );

  const accionLote = useCallback(async () => {
    try {
      const r = await pedirLote();
      setEstado(r.estado);
      if (!r.aceptada) setError('No se pudo pedir el lote (¿saldo o lote en camino?).');
    } catch (e) {
      setError(e instanceof ErrorApi ? e.message : 'No se pudo pedir el lote.');
    }
  }, []);

  /* --- Teclado: E para actuar, B/T para paneles, espacio para pausar --- */
  useEffect(() => {
    const alPulsar = async (e: KeyboardEvent) => {
      if (e.target instanceof HTMLInputElement) return;
      if (e.code === 'KeyE') {
        if (panel === 'cotizar') {
          await accionCotizar(precio);
        } else if (panel === 'ninguno') {
          await accionAtender();
        }
      } else if (e.code === 'KeyB') {
        setPanel((p) => (p === 'almacen' ? 'ninguno' : 'almacen'));
      } else if (e.code === 'KeyT') {
        setPanel((p) => (p === 'contratar' ? 'ninguno' : 'contratar'));
      } else if (e.code === 'Escape') {
        setPanel('ninguno');
      } else if (e.code === 'Space') {
        e.preventDefault();
        setPausado((p) => !p);
      } else if (e.code === 'Digit1') {
        setVelocidad(1);
      } else if (e.code === 'Digit2') {
        setVelocidad(2);
      } else if (e.code === 'Digit4') {
        setVelocidad(4);
      }
    };
    window.addEventListener('keydown', alPulsar);
    return () => window.removeEventListener('keydown', alPulsar);
  }, [accionAtender, accionCotizar, panel, precio]);

  const mover = useCallback((x: number, z: number) => {
    setPosicion({ x, z });
  }, []);

  const rotar = useCallback((r: number) => {
    setRotacion(r);
  }, []);

  return (
    <div className="relative h-screen w-screen overflow-hidden bg-slate-950">
      {/* El canvas va en un contenedor propio, sin position, para que el
       * HUD (absolute) se posicione respecto a ESTE contenedor y no al
       * body. Con el canvas como hermano directo, el HUD quedaba por
       * debajo y el texto blanco se leía sobre el techo claro del taller. */}
      <div className="absolute inset-0">
      <Canvas
        gl={{ preserveDrawingBuffer: true }}
        camera={{ position: [0, 1.65, 0], fov: 75, up: [0, 1, 0], rotation: [0, 0, 0] }}
        shadows={false}
      >
        <color attach="background" args={['#0f172a']} />
        <ambientLight intensity={1.15} />
        <hemisphereLight args={['#e2e8f0', '#475569', 1.0]} />
        <directionalLight position={[8, 14, 8]} intensity={1.6} />
        <pointLight position={[-6, 3.5, -8]} intensity={40} distance={20} color="#fde68a" />
        <pointLight position={[2, 3.5, -8]} intensity={40} distance={20} color="#fde68a" />
        <pointLight position={[-9, 3.2, 6]} intensity={35} distance={18} color="#e0f2fe" />
        <pointLight position={[6, 3.2, 4]} intensity={30} distance={18} color="#e0f2fe" />

        <Escena estado={estado} />
        <Controles
          posicion={posicion}
          rotacion={rotacion}
          onMover={mover}
          activo={!estado?.terminada}
        />
        <CamaraJugador posicion={posicion} rotacion={rotacion} onRotar={rotar} />
      </Canvas>
      </div>

      <Hud
        estado={estado}
        cerca={cerca}
        pausado={pausado}
        velocidad={velocidad}
        error={error}
        panel={panel}
        precio={precio}
        onPrecio={setPrecio}
        onAccion={accionAtender}
        onCotizar={accionCotizar}
        onContratar={accionContratar}
        onLote={accionLote}
        onCerrarPanel={() => setPanel('ninguno')}
        onLimpiarError={() => setError(null)}
        onVelocidad={setVelocidad}
        onPausa={() => setPausado((p) => !p)}
      />
    </div>
  );
}
