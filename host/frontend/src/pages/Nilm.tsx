import { BrainCircuit, CircleCheck, CircleOff } from 'lucide-react'
import { MetricCard, Panel } from '../components'
import type { GridData } from '../types'

const appliances = ['Kettle / heater', 'Ceiling fan', 'Laptop / LED SMPS', 'Refrigerator']

export default function Nilm({ data }: { data: GridData }) {
  const n = data.nilm
  return <div className="space-y-4">
    <div className="grid gap-4 sm:grid-cols-2 xl:grid-cols-4">{appliances.map((name, index) => {
      const on = Boolean((n?.active_mask ?? 0) & (1 << index)); const confidence = n?.confidence[index]
      return <article key={name} className={`panel relative overflow-hidden p-5 ${on ? 'border-cyan-500/40' : ''}`}>
        <div className={`absolute inset-x-0 top-0 h-[2px] ${on ? 'bg-cyan-400' : 'bg-slate-800'}`} />
        <div className="flex items-start justify-between"><div><div className="eyebrow">LOAD 0{index+1}</div><h2 className="mt-2 text-base font-semibold text-slate-200">{name}</h2></div>{on ? <CircleCheck className="text-cyan-300" size={20}/> : <CircleOff className="text-slate-600" size={20}/>}</div>
        <div className="metric mt-8 text-3xl font-semibold">{n ? (n.est_power_w[index] ?? 0).toFixed(0) : '—'}<span className="ml-1 text-sm font-normal text-slate-500">W</span></div>
        <div className="mt-4 grid grid-cols-2 border-t border-slate-800 pt-3 text-xs"><span className={on?'text-cyan-300':'text-slate-500'}>{on?'ON':'OFF'}</span><span className="metric text-right text-slate-400">CONF {confidence === undefined ? '—' : `${(confidence*100).toFixed(0)}%`}</span><span className="eyebrow col-span-2 mt-3">Estimated energy</span><span className="metric col-span-2 mt-1 text-slate-300">{n?.est_energy_wh ? n.est_energy_wh[index].toFixed(4) : '—'}<span className="ml-1 text-slate-500">Wh</span></span></div>
      </article>
    })}</div>
    <div className="grid gap-4 lg:grid-cols-[.7fr_1.3fr]">
      <Panel title="Unknown-load residual"><div className="flex items-center gap-5"><div className="flex h-20 w-20 items-center justify-center rounded-full border-4 border-amber-400/70 bg-amber-400/5"><BrainCircuit className="text-amber-300" size={30}/></div><div><div className="metric text-3xl font-semibold">{n ? n.unknown_power_w.toFixed(1) : '—'}<span className="ml-1 text-sm text-slate-500">W</span></div><div className="mt-1 text-xs text-slate-500">P_meas − Σ estimated active loads</div></div></div><div className="mt-5 h-2 overflow-hidden rounded bg-slate-800"><div className="h-full bg-amber-400 transition-all" style={{width:`${Math.min(100,Math.max(0,(n?.unknown_power_w ?? 0)/10))}%`}}/></div></Panel>
      <Panel title="Inference diagnostics"><div className="grid gap-3 sm:grid-cols-3"><MetricCard label="Mahalanobis distance" value={n?.last_mahalanobis.toFixed(2) ?? '—'} unit="dM" tone={(n?.last_mahalanobis ?? 0)>4.5?'text-amber-300':'text-slate-100'}/><MetricCard label="Inference time" value={n?.inference_us.toFixed(0) ?? '—'} unit="µs"/><MetricCard label="Active signature mask" value={n ? `0x${n.active_mask.toString(16).padStart(2,'0')}` : '—'} unit="BITMASK"/></div></Panel>
    </div>
    <div className="rounded-lg border border-slate-800 bg-slate-900/50 px-4 py-3 text-xs leading-5 text-slate-500">Classification runs on the controller using a CUSUM event detector, a 10→16→4 MLP, and a diagonal Mahalanobis rejection gate. Replay values are labeled in the console header.</div>
  </div>
}
