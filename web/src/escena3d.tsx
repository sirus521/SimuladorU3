// escena3d.tsx — La escena del taller en primera persona.
//
// Toda la geometría es procedural (cajas, cilindros, planos): no hay modelos
// ni texturas externas.
//
// NOTA IMPORTANTE (ver notas/04-parte-visual.md): en React-Three-Fiber v9 hay
// que declarar `up` y `rotation` de forma explícita en la prop `camera`. Si
// no, la cámara queda mirando al suelo y renderiza un cuadro vacío sin dar
// ningún error.

import { useMemo, useRef } from 'react';
import { useFrame } from '@react-three/fiber';
import * as THREE from 'three';

import {
  COLOR,
  COLOR_COCHE,
  LOCAL,
  POSICION_BAHIA,
  POSICION_PUESTO,
  PUERTA,
} from './escena';
import type { EstadoPartida, Vehiculo } from './tipos';
import { EstadoVehiculo as EV } from './tipos';

/* ------------------------------------------------------------------ */
/* Piezas del local                                                     */
/* ------------------------------------------------------------------ */

function Suelo() {
  return (
    <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, 0, 0]} receiveShadow={false}>
      <planeGeometry args={[LOCAL.ancho, LOCAL.fondo]} />
      <meshStandardMaterial color={COLOR.suelo} roughness={0.95} />
    </mesh>
  );
}

function Paredes() {
  const ancho = LOCAL.ancho;
  const fondo = LOCAL.fondo;
  const alto = LOCAL.alto;
  const medio = ancho / 2;
  const medioF = fondo / 2;

  return (
    <group>
      {/* Pared del fondo */}
      <mesh position={[0, alto / 2, -medioF]}>
        <boxGeometry args={[ancho, alto, 0.2]} />
        <meshStandardMaterial color={COLOR.pared} />
      </mesh>
      {/* Pared izquierda */}
      <mesh position={[-medio, alto / 2, 0]}>
        <boxGeometry args={[0.2, alto, fondo]} />
        <meshStandardMaterial color={COLOR.pared} />
      </mesh>
      {/* Pared derecha */}
      <mesh position={[medio, alto / 2, 0]}>
        <boxGeometry args={[0.2, alto, fondo]} />
        <meshStandardMaterial color={COLOR.pared} />
      </mesh>
      {/* Techo */}
      <mesh position={[0, alto, 0]} rotation={[Math.PI / 2, 0, 0]}>
        <planeGeometry args={[ancho, fondo]} />
        <meshStandardMaterial color="#0f172a" side={2} />
      </mesh>
      {/* Pared frontal, con hueco de puerta */}
      <mesh position={[-medio + 4, alto / 2, medioF]}>
        <boxGeometry args={[ancho - 8, alto, 0.2]} />
        <meshStandardMaterial color={COLOR.pared} />
      </mesh>
      <mesh position={[medio - 2, alto / 2, medioF]}>
        <boxGeometry args={[4, alto, 0.2]} />
        <meshStandardMaterial color={COLOR.pared} />
      </mesh>
    </group>
  );
}

/** Marcas de las bahías pintadas en el suelo. */
function MarcasBahias({ diagnostico, reparacion }: { diagnostico: number; reparacion: number }) {
  const marcas = useMemo(() => {
    const lista: { x: number; z: number }[] = [];
    for (let i = 0; i < diagnostico; i++) lista.push(POSICION_BAHIA(i, true));
    for (let i = 0; i < reparacion; i++) lista.push(POSICION_BAHIA(i, false));
    return lista;
  }, [diagnostico, reparacion]);

  return (
    <group>
      {marcas.map((m, i) => (
        <mesh key={i} rotation={[-Math.PI / 2, 0, 0]} position={[m.x, 0.02, m.z]}>
          <planeGeometry args={[3.4, 5]} />
          <meshStandardMaterial color={COLOR.pista} transparent opacity={0.16} />
        </mesh>
      ))}
    </group>
  );
}

/* ------------------------------------------------------------------ */
/* Mobiliario                                                           */
/* ------------------------------------------------------------------ */

function Mostrador({
  x,
  z,
  rotY,
  color,
}: {
  x: number;
  z: number;
  rotY: number;
  color: string;
}) {
  return (
    <group position={[x, 0, z]} rotation={[0, rotY, 0]}>
      <mesh position={[0, 0.5, 0]}>
        <boxGeometry args={[3, 1, 1.2]} />
        <meshStandardMaterial color={color} roughness={0.8} />
      </mesh>
      <mesh position={[0, 1.05, 0]}>
        <boxGeometry args={[3.2, 0.1, 1.4]} />
        <meshStandardMaterial color={COLOR.metal} />
      </mesh>
    </group>
  );
}

function Estanteria({ x, z }: { x: number; z: number }) {
  return (
    <group position={[x, 0, z]}>
      {[0, 1, 2].map((n) => (
        <mesh key={n} position={[0, 0.4 + n * 0.85, 0]}>
          <boxGeometry args={[1.2, 0.1, 4]} />
          <meshStandardMaterial color={COLOR.metal} />
        </mesh>
      ))}
      {Array.from({ length: 9 }, (_, i) => (
        <mesh
          key={`c-${i}`}
          position={[-0.35 + (i % 3) * 0.35, 0.62 + Math.floor(i / 3) * 0.85, -1.4 + (i % 3) * 0.1]}
        >
          <boxGeometry args={[0.3, 0.32, 0.3]} />
          <meshStandardMaterial color={COLOR.madera} />
        </mesh>
      ))}
    </group>
  );
}

/* ------------------------------------------------------------------ */
/* Vehículos                                                            */
/* ------------------------------------------------------------------ */

function Coche({
  x,
  z,
  rotY,
  color,
  trabaja,
}: {
  x: number;
  z: number;
  rotY: number;
  color: string;
  trabaja: boolean;
}) {
  const ref = useRef<THREE.Group>(null);

  useFrame(() => {
    if (ref.current && trabaja) {
      // Vibración suave mientras se trabaja: el coche "está en el taller".
      ref.current.position.y = 0.55 + Math.sin(performance.now() / 90) * 0.012;
    }
  });

  return (
    <group ref={ref} position={[x, 0.55, z]} rotation={[0, rotY, 0]}>
      <mesh>
        <boxGeometry args={[1.8, 0.7, 4]} />
        <meshStandardMaterial color={color} roughness={0.5} metalness={0.2} />
      </mesh>
      <mesh position={[0, 0.5, -0.2]}>
        <boxGeometry args={[1.6, 0.55, 2]} />
        <meshStandardMaterial color={color} roughness={0.35} metalness={0.25} />
      </mesh>
      {/* Ruedas */}
      {[
        [-0.9, 0, -1.3],
        [0.9, 0, -1.3],
        [-0.9, 0, 1.3],
        [0.9, 0, 1.3],
      ].map(([wx, , wz], i) => (
        <mesh key={i} position={[wx, -0.28, wz]} rotation={[0, 0, Math.PI / 2]}>
          <cylinderGeometry args={[0.32, 0.32, 0.22, 12]} />
          <meshStandardMaterial color="#1f2937" />
        </mesh>
      ))}
      {/* Faros */}
      <mesh position={[-0.55, 0, 2.02]}>
        <boxGeometry args={[0.35, 0.18, 0.05]} />
        <meshStandardMaterial color="#fde047" />
      </mesh>
      <mesh position={[0.55, 0, 2.02]}>
        <boxGeometry args={[0.35, 0.18, 0.05]} />
        <meshStandardMaterial color="#fde047" />
      </mesh>
    </group>
  );
}

/* ------------------------------------------------------------------ */
/* Personajes (jugador y empleados)                                    */
/* ------------------------------------------------------------------ */

function Persona({
  x,
  z,
  rotY,
  color,
}: {
  x: number;
  z: number;
  rotY: number;
  color: string;
}) {
  return (
    <group position={[x, 0, z]} rotation={[0, rotY, 0]}>
      {/* Piernas */}
      <mesh position={[-0.16, 0.4, 0]}>
        <boxGeometry args={[0.22, 0.8, 0.24]} />
        <meshStandardMaterial color="#1f2937" />
      </mesh>
      <mesh position={[0.16, 0.4, 0]}>
        <boxGeometry args={[0.22, 0.8, 0.24]} />
        <meshStandardMaterial color="#1f2937" />
      </mesh>
      {/* Torso */}
      <mesh position={[0, 1.15, 0]}>
        <boxGeometry args={[0.62, 0.75, 0.36]} />
        <meshStandardMaterial color={color} roughness={0.7} />
      </mesh>
      {/* Brazos */}
      <mesh position={[-0.4, 1.15, 0]}>
        <boxGeometry args={[0.18, 0.7, 0.2]} />
        <meshStandardMaterial color={color} roughness={0.7} />
      </mesh>
      <mesh position={[0.4, 1.15, 0]}>
        <boxGeometry args={[0.18, 0.7, 0.2]} />
        <meshStandardMaterial color={color} roughness={0.7} />
      </mesh>
      {/* Cabeza */}
      <mesh position={[0, 1.75, 0]}>
        <boxGeometry args={[0.42, 0.42, 0.4]} />
        <meshStandardMaterial color="#e8d5c0" />
      </mesh>
      {/* Gorra */}
      <mesh position={[0, 1.98, 0]}>
        <boxGeometry args={[0.46, 0.12, 0.44]} />
        <meshStandardMaterial color={color} />
      </mesh>
    </group>
  );
}

/* ------------------------------------------------------------------ */
/* Indicadores flotantes                                               */
/* ------------------------------------------------------------------ */

function Indicador({
  x,
  z,
  y,
  color,
}: {
  x: number;
  z: number;
  y: number;
  color: string;
}) {
  const ref = useRef<THREE.Mesh>(null);
  useFrame(({ clock }) => {
    if (ref.current) {
      ref.current.position.y = y + Math.sin(clock.elapsedTime * 3) * 0.08;
      ref.current.rotation.y += 0.01;
    }
  });
  return (
    <mesh ref={ref} position={[x, y, z]}>
      <octahedronGeometry args={[0.28]} />
      <meshStandardMaterial color={color} emissive={color} emissiveIntensity={0.6} />
    </mesh>
  );
}

/* ------------------------------------------------------------------ */
/* La escena completa                                                   */
/* ------------------------------------------------------------------ */

/** Convierte el estado del servidor en posiciones de coches en la escena. */
function useCoches(estado: EstadoPartida) {
  return useMemo(() => {
    const diagnostico: { x: number; z: number; rotY: number; color: string; trabaja: boolean }[] = [];
    const reparacion: { x: number; z: number; rotY: number; color: string; trabaja: boolean }[] = [];
    const otros: { x: number; z: number; rotY: number; color: string; trabaja: boolean }[] = [];

    for (const v of estado.vehiculos) {
      const color = COLOR_COCHE[v.tipo] ?? COLOR_COCHE[0];
      const pos = posicionVehiculo(v, estado);
      if (v.estado === EV.EnDiagnostico || v.estado === EV.EsperaDiagnostico) {
        diagnostico.push({ ...pos, color, trabaja: v.estado === EV.EnDiagnostico });
      } else if (v.estado === EV.EnReparacion || v.estado === EV.EsperaReparacion) {
        reparacion.push({ ...pos, color, trabaja: v.estado === EV.EnReparacion });
      } else {
        otros.push({ ...pos, color, trabaja: false });
      }
    }
    return { diagnostico, reparacion, otros };
  }, [estado.vehiculos]);
}

/** Posición de un vehículo según su estado. */
function posicionVehiculo(
  v: Vehiculo,
  estado: EstadoPartida,
): { x: number; z: number; rotY: number } {
  switch (v.estado) {
    case EV.EnRecepcion:
      return { x: POSICION_PUESTO.recepcion.x, z: POSICION_PUESTO.recepcion.z + 1.6, rotY: 0 };
    case EV.EsperaRecepcion: {
      const i = estado.vehiculos
        .filter((x) => x.estado === EV.EsperaRecepcion)
        .findIndex((x) => x.id === v.id);
      return { x: POSICION_PUESTO.recepcion.x + 2.6 + i * 2.2, z: POSICION_PUESTO.recepcion.z, rotY: Math.PI / 2 };
    }
    case EV.EnDiagnostico: {
      const i = estado.vehiculos
        .filter((x) => x.estado === EV.EnDiagnostico)
        .findIndex((x) => x.id === v.id);
      return { ...POSICION_BAHIA(i, true), rotY: 0 };
    }
    case EV.EsperaDiagnostico: {
      const i = estado.vehiculos
        .filter((x) => x.estado === EV.EsperaDiagnostico)
        .findIndex((x) => x.id === v.id);
      return { x: POSICION_PUESTO.diagnostico.x + 2.4 + i * 2.2, z: POSICION_PUESTO.diagnostico.z, rotY: Math.PI / 2 };
    }
    case EV.EnCotizacion:
      return { x: POSICION_PUESTO.cotizacion.x, z: POSICION_PUESTO.cotizacion.z + 1.6, rotY: 0 };
    case EV.EsperaCotizacion: {
      const i = estado.vehiculos
        .filter((x) => x.estado === EV.EsperaCotizacion)
        .findIndex((x) => x.id === v.id);
      return { x: POSICION_PUESTO.cotizacion.x + 2.4 + i * 2.2, z: POSICION_PUESTO.cotizacion.z, rotY: Math.PI / 2 };
    }
    case EV.EnAlmacen:
      return { x: POSICION_PUESTO.almacen.x + 1.4, z: POSICION_PUESTO.almacen.z, rotY: Math.PI / 2 };
    case EV.EsperaAlmacen: {
      const i = estado.vehiculos
        .filter((x) => x.estado === EV.EsperaAlmacen)
        .findIndex((x) => x.id === v.id);
      return { x: POSICION_PUESTO.almacen.x + 1.4, z: POSICION_PUESTO.almacen.z - 2.2 - i * 2.2, rotY: 0 };
    }
    case EV.EnReparacion: {
      const i = estado.vehiculos
        .filter((x) => x.estado === EV.EnReparacion)
        .findIndex((x) => x.id === v.id);
      return { ...POSICION_BAHIA(i, false), rotY: 0 };
    }
    case EV.EsperaReparacion: {
      const i = estado.vehiculos
        .filter((x) => x.estado === EV.EsperaReparacion)
        .findIndex((x) => x.id === v.id);
      return { x: POSICION_PUESTO.reparacion.x + 2.4 + i * 2.2, z: POSICION_PUESTO.reparacion.z, rotY: Math.PI / 2 };
    }
    case EV.EnEntrega:
      return { x: POSICION_PUESTO.entrega.x, z: POSICION_PUESTO.entrega.z + 1.6, rotY: 0 };
    case EV.EsperaEntrega: {
      const i = estado.vehiculos
        .filter((x) => x.estado === EV.EsperaEntrega)
        .findIndex((x) => x.id === v.id);
      return { x: POSICION_PUESTO.entrega.x + 2.4 + i * 2.2, z: POSICION_PUESTO.entrega.z, rotY: Math.PI / 2 };
    }
    default:
      return { x: PUERTA.x, z: PUERTA.z, rotY: 0 };
  }
}

export function Escena({ estado }: { estado: EstadoPartida | null }) {
  const coches = useCoches(estado ?? ({ vehiculos: [] } as unknown as EstadoPartida));

  const nBahiasDiag = estado ? 2 : 0;
  const nBahiasRep = estado ? 3 : 0;

  return (
    <group>
      <Suelo />
      <Paredes />
      <MarcasBahias diagnostico={nBahiasDiag} reparacion={nBahiasRep} />

      {/* Mostradores del frente */}
      <Mostrador {...POSICION_PUESTO.recepcion} color="#334155" />
      <Mostrador {...POSICION_PUESTO.cotizacion} color="#334155" />
      <Mostrador {...POSICION_PUESTO.entrega} color="#334155" />

      {/* Almacén */}
      <Estanteria x={POSICION_PUESTO.almacen.x} z={POSICION_PUESTO.almacen.z - 1.2} />

      {/* Focos sobre las bahías: sin ellos el fondo del taller queda
       * negro y no se ve el trabajo. */}
      {Array.from({ length: 2 }, (_, i) => {
        const p = POSICION_BAHIA(i, true);
        return (
          <pointLight
            key={`ld${i}`}
            position={[p.x, 3.6, p.z]}
            intensity={45}
            distance={16}
            color="#fde68a"
          />
        );
      })}
      {Array.from({ length: 3 }, (_, i) => {
        const p = POSICION_BAHIA(i, false);
        return (
          <pointLight
            key={`lr${i}`}
            position={[p.x, 3.6, p.z]}
            intensity={45}
            distance={16}
            color="#fde68a"
          />
        );
      })}

      {/* Vehículos */}
      {coches.diagnostico.map((c, i) => (
        <Coche key={`d${i}`} {...c} />
      ))}
      {coches.reparacion.map((c, i) => (
        <Coche key={`r${i}`} {...c} />
      ))}
      {coches.otros.map((c, i) => (
        <Coche key={`o${i}`} {...c} />
      ))}

      {/* Indicadores de trabajo pendiente */}
      {estado && estado.colas.recepcion > 0 && (
        <Indicador
          x={POSICION_PUESTO.recepcion.x}
          z={POSICION_PUESTO.recepcion.z}
          y={2.4}
          color={COLOR.ocupado}
        />
      )}
      {estado && estado.colas.diagnostico > 0 && (
        <Indicador
          x={POSICION_PUESTO.diagnostico.x}
          z={POSICION_PUESTO.diagnostico.z}
          y={2.4}
          color={COLOR.ocupado}
        />
      )}
      {estado && estado.colas.cotizacion > 0 && (
        <Indicador
          x={POSICION_PUESTO.cotizacion.x}
          z={POSICION_PUESTO.cotizacion.z}
          y={2.4}
          color={COLOR.ocupado}
        />
      )}
      {estado && estado.colas.almacen > 0 && (
        <Indicador
          x={POSICION_PUESTO.almacen.x}
          z={POSICION_PUESTO.almacen.z}
          y={2.4}
          color={COLOR.falta}
        />
      )}
      {estado && estado.colas.reparacion > 0 && (
        <Indicador
          x={POSICION_PUESTO.reparacion.x}
          z={POSICION_PUESTO.reparacion.z}
          y={2.4}
          color={COLOR.ocupado}
        />
      )}
      {estado && estado.colas.entrega > 0 && (
        <Indicador
          x={POSICION_PUESTO.entrega.x}
          z={POSICION_PUESTO.entrega.z}
          y={2.4}
          color={COLOR.listo}
        />
      )}

      {/* El jugador NO se dibuja: en primera persona la cámara está en su
       * cabeza, así que su propio cuerpo sólo se vería desde dentro.
       * Su posición se usa para el prompt de acción y para las colisiones. */}

      {/* Empleados: cada uno en su puesto */}
      {estado?.empleados.map((e) => {
        const puesto = e.puesto;
        if (puesto < 0) return null;
        const pos =
          puesto === 1 || puesto === 4
            ? POSICION_BAHIA(0, puesto === 1)
            : { x: POSICION_PUESTO.recepcion.x, z: POSICION_PUESTO.recepcion.z };
        return (
          <Persona
            key={e.id}
            x={pos.x + 2.2}
            z={pos.z + 1.8}
            rotY={Math.PI}
            color={COLOR.mecanico}
          />
        );
      })}
    </group>
  );
}
