// escena.ts — Geometría y distribución del taller.
//
// Todo es procedural: no hay modelos ni texturas externas. Así el proyecto
// no depende de descargar nada y no hay temas de licencias.
//
// La planta del local se define aquí como datos, y los componentes de la
// escena la dibujan. Ver notas/04-parte-visual.md.

export type PuestoId =
  | 'recepcion'
  | 'diagnostico'
  | 'cotizacion'
  | 'almacen'
  | 'reparacion'
  | 'entrega';

export const PUESTOS: PuestoId[] = [
  'recepcion',
  'diagnostico',
  'cotizacion',
  'almacen',
  'reparacion',
  'entrega',
];

/** Posición de un puesto en la planta (x, z) y hacia dónde mira. */
export const POSICION_PUESTO: Record<PuestoId, { x: number; z: number; rotY: number }> = {
  // Frente (entrada del cliente)
  recepcion: { x: -11, z: 7.5, rotY: 0 },
  cotizacion: { x: -6.5, z: 7.5, rotY: 0 },
  // Lateral izquierdo
  almacen: { x: -12.5, z: 1.5, rotY: Math.PI / 2 },
  // Fondo
  diagnostico: { x: -6, z: -8, rotY: 0 },
  reparacion: { x: 1.5, z: -8, rotY: 0 },
  // Salida
  entrega: { x: 9, z: -8, rotY: 0 },
};

/** Posición de una bahía de diagnóstico o de reparación. */
export const POSICION_BAHIA = (indice: number, esDiagnostico: boolean): { x: number; z: number } => {
  const base = esDiagnostico ? -6 : 1.5;
  const separacion = 4.2;
  const columnas = 2;
  const fila = Math.floor(indice / columnas);
  const col = indice % columnas;
  return { x: base + col * separacion, z: -8 + fila * 5.5 };
};

/** Dónde aparece un cliente que acaba de llegar. */
export const PUERTA = { x: -14, z: 9 };

/** Punto de la cola de un puesto (los clientes se apilan en fila). */
export const COLA_PUESTO: Record<PuestoId, { x: number; z: number }> = {
  recepcion: { x: -11, z: 5.2 },
  cotizacion: { x: -6.5, z: 5.2 },
  almacen: { x: -10.2, z: 1.5 },
  diagnostico: { x: -6, z: -5.4 },
  reparacion: { x: 1.5, z: -5.4 },
  entrega: { x: 9, z: -5.4 },
};

/** El jugador aparece aquí, en la entrada, mirando al centro del taller. */
export const INICIO_JUGADOR = { x: -12.5, z: 6.5 };

/** Rotación inicial (radianes): mirando hacia el centro del local. */
export const ROTACION_INICIAL = -1.35;

/** Dimensiones del local. */
export const LOCAL = { ancho: 30, fondo: 22, alto: 4.2 };

/** Colores con significado fijo (ver notas/04-parte-visual.md). */
export const COLOR = {
  ocupado: '#f59e0b',      // ámbar: recurso ocupado
  libre: '#cbd5e1',        // gris: libre
  mecanico: '#0ea5e9',     // azul: mecánico ocupado
  falta: '#ef4444',        // rojo: falta algo
  listo: '#22c55e',        // verde: listo para cobrar
  jugador: '#3b82f6',      // azul: el jugador está aquí
  suelo: '#475569',
  pared: '#1e293b',
  metal: '#94a3b8',
  madera: '#78350f',
  pista: '#fbbf24',
};

/** Color de un vehículo según su tipo de avería. */
export const COLOR_COCHE = ['#3b82f6', '#f97316', '#ef4444'];

/** Nombre legible de un puesto, para el HUD. */
export const NOMBRE_PUESTO: Record<PuestoId, string> = {
  recepcion: 'Recepción',
  diagnostico: 'Diagnóstico',
  cotizacion: 'Cotización',
  almacen: 'Almacén',
  reparacion: 'Reparación',
  entrega: 'Entrega',
};

/** Texto de la acción que el jugador puede hacer en cada puesto. */
export const ACCION_PUESTO: Record<PuestoId, string> = {
  recepcion: 'Atender cliente',
  diagnostico: 'Diagnosticar',
  cotizacion: 'Cotizar',
  almacen: 'Tomar piezas',
  reparacion: 'Reparar',
  entrega: 'Entregar',
};
