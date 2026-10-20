// tipos.ts — Contrato con la API del servidor C.
//
// Esta forma la congela la fase F5 (ver notas/05-plan-de-trabajo.md). Si
// cambia una clave en el servidor, hay que cambiarla aquí y en
// servidor/rutas.c, y actualizar la nota de la API.

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

export interface EscenarioDisponible {
  archivo: string;
}
