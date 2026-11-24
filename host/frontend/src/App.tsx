import { useState } from 'react'
import { Activity, Circle, Clock3, Menu } from 'lucide-react'
import { HeaderMark, ViewIcon } from './components'
import { useGridGuardWs } from './hooks/useGridGuardWs'
import Overview from './pages/Overview'
import Waveforms from './pages/Waveforms'
import Nilm from './pages/Nilm'
import FaultLab from './pages/FaultLab'

const views = ['Overview','Waveforms','NILM','Fault Lab'] as const
type View = typeof views[number]

export default function App() {
  const [view,setView] = useState<View>('Overview')
  const {state,command} = useGridGuardWs()
  const now = state.telemetry ? new Date().toLocaleTimeString('en-GB',{hour12:false}) : '--:--:--'
  return <div className="min-h-screen">
    <header className="sticky top-0 z-10 border-b border-slate-800/90 bg-slate-950/90 backdrop-blur-xl">
      <div className="mx-auto flex max-w-[1600px] items-center justify-between gap-4 px-4 py-3 sm:px-6">
        <div className="flex items-center gap-3"><HeaderMark/><div><div className="text-sm font-bold tracking-[.18em] text-slate-100">GRIDGUARD</div><div className="eyebrow mt-1">PROTECTION ENGINEERING CONSOLE</div></div></div>
        <div className="hidden items-center gap-5 text-xs text-slate-500 md:flex"><span className="flex items-center gap-2"><Circle size={8} fill={state.connected?'#34d399':'#64748b'} className={state.connected?'text-emerald-400':'text-slate-600'}/>{state.connected?'STREAM ACTIVE':'STREAM WAITING'}</span><span className="flex items-center gap-2"><Clock3 size={13}/><span className="metric">{now}</span></span><span className="flex items-center gap-2"><Activity size={13}/>{state.mode==='REPLAY'?'REPLAY SOURCE':'PICO 2 W'}</span></div>
        <button className="control flex items-center gap-2 text-xs md:hidden"><Menu size={15}/>Views</button>
      </div>
      <nav className="mx-auto flex max-w-[1600px] gap-1 overflow-x-auto px-4 sm:px-6" aria-label="Console views">{views.map(name=><button key={name} onClick={()=>setView(name)} className={`flex items-center gap-2 border-b-2 px-4 py-3 text-xs font-medium transition ${view===name?'border-sky-400 text-sky-300':'border-transparent text-slate-500 hover:text-slate-200'}`}><ViewIcon name={name}/>{name}</button>)}</nav>
    </header>
    <main className="mx-auto max-w-[1600px] space-y-5 px-4 py-6 sm:px-6">
      <div className="flex items-end justify-between"><div><div className="eyebrow">{view==='Overview'?'LIVE SYSTEM STATE':view==='Waveforms'?'SIGNAL INSPECTION':view==='NILM'?'LOAD DISAGGREGATION':'PROTECTION TEST & HISTORY'}</div><h1 className="mt-1 text-xl font-semibold tracking-tight text-slate-100">{view==='Fault Lab'?'Fault History & Scenario Lab':view==='NILM'?'NILM Disaggregation':view}</h1></div><div className="hidden text-right sm:block"><div className="eyebrow">{state.statusText}</div><div className="metric mt-1 text-xs text-slate-500">{state.telemetry ? `FRAME ${state.telemetry.ts_ms}` : 'NO LIVE FRAME'}</div></div></div>
      {view==='Overview'&&<Overview data={state}/>} {view==='Waveforms'&&<Waveforms data={state}/>} {view==='NILM'&&<Nilm data={state}/>} {view==='Fault Lab'&&<FaultLab data={state} command={command}/>}
      <footer className="flex flex-wrap items-center justify-between gap-2 border-t border-slate-900 pt-4 text-[10px] uppercase tracking-wider text-slate-600"><span>GridGuard · RP2350A Cortex-M33 · Internal physics plant</span><span>Low-voltage DC protection demonstration · No mains connection</span></footer>
    </main>
  </div>
}
