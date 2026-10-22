// tipos.ts — Contrato con la API de partida (C) y tipos del cliente.
//
// La forma de estos tipos la fija el servidor C (servidor/rutas_juego.c).
// Si se cambia una clave allí, hay que cambiarla aquí.

/** Puestos del taller, en el mismo orden que el enum de C. */
export const enum Puesto {
  Recepcion = 0,
  Diagnostico = 1,
  Cotizacion = 2,
  Almacen = 3,
  Reparacion = 4,
  Entrega = 5,
}

/** Estados de un vehículo, en el mismo orden que el enum de C. */
export const enum EstadoVehiculo {
  EsperaRecepcion = 0,
  EnRecepcion = 1,
  EsperaDiagnostico = 2,
  EnDiagnostico = 3,
  EsperaCotizacion = 4,
  EnCotizacion = 5,
  EsperaAlmacen = 6,
  EnAlmacen = 7,
  EsperaReparacion = 8,
  EnReparacion = 9,
  EsperaEntrega = 10,
  EnEntrega = 11,
  Entregado = 12,
  Rechazado = 13,
  Ido = 14,
}

export interface Vehiculo {
  id: number;
  tipo: number;
  estado: EstadoVehiculo;
  t_llegada: number;
  /** Horas que lleva en el sistema. */
  espera: number;
  /** Disposición a pagar del cliente. */
  disposicion: number;
  precio_ofrecido: number;
  /** Precio justo del tipo de avería (lo manda el servidor). */
  precio_justo: number;
  coste_pieza: number;
  piezas_necesarias: number;
  /** Horas que lleva esperando en la cola actual. */
  en_espera: number;
}

export interface Empleado {
  id: number;
  tipo: number;
  puesto: number;
  ocupado: number;
}

export interface Colas {
  recepcion: number;
  diagnostico: number;
  cotizacion: number;
  almacen: number;
  reparacion: number;
  entrega: number;
}

export interface DiaHistorial {
  dia: number;
  entregas: number;
  cuota: number;
  cumplio_cuota: number;
  clientes_perdidos: number;
  ingresos: number;
  gastos: number;
  saldo: number;
  reputacion: number;
  espera_media_rep: number;
}

export interface EstadoPartida {
  dia: number;
  t_actual: number;
  saldo: number;
  ingresos_dia: number;
  gastos_dia: number;
  reputacion: number;
  cuota: number;
  entregas_dia: number;
  perdidos_dia: number;
  stock_refacciones: number;
  lote_en_camino: number;
  jugador_puesto: number;
  jugador_vehiculo: number;
  terminada: number;
  ganada: number;
  motivo: string;
  semilla: number;

  /* Reglas económicas, mandadas por el servidor para no duplicarlas. */
  nomina_diaria: number;
  alquiler_diario: number;
  precio_lote: number;
  precio_lote_urgente: number;
  plazo_lote_urgente_h: number;
  piezas_por_lote: number;
  plazo_lote_h: number;
  horas_atencion: number;
  dias_partida: number;
  colas: Colas;
  vehiculos: Vehiculo[];
  empleados: Empleado[];
  historial: DiaHistorial[];
}

export interface RespuestaAccion {
  aceptada: number;
  estado: EstadoPartida;
}

/* ------------------------------------------------------------------ */
/* Contrato de la API de análisis (motor de simulación clásico)          */
/* ------------------------------------------------------------------ */

export interface Parametros {
  nombre: string;
  semilla: number;
  dias: number;
  dias_calentamiento: number;
  replicas: number;
  llegadas_por_dia: number;
  bahias_diagnostico: number;
  mecanicos: number;
  refacciones_stock: number;
  refacciones_lote: number;
  refacciones_periodo_h: number;
  presupuesto_aceptacion: number;
  presupuesto_decision_h: number;
  hora_apertura: number;
  hora_cierre: number;
  tipos: TipoAveria[];
}

export interface TipoAveria {
  peso: number;
  media_diagnostico_h: number;
  media_reparacion_h: number;
  refacciones: number;
}

/** Una métrica siempre viene con su media y su margen de IC 95 %. */
export interface Metrica {
  valor: number;
  ic95: number;
}

export interface Resumen {
  tpr_medio: Metrica;
  tpr_p90: Metrica;
  tpr_min: Metrica;
  tpr_max: Metrica;
  throughput_diario: Metrica;
  pct_rechazo: Metrica;
  pct_bloqueo_refaccion: Metrica;
  pct_ociosidad: Metrica;
  util_diagnostico: Metrica;
  util_reparacion: Metrica;
  cola_media_diagnostico: Metrica;
  cola_media_reparacion: Metrica;
  wip_diagnostico: Metrica;
  wip_reparacion: Metrica;
  permanencia_diagnostico: Metrica;
  permanencia_reparacion: Metrica;
}

export interface Validacion {
  lambda_diagnostico: number;
  mu_diagnostico: number;
  servidores: number;
  lq_teorico: number;
  l_teorico: number;
  w_teorico: number;
  lq_simulado: number;
  l_simulado: number;
  w_simulado: number;
  error_lq_pct: number;
  error_l_pct: number;
  error_w_pct: number;
  estable: number;
  comparacion_exacta: number;
  en_estacionario: number;
  deriva_tpr_pct: number;
  tpr_primera_mitad: number;
  tpr_segunda_mitad: number;
}

export interface PuntoSerie {
  t: number;
  cola_diagnostico: number;
  cola_reparacion: number;
  cola_refacciones: number;
  en_diagnostico: number;
  en_reparacion: number;
}

export interface Histograma {
  clases: number[];
  conteos: number[];
  ancho: number;
  maximo: number;
}

export interface ResultadoTipo {
  media: number;
  ic95: number;
  muestras: number;
}

export interface RespuestaSimulacion {
  escenario: string;
  semilla_base: number;
  replicas: number;
  parametros: Parametros;
  resumen: Resumen;
  validacion: Validacion;
  series: PuntoSerie[];
  histograma: Histograma;
  por_tipo: ResultadoTipo[];
  avisos?: string;
}
