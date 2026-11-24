import type { ReactNode } from 'react'
import { Activity, AlertTriangle, CircuitBoard, Radio, ShieldCheck, Waves, Zap } from 'lucide-react'
import type { GridData } from './types'

export const relayNames = ['BOOT_SAFE_OPEN', 'SELF_TEST', 'NORMAL_CLOSED', 'TRIP_REQUESTED', 'COIL_DEENERGIZED', 'VERIFY_OPEN', 'PHYSICAL_DISCONNECT_CONFIRMED', 'TRIP_FAILURE', 'DISCONNECT_UNVERIFIED', 'RESET_CHECK', 'VERIFY_CLOSED', 'CLOSE_FAILURE']
export const faultNames = ['NONE', 'SHORT_CIRCUIT', 'OVERCURRENT', 'IDMT_OVERLOAD', 'UNDERVOLTAGE', 'OVERVOLTAGE', 'FREQ_DRIFT', 'HIGH_THD_WARN', 'ADC_HEALTH', 'RELAY_VERIFY']

export function Panel({ title, action, children, className = '' }: { title: string; action?: ReactNode; children: ReactNode; className?: string }) {
  return <section className={`panel ${className}`}><div className="flex items-center justify-between border-b border-slate-800 px-4 py-3"><h2 className="eyebrow">{title}</h2>{action}</div><div className="p-4">{children}</div></section>
}

export function MetricCard({ label, value, unit, tone = 'text-slate-100' }: { label: string; value: string; unit?: string; tone?: string }) {
  return <div className="panel min-h-[100px] p-4"><div className="eyebrow">{label}</div><div className={`metric mt-3 text-2xl font-semibold ${tone}`}>{value}<span className="ml-1 text-xs font-normal text-slate-500">{unit}</span></div></div>
}

export function StatusBanner({ data }: { data: GridData }) {
  const state = data.protection ? relayNames[data.protection.relay_state] ?? 'UNKNOWN_STATE' : 'NO PROTECTION TELEMETRY'
  const verifying = ['VERIFY_OPEN', 'VERIFY_CLOSED', 'RESET_CHECK', 'SELF_TEST', 'COIL_DEENERGIZED'].includes(state)
  const failed = ['TRIP_FAILURE', 'CLOSE_FAILURE'].includes(state) || Boolean(data.protection?.active_fault && data.protection.active_fault !== 7)
  const unverified = state === 'DISCONNECT_UNVERIFIED' || !data.connected
  const color = failed ? 'border-rose-500/50 bg-rose-500/10 text-rose-300' : verifying || data.mode === 'REPLAY' ? 'border-amber-400/50 bg-amber-400/10 text-amber-200' : unverified ? 'border-slate-700 bg-slate-900 text-slate-400' : 'border-emerald-500/40 bg-emerald-500/10 text-emerald-300'
  const Icon = failed ? AlertTriangle : verifying ? Activity : unverified ? Radio : ShieldCheck
  return <div className={`flex min-h-[92px] items-center justify-between gap-4 rounded-xl border px-5 py-4 ${color}`}>
    <div className="flex items-center gap-4"><Icon size={22} /><div><div className="eyebrow !text-current opacity-70">Physical relay verification</div><div className="metric mt-1 text-lg font-bold tracking-wide">{state}</div></div></div>
    <div className="text-right"><div className="metric text-xs font-bold tracking-widest">{data.mode ?? 'NO API'}{data.mode === 'REPLAY' ? ' MODE' : ''}</div><div className="mt-1 text-xs opacity-70">{data.statusText}</div></div>
  </div>
}

export function EmptyChart({ label }: { label: string }) {
  return <div className="flex h-[280px] items-center justify-center rounded-md border border-dashed border-slate-800 text-xs text-slate-600"><span className="metric">{label}</span></div>
}

export function HeaderMark() { return <div className="flex h-9 w-9 items-center justify-center rounded-lg border border-sky-500/30 bg-sky-500/10 text-sky-300"><CircuitBoard size={19} /></div> }
export function ViewIcon({ name }: { name: string }) { const props={size:16,strokeWidth:1.8}; if(name==='Overview')return <ShieldCheck {...props}/>;if(name==='Waveforms')return <Waves {...props}/>;if(name==='NILM')return <Zap {...props}/>;return <Activity {...props}/> }
