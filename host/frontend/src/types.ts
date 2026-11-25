export type Mode = 'LIVE' | 'REPLAY'
export type EventMessage = { mode: Mode; kind: string; data: Record<string, unknown> }
export interface Telemetry {
  ts_ms: number; v_rms: number; i_rms: number; p_w: number; q1_var: number; s_va: number; pf: number; freq_hz: number;
  thd_i_pct: number; thd_v_pct: number; energy_wh: number; thermal_pct: number; harm_i_rms: number[]
}
export interface Protection {
  ts_ms: number; relay_state: number; active_fault: number; coil_cmd: number; aux_state: number; feedback_mode: number;
  verify_samples: number; v_src_v: number; v_load_v: number; trip_latency_us: number
}
export interface Waveform { ts_ms: number; v_samples: number[]; i_samples: number[] }
export interface Nilm {
  ts_ms: number; active_mask: number; est_power_w: number[]; est_energy_wh?: number[]; confidence: number[]; unknown_power_w: number;
  last_mahalanobis: number; inference_us: number; last_event_id?: number; last_event_type?: number
}
export interface Health { ts_ms: number; dsp_exec_us: number; max_dsp_us: number; dropped_frames: number; uart_crc_err: number; nvs_records: number; free_stack_min: number }
export interface FaultRecord { record_id: number; ts_ms: number; fault_code: number; relay_result: number; trigger_val: number; threshold_val: number; v_src_v: number; v_load_v: number; verify_ms: number; mode?: string }
export interface GridData {
  mode: Mode | null; connected: boolean; statusText: string; telemetry: Telemetry | null; protection: Protection | null;
  waveform: Waveform | null; nilm: Nilm | null; health: Health | null; lastFrameAt: number | null; history: Array<{ ts: number; source: number; load: number }>
}
