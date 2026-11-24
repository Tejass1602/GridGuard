import { useEffect, useState } from 'react'
import { Download, RotateCcw, TriangleAlert } from 'lucide-react'
import { faultNames, Panel } from '../components'
import type { FaultRecord, GridData } from '../types'

const faultScenarios = ['Clear injected fault','Voltage sag','Voltage swell','Frequency low','Frequency high','Short circuit','Overload','ADC clipping']
const feedbackModes = ['Hardware · high side','Hardware · low side','Virtual · healthy','Virtual · welded','Virtual · no source']
const applianceNames = ['Kettle / heater','Fan','SMPS','Fridge']
const faultLabel = (code: number) => faultNames[code] ?? `FAULT_${code}`

export default function FaultLab({ data, command }: { data: GridData; command: (path: string, body: object) => Promise<{accepted:boolean;mode:string}> }) {
  const [fault, setFault] = useState(0); const [magnitude, setMagnitude] = useState('0'); const [mode, setMode] = useState(2)
  const [records, setRecords] = useState<FaultRecord[]>([]); const [message, setMessage] = useState('')
  const send = async (path: string, body: object) => { try { const result=await command(path,body);setMessage(`Command accepted · ${result.mode}`) } catch(error) { setMessage(error instanceof Error?error.message:'Command failed') } }
  useEffect(() => { let alive=true; const load=async()=>{try{const response=await fetch('/api/faults');if(response.ok){const rows=await response.json() as FaultRecord[];if(alive)setRecords(rows)}}catch{if(alive)setRecords([])}};void load();const timer=window.setInterval(()=>void load(),3000);return()=>{alive=false;window.clearInterval(timer)} }, [])
  return <div className="grid gap-4 xl:grid-cols-[.85fr_1.15fr]">
    <div className="space-y-4">
      <Panel title="Scenario controls"><div className="space-y-5">
        <div><label className="eyebrow" htmlFor="fault-select">Inject physics fault</label><div className="mt-2 flex gap-2"><select id="fault-select" className="control min-w-0 flex-1" value={fault} onChange={e=>setFault(Number(e.target.value))}>{faultScenarios.map((name,index)=><option key={name} value={index}>{name}</option>)}</select><input aria-label="fault magnitude" className="control w-24" type="number" value={magnitude} onChange={e=>setMagnitude(e.target.value)} /></div><button className="mt-2 inline-flex items-center gap-2 rounded-md bg-rose-500/15 px-3 py-2 text-xs font-semibold text-rose-300 hover:bg-rose-500/25" onClick={()=>void send('fault',{fault_scenario:fault,magnitude:Number(magnitude)})}><TriangleAlert size={14}/>Apply scenario</button></div>
        <div><label className="eyebrow" htmlFor="feedback-select">Relay feedback source</label><div className="mt-2 flex gap-2"><select id="feedback-select" className="control flex-1" value={mode} onChange={e=>setMode(Number(e.target.value))}>{feedbackModes.map((name,index)=><option key={name} value={index}>{name}</option>)}</select><button className="control text-xs" onClick={()=>void send('feedback-mode',{feedback_mode:mode})}>Set</button></div></div>
        <div><div className="eyebrow">Simulated appliances</div><div className="mt-2 grid grid-cols-2 gap-2">{applianceNames.map((name,index)=>{const on=Boolean((data.nilm?.active_mask??0)&(1<<index));return <button key={name} className={`rounded-md border px-3 py-2 text-left text-xs ${on?'border-cyan-500/40 bg-cyan-500/10 text-cyan-200':'border-slate-700 bg-slate-900 text-slate-400'}`} onClick={()=>void send('appliance',{appliance_id:index,target_state:!on})}>{name}<span className="float-right metric">{on?'ON':'OFF'}</span></button>})}</div></div>
        <button className="inline-flex items-center gap-2 rounded-md border border-slate-700 px-3 py-2 text-xs text-slate-300 hover:border-emerald-500/50" onClick={()=>void send('reset-relay',{force_clear_latch:false})}><RotateCcw size={14}/>Request safe reset</button>
        {message&&<div aria-live="polite" className="rounded border border-slate-800 bg-slate-950 px-3 py-2 text-xs text-slate-400">{message}</div>}
      </div></Panel>
      <Panel title="System health"><div className="grid grid-cols-2 gap-3">{[
        ['DSP latest',data.health?.dsp_exec_us,'µs'],['DSP peak',data.health?.max_dsp_us,'µs'],['CRC errors',data.health?.uart_crc_err,'count'],['Dropped frames',data.health?.dropped_frames,'count'],['NVS records',data.health?.nvs_records,'records'],['Stack headroom',data.health?.free_stack_min,'bytes']
      ].map(([label,value,unit])=><div key={String(label)} className="rounded-md border border-slate-800 bg-slate-950/70 p-3"><div className="eyebrow">{label}</div><div className="metric mt-2 text-lg">{value === undefined ? '—' : Number(value).toLocaleString()}<span className="ml-1 text-[10px] text-slate-500">{unit}</span></div></div>)}</div></Panel>
    </div>
    <Panel title="Persistent fault history" action={<a href="/api/faults/export.csv" className="inline-flex items-center gap-1 text-xs text-sky-300 hover:text-sky-200"><Download size={14}/>Export CSV</a>}>
      <div className="scrollbar max-h-[610px] overflow-auto"><table className="w-full border-collapse text-left text-xs"><thead className="sticky top-0 bg-slate-950 text-[10px] uppercase tracking-wider text-slate-500"><tr>{['Record','Time ms','Fault','Relay result','Vsrc','Vload','Verify ms'].map(x=><th key={x} className="border-b border-slate-800 px-3 py-3 font-medium">{x}</th>)}</tr></thead><tbody>{records.length?records.map(row=><tr key={`${row.mode??'live'}-${row.record_id}`} className="border-b border-slate-800/70 text-slate-300"><td className="metric px-3 py-3">{row.record_id}</td><td className="metric px-3 py-3">{row.ts_ms}</td><td className="px-3 py-3 text-rose-300">{faultLabel(row.fault_code)}</td><td className="metric px-3 py-3">{row.relay_result}</td><td className="metric px-3 py-3">{row.v_src_v.toFixed(2)} V</td><td className="metric px-3 py-3">{row.v_load_v.toFixed(2)} V</td><td className="metric px-3 py-3">{row.verify_ms}</td></tr>):<tr><td colSpan={7} className="px-3 py-16 text-center text-slate-600">No fault records received from the controller.</td></tr>}</tbody></table></div>
      <div className="mt-3 flex items-center justify-between text-[10px] text-slate-600"><span>Source: Pico NVS via CDC</span><span>Refresh interval 3 s</span></div>
    </Panel>
  </div>
}
