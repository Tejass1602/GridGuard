import { useEffect, useMemo, useRef, useState } from 'react'
import type { ReactNode } from 'react'
import ReactECharts from 'echarts-for-react'
import { Activity, AlertTriangle, Check, CircuitBoard, Download, RotateCcw, ShieldCheck, Zap } from 'lucide-react'
import { useGridGuardWs } from './hooks/useGridGuardWs'
import type { FaultRecord, GridData } from './types'

const appliances = [
  { name: 'Kettle', detail: '1000 W', behavior: 'Resistive · Unity PF', color: '#D97706' },
  { name: 'Ceiling fan', detail: '75 W', behavior: 'Inductive Motor · 2.2× Inrush', color: '#0F766E' },
  { name: 'Laptop SMPS', detail: '65 W', behavior: 'Nonlinear SMPS · High THD', color: '#1D4ED8' },
  { name: 'Refrigerator', detail: '250 W', behavior: 'Compressor · 4.5× Inrush', color: '#7C3AED' },
]
const applianceName = (index: number) => ['Kettle', 'Ceiling Fan', 'Laptop SMPS', 'Fridge', 'Unknown Residual'][index] ?? `Appliance ${index + 1}`
const feedbackModes = ['Hardware · high side', 'Hardware · low side', 'Virtual · healthy', 'Virtual · welded', 'Virtual · no source']
const faultActions = [
  { label: 'Short Circuit', scenario: 5, magnitude: 28 },
  { label: 'Overcurrent', scenario: 6, magnitude: 14 },
  { label: 'IDMT Overload', scenario: 6, magnitude: 10 },
  { label: 'Undervoltage', scenario: 1, magnitude: 180 },
  { label: 'Overvoltage', scenario: 2, magnitude: 265 },
  { label: 'Frequency Drift', scenario: 3, magnitude: 47.5 },
  { label: 'Clear Fault', scenario: 0, magnitude: 0 },
]
const relayNames = ['Boot safe open', 'Self test', 'Normal closed', 'Trip requested', 'Coil de-energized', 'Verify open', 'Physical disconnect confirmed', 'Trip failure', 'Disconnect unverified', 'Reset check', 'Verify closed', 'Close failure']
const faultNames = ['None', 'Short circuit', 'Overcurrent', 'IDMT overload', 'Undervoltage', 'Overvoltage', 'Frequency drift', 'High THD warning', 'ADC health', 'Relay verify']
const faultTone = (code: number) => code === 0 ? 'good' : [4, 5, 6, 7].includes(code) ? 'warning' : 'bad'
const relayTone = (code: number) => [2, 6].includes(code) ? 'good' : [1, 3, 4, 5, 9, 10].includes(code) ? 'warning' : [7, 8, 11].includes(code) ? 'bad' : 'muted'
const fmt = (value: number | undefined, digits = 1) => value === undefined || !Number.isFinite(value) ? '—' : value.toFixed(digits)
const formatUptime = (milliseconds: number) => {
  if (milliseconds < 1_000) return `${Math.floor(Math.max(0, milliseconds))} ms`
  const tenths = Math.floor(Math.max(0, milliseconds) / 100)
  const hours = Math.floor(tenths / 36_000)
  const minutes = Math.floor((tenths % 36_000) / 600)
  const seconds = Math.floor((tenths % 600) / 10)
  return `${String(hours).padStart(2, '0')}:${String(minutes).padStart(2, '0')}:${String(seconds).padStart(2, '0')}.${tenths % 10}`
}
const clampPct = (value: number, max: number) => `${Math.max(0, Math.min(100, value / max * 100))}%`

function Card({ title, action, children, className = '' }: { title: string; action?: ReactNode; children: ReactNode; className?: string }) {
  return <section className={`card ${className}`}><header className="card-head"><h2>{title}</h2>{action}</header><div className="card-body">{children}</div></section>
}

function Metric({ label, value, unit, tone = '' }: { label: string; value: string; unit?: string; tone?: string }) {
  return <article className="metric-card"><div className="metric-label">{label}</div><div className={`metric-value ${tone}`}>{value}<span>{unit}</span></div></article>
}

function relaySummary(data: GridData) {
  const state = data.protection?.relay_state
  if (state === 6) return { text: 'Physical Disconnect Confirmed', tone: 'good' }
  if (state === 7 || state === 11) return { text: 'Trip Failure', tone: 'bad' }
  if (state === 2) return { text: 'Relay Closed · Load Powered', tone: 'good' }
  if ([1, 3, 4, 5, 9, 10].includes(state ?? -1)) return { text: 'Relay Verification In Progress', tone: 'warning' }
  return { text: data.connected ? 'Awaiting Protection State' : 'Pico 2 W Disconnected', tone: 'muted' }
}

function Overview({ data, states, onToggle, feedbackMode, onFeedback, onFault, onReset, toast, busy }: {
  data: GridData; states: boolean[]; onToggle: (index: number) => void; feedbackMode: number; onFeedback: (mode: number) => void
  onFault: (scenario: number, magnitude: number, label: string) => void; onReset: () => void; toast: string; busy: boolean
}) {
  const t = data.telemetry
  const p = data.protection
  const zeroLoad = (t?.i_rms ?? 0) < 0.05
  const peakI = zeroLoad ? 0 : Math.max(...(data.waveform?.i_samples ?? []).map((sample) => Math.abs(sample / 100)), 0)
  const iLimit = Math.max(6, Math.ceil(peakI * 1.25))
  const scopeOption = useMemo(() => ({
    animation: false, grid: { left: 58, right: 58, top: 36, bottom: 28 },
    tooltip: { trigger: 'axis' }, legend: { top: 0, textStyle: { color: '#57534E', fontFamily: 'Plus Jakarta Sans, sans-serif', fontSize: 12 } },
    xAxis: { type: 'category', data: Array.from({ length: data.waveform?.v_samples.length ?? 64 }, (_, i) => i), axisLabel: { color: '#78716C', show: false, fontSize: 12 }, boundaryGap: false, axisLine: { lineStyle: { color: '#E7E2D8' } } },
    yAxis: [
      { type: 'value', name: 'V', min: -400, max: 400, axisLabel: { color: '#1D4ED8', fontSize: 12 }, nameTextStyle: { color: '#1D4ED8', fontSize: 12 }, splitLine: { lineStyle: { color: '#E7E2D8' } } },
      { type: 'value', name: 'A', min: -iLimit, max: iLimit, axisLabel: { color: '#0F766E', fontSize: 12 }, nameTextStyle: { color: '#0F766E', fontSize: 12 }, splitLine: { show: false } },
    ],
    series: [
      { name: 'v(t)', type: 'line', yAxisIndex: 0, showSymbol: false, data: data.waveform?.v_samples.map(x => x / 10) ?? [], lineStyle: { color: '#1D4ED8', width: 1.8 } },
      { name: 'i(t)', type: 'line', yAxisIndex: 1, showSymbol: false, data: data.waveform?.i_samples.map(x => zeroLoad ? 0 : x / 100) ?? [], lineStyle: { color: '#0F766E', width: 1.8 } },
    ],
  }), [data.waveform, iLimit, zeroLoad])
  const banner = relaySummary(data)
  const voltsMax = Math.max(300, p?.v_src_v ?? 0, p?.v_load_v ?? 0)
  return <div className="page-stack">
    <section className={`relay-banner ${banner.tone}`}>
      <div className="banner-top"><div className="banner-status"><span className="status-symbol"><ShieldCheck size={20}/></span><div><div className="eyebrow">Relay status</div><h2>{banner.text}</h2></div></div><div className="reset-action"><button className="button button-primary" onClick={onReset} disabled={busy}><RotateCcw size={16}/>Safe Reset / Close Relay</button>{p?.active_fault === 0 && p?.relay_state === 6 && <span className="reset-hint">Click Safe Reset to re-energize load</span>}</div></div>
      <div className="voltage-bars">
        <div className="voltage-item"><div className="voltage-label"><span>Source voltage</span><strong className="tabular">{fmt(p?.v_src_v, 1)} V</strong></div><div className="bar-track"><i className="source-fill" style={{ width: clampPct(p?.v_src_v ?? 0, voltsMax) }}/></div></div>
        <div className="voltage-item"><div className="voltage-label"><span>Load voltage</span><strong className="tabular">{fmt(p?.v_load_v, 1)} V</strong></div><div className="bar-track"><i className="load-fill" style={{ width: clampPct(p?.v_load_v ?? 0, voltsMax) }}/></div></div>
        <div className="relay-details"><span className={`pill ${p?.aux_state ? 'good' : 'muted'}`}>AUX {p?.aux_state ? 'Open' : 'Closed'}</span><span className="pill muted">Coil {p?.coil_cmd ? 'Energized' : 'De-energized'}</span></div>
      </div>
    </section>
    <div className="monitor-grid">
      <div className="monitor-main">
        <div className="metric-grid">
          <Metric label="Voltage V" value={fmt(t?.v_rms)} unit="V" tone="voltage"/>
          <Metric label="Current A" value={fmt(t?.i_rms, 3)} unit="A" tone="current"/>
          <Metric label="Active Power W" value={fmt(t?.p_w, 1)} unit="W"/>
          <Metric label="Frequency Hz" value={fmt(t?.freq_hz, 2)} unit="Hz"/>
        </div>
        <div className="secondary-strip">
          <div><span>Power Factor</span><strong>{zeroLoad ? '—' : fmt(t?.pf, 3)}</strong></div>
          <div><span>Reactive Q1</span><strong>{fmt(t?.q1_var, 1)} <small>var</small></strong></div>
          <div><span>Apparent S</span><strong>{fmt(t?.s_va, 1)} <small>VA</small></strong></div>
          <div><span>Current THD</span><strong>{zeroLoad ? '0.0%' : `${fmt(t?.thd_i_pct, 1)}%`}</strong></div>
          <div><span>Thermal</span><strong>{fmt(t?.thermal_pct, 1)}%</strong></div>
        </div>
        <Card title="50 Hz Oscilloscope" action={<span className="chart-key"><i className="key-blue"/>v(t)<i className="key-teal"/>i(t)</span>}>
          <div className="scope-meta"><span>Voltage axis · ±400 V</span><span>Current axis · ±{iLimit} A</span></div>
          <div className="chart-scope">{data.waveform ? <ReactECharts option={scopeOption} style={{ height: '100%', width: '100%' }} notMerge/> : <div className="empty-state">Waiting for waveform samples</div>}</div>
        </Card>
      </div>
      <Card title="Controls" className="controls-card">
        <div className="control-section"><div className="section-label">Appliances</div><div className="appliance-switches">{appliances.map((item, index) => <button key={item.name} className={`appliance-switch ${states[index] ? 'selected' : ''}`} aria-pressed={states[index]} onClick={() => onToggle(index)}><span className="appliance-copy"><span className="appliance-icon"><Zap size={16}/></span><span><strong>{item.name}</strong><small>{item.detail}</small></span></span><span className={`switch ${states[index] ? 'on' : ''}`}><i/></span></button>)}</div></div>
        <div className="control-section"><label className="section-label" htmlFor="feedback-mode">Feedback mode</label><select id="feedback-mode" className="select" value={feedbackMode} onChange={e => onFeedback(Number(e.target.value))}>{feedbackModes.map((name, index) => <option value={index} key={name}>{name}</option>)}</select><p className="hint">Synced from live relay protection state.</p></div>
        <div className="control-section"><div className="section-label">Fault injection</div><div className="fault-buttons">{faultActions.map(fault => <button key={fault.label} className={`fault-button ${fault.scenario === 0 ? 'clear' : ''}`} disabled={busy} onClick={() => onFault(fault.scenario, fault.magnitude, fault.label)}>{fault.label}</button>)}</div></div>
        {toast && <div className={`toast ${toast.startsWith('Error') ? 'error' : 'success'}`} role="status">{toast.startsWith('Error') ? <AlertTriangle size={15}/> : <Check size={15}/>}<span>{toast}</span></div>}
      </Card>
    </div>
  </div>
}

function NilmAndFaults({ data, paused }: { data: GridData; paused: boolean }) {
  const n = data.nilm
  const [records, setRecords] = useState<FaultRecord[]>([])
  const [switchEvents, setSwitchEvents] = useState<Array<{ key: string; time: string; name: string; on: boolean; watts: number; confidence: number; distance: number }>>([])
  const previousEvent = useRef<{ mask: number; id?: number; type?: number } | null>(null)
  useEffect(() => {
    if (paused) return
    let alive = true
    const load = async () => { try { const response = await fetch('/api/faults'); if (response.ok) { const rows = await response.json() as FaultRecord[]; if (alive) setRecords(rows) } } catch { /* keep the latest successful history */ } }
    void load(); const timer = window.setInterval(() => void load(), 3000)
    return () => { alive = false; window.clearInterval(timer) }
  }, [paused])
  const ordered = useMemo(() => Array.from(new Map(records.map(row => [row.record_id, row])).values()).sort((a, b) => b.ts_ms - a.ts_ms || b.record_id - a.record_id), [records])
  const harmonics = data.telemetry?.harm_i_rms ?? []
  const maxHarmonic = Math.max(...harmonics, 0)
  const harmonicOption = useMemo(() => ({ animation: false, grid: { left: 52, right: 20, top: 36, bottom: 28 }, tooltip: { trigger: 'axis' }, xAxis: { type: 'category', data: ['h1', 'h3', 'h5', 'h7', 'h9'], axisLabel: { color: '#78716C', fontSize: 12 }, axisLine: { lineStyle: { color: '#E7E2D8' } } }, yAxis: { type: 'value', min: 0, max: Math.max(1.0, maxHarmonic * 1.2), name: 'A RMS', nameTextStyle: { color: '#57534E', fontSize: 12 }, axisLabel: { color: '#78716C', fontSize: 12, formatter: (value: number) => Number(value).toFixed(1) }, splitLine: { lineStyle: { color: '#E7E2D8' } } }, series: [{ type: 'bar', data: harmonics, barWidth: '48%', itemStyle: { color: '#0F766E', borderRadius: [4, 4, 0, 0] } }] }), [harmonics, maxHarmonic])
  const powers = appliances.map((_, i) => Math.max(0, n?.est_power_w[i] ?? 0))
  const residual = Math.max(0, n?.unknown_power_w ?? 0)
  const totalPower = powers.reduce((sum, watts) => sum + watts, 0) + residual
  const donutData = [...appliances.map((item, i) => ({ name: item.name === 'Refrigerator' ? 'Fridge' : item.name, value: powers[i], itemStyle: { color: item.color } })), { name: 'Unknown Residual', value: residual, itemStyle: { color: '#A8A29E' } }]
  const donutOption = useMemo(() => ({ animation: false, tooltip: { trigger: 'item', formatter: (params: { name: string; value: number; percent: number }) => `${params.name}<br/><b>${params.value.toFixed(1)} W</b> · ${params.percent.toFixed(1)}%` }, legend: { show: false }, series: [{ type: 'pie', radius: ['62%', '82%'], center: ['50%', '50%'], avoidLabelOverlap: true, label: { show: false }, itemStyle: { borderColor: '#FFFDF9', borderWidth: 3 }, data: donutData }], graphic: [{ type: 'text', left: 'center', top: '40%', style: { text: `${totalPower.toFixed(0)} W`, fill: '#1C1917', font: '700 25px Plus Jakarta Sans' } }, { type: 'text', left: 'center', top: '53%', style: { text: 'ACTIVE LOAD', fill: '#78716C', font: '600 10px Plus Jakarta Sans', letterSpacing: 1 } }] }), [donutData, totalPower])
  useEffect(() => {
    if (!n) return
    const prev = previousEvent.current
    const current = { mask: n.active_mask, id: n.last_event_id, type: n.last_event_type }
    if (!prev) { previousEvent.current = current; return }
    const maskChanged = prev.mask !== current.mask
    const eventChanged = current.id !== undefined && current.type !== undefined && (prev.id !== current.id || prev.type !== current.type)
    if (maskChanged || eventChanged) {
      let index = eventChanged ? current.id! : 0
      if (!eventChanged) { const changed = prev.mask ^ current.mask; index = Math.max(0, Math.log2(changed & -changed) | 0) }
      if (index < 5) {
        const on = eventChanged ? current.type === 1 : Boolean(current.mask & (1 << index))
        const confidence = Math.max(0, Math.min(1, n.confidence[index] ?? 0))
        const key = `${n.ts_ms}:${current.id}:${current.type}:${current.mask}`
        setSwitchEvents(existing => [{ key, time: new Date().toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: '2-digit' }), name: applianceName(index), on, watts: index < 4 ? Math.max(0, n.est_power_w[index] ?? 0) : residual, confidence, distance: n.last_mahalanobis }, ...existing].slice(0, 10))
      }
    }
    previousEvent.current = current
  }, [n])
  const health = data.health
  const healthItems: Array<[string, number | undefined, string]> = [['DSP µs', health?.dsp_exec_us, 'µs'], ['NILM µs', n?.inference_us, 'µs'], ['CRC Errors', health?.uart_crc_err, ''], ['Dropped Frames', health?.dropped_frames, '']]
  return <div className="page-stack">
      <div className="nilm-top">
      <div className="nilm-appliance-column"><div className="nilm-grid">{appliances.map((item, index) => {
        const on = Boolean((n?.active_mask ?? 0) & (1 << index)); const confidence = Math.max(0, Math.min(1, n?.confidence[index] ?? 0)); const watts = powers[index]; const share = totalPower > 0 ? watts / totalPower * 100 : 0; const energy = n?.est_energy_wh?.[index] ?? 0
        return <article className={`nilm-card ${on ? 'is-on' : 'is-off'}`} key={item.name}><div className="nilm-card-top"><div><h2>{item.name === 'Refrigerator' ? 'Fridge' : item.name}</h2><span className="muted-text">Rated {item.detail}</span></div><span className={`pill ${on ? 'good' : 'muted'}`}>{on ? 'ON' : 'OFF'}</span></div><div className="load-behavior" style={{ '--load-color': item.color } as React.CSSProperties}>{item.behavior}</div><div className="nilm-primary-readout"><div className="nilm-watts">{fmt(watts, 1)}<span>W</span></div><span className="nilm-share">{share.toFixed(1)}%<small> load share</small></span></div><div className="nilm-stats"><div><span>Session Energy</span><strong>{fmt(energy, 2)} <small>Wh</small></strong></div><div><span>Est. Operating Cost</span><strong>₹{(watts * 7.5 / 1000).toFixed(2)} <small>/hr</small></strong></div></div><div className={`confidence-block ${on ? '' : 'dimmed'}`}><div className="confidence-line"><span>Classifier confidence</span><strong>{n ? `${(confidence * 100).toFixed(1)}%` : '—'}</strong></div><div className="confidence-track"><i style={{ width: `${confidence * 100}%`, backgroundColor: item.color }}/></div></div></article>
      })}</div></div>
      <div className="nilm-chart-column"><Card title="Odd Current Harmonics"><div className="harmonic-chart">{data.telemetry ? <ReactECharts option={harmonicOption} style={{ height: '100%', width: '100%' }} notMerge/> : <div className="empty-state">Waiting for telemetry</div>}</div></Card><Card title="Active Power Breakdown"><div className="power-donut-wrap"><ReactECharts option={donutOption} style={{ height: 230, width: '100%' }} notMerge/><div className="donut-legend">{donutData.map((entry, i) => <div key={entry.name}><i style={{ backgroundColor: appliances[i]?.color ?? '#A8A29E' }}/><span>{entry.name}</span><strong>{totalPower ? `${(entry.value / totalPower * 100).toFixed(1)}%` : '0.0%'}</strong></div>)}</div></div></Card></div>
    </div>
    <Card title="NILM Classifier Diagnostics"><div className="diagnostic-strip"><div className="diagnostic-pill"><span>Last Detected Event</span><strong>{n?.last_event_id !== undefined && n.last_event_id < 5 && (n.last_event_type === 1 || n.last_event_type === 2) ? `${applianceName(n.last_event_id)} · ${n.last_event_type === 1 ? 'Turned ON' : 'Turned OFF'}` : 'No switching event'}</strong></div><div className="diagnostic-pill"><span>Mahalanobis Distance · d<sub>M</sub></span><strong>{fmt(n?.last_mahalanobis, 2)}</strong><div className="distance-track"><i style={{ width: `${Math.max(0, Math.min(100, (n?.last_mahalanobis ?? 0) / 8 * 100))}%` }}/></div><small>Unknown threshold · 8.0</small></div><div className="diagnostic-pill"><span>On-Chip Inference · RP2350 Cortex-M33</span><strong>{fmt(n?.inference_us, 0)} <small>µs</small></strong></div><div className="diagnostic-pill"><span>Unknown Residual</span><strong>{fmt(residual, 1)} <small>W</small></strong></div></div></Card>
    <Card title="Recent NILM Switching Events"><div className="event-list">{switchEvents.length ? switchEvents.map(event => <div className="event-row" key={event.key}><time>{event.time}</time><strong>{event.name}</strong><span className={`event-state ${event.on ? 'event-on' : 'event-off'}`}>{event.on ? 'Turned ON' : 'Turned OFF'}</span><span>{event.watts.toFixed(1)} W</span><span>{(event.confidence * 100).toFixed(1)}%</span><span>d<sub>M</sub> {event.distance.toFixed(2)}</span></div>) : <div className="event-empty">Waiting for appliance switching events</div>}</div></Card>
    <Card title="Persistent NVS Fault Log" action={<a className="button button-secondary export-button" href="/api/faults/export.csv"><Download size={15}/>Export CSV</a>}>
      <div className="table-wrap"><table><thead><tr>{['Record', 'Time', 'Fault', 'Relay result', 'Trigger', 'Source V', 'Load V', 'Verify'].map(x => <th key={x}>{x}</th>)}</tr></thead><tbody>{ordered.length ? ordered.map(row => <tr key={row.record_id}><td>{row.record_id}</td><td>{formatUptime(row.ts_ms)}</td><td><span className={`pill ${faultTone(row.fault_code)}`}>{faultNames[row.fault_code] ?? `Fault ${row.fault_code}`}</span></td><td><span className={`pill ${relayTone(row.relay_result)}`}>{relayNames[row.relay_result] ?? `Relay ${row.relay_result}`}</span></td><td>{fmt(row.trigger_val, 2)}</td><td>{fmt(row.v_src_v, 1)} V</td><td>{fmt(row.v_load_v, 1)} V</td><td>{fmt(row.verify_ms, 0)} ms</td></tr>) : <tr><td colSpan={8} className="empty-table">No stored fault records</td></tr>}</tbody></table></div>
    </Card>
    <section className="health-strip"><div className="health-title"><Activity size={17}/><strong>Controller Health</strong></div>{healthItems.map(([label, value, unit]) => <div className="health-item" key={label}><span>{label}</span><strong>{value === undefined ? '—' : `${value.toLocaleString()} ${unit}`}</strong></div>)}</section>
  </div>
}

export default function App() {
  const [tab, setTab] = useState<'monitor' | 'nilm'>('monitor')
  const [paused, setPaused] = useState(false)
  const [applianceStates, setApplianceStates] = useState<[boolean, boolean, boolean, boolean]>([false, false, false, false])
  const [feedbackMode, setFeedbackMode] = useState(2)
  const applianceTouched = useRef<[boolean, boolean, boolean, boolean]>([false, false, false, false])
  const [toast, setToast] = useState('')
  const [busy, setBusy] = useState(false)
  const { state, command } = useGridGuardWs(paused)
  useEffect(() => { if (state.nilm) setApplianceStates(current => current.map((value, index) => applianceTouched.current[index] ? value : Boolean(state.nilm && (state.nilm.active_mask & (1 << index)))) as [boolean, boolean, boolean, boolean]) }, [state.nilm])
  useEffect(() => { if (state.protection) setFeedbackMode(state.protection.feedback_mode) }, [state.protection?.feedback_mode])
  useEffect(() => { if (!toast) return; const timer = window.setTimeout(() => setToast(''), 3500); return () => window.clearTimeout(timer) }, [toast])
  const sendCommand = async (endpoint: string, body: object, success: string) => {
    try { await command(endpoint, body); setToast(success) }
    catch (error) { setToast(`Error: ${error instanceof Error ? error.message : 'Request failed'}`) }
  }
  const toggleAppliance = async (index: number) => {
    applianceTouched.current[index] = true
    const target = !applianceStates[index]
    setApplianceStates(current => current.map((value, i) => i === index ? target : value) as [boolean, boolean, boolean, boolean])
    try { await command('appliance', { appliance_id: index, target_state: target }); setToast(`${appliances[index].name} switched ${target ? 'on' : 'off'}`) }
    catch (error) { setApplianceStates(current => current.map((value, i) => i === index ? !target : value) as [boolean, boolean, boolean, boolean]); setToast(`Error: ${error instanceof Error ? error.message : 'Request failed'}`) }
  }
  const onFeedback = (mode: number) => { setFeedbackMode(mode); void sendCommand('feedback-mode', { feedback_mode: mode }, 'Feedback mode updated') }
  const status = state.mode === 'REPLAY' ? 'Replay · Pico 2 W' : 'Live · Pico 2 W'
  return <div className="app-shell">
    <header className="topbar"><div className="topbar-inner"><div className="brand"><span className="brand-icon"><CircuitBoard size={19}/></span><span>GridGuard</span></div><nav className="tabs" aria-label="Main navigation"><button className={tab === 'monitor' ? 'active' : ''} onClick={() => setTab('monitor')}>Live Monitor</button><button className={tab === 'nilm' ? 'active' : ''} onClick={() => setTab('nilm')}>NILM &amp; Fault Log</button></nav><div className="header-actions"><span className={`live-pill ${state.connected ? '' : 'disconnected'}`}><i/>{status}</span><button className="button button-secondary pause-button" onClick={() => setPaused(value => !value)}>{paused ? 'Resume' : 'Pause'}<span>{paused ? '▶' : 'Ⅱ'}</span></button></div></div></header>
    <main className="content"><div className="page-heading"><div><p className="eyebrow">GridGuard protection instrument</p><h1>{tab === 'monitor' ? 'Live Monitor' : 'NILM & Fault Log'}</h1></div><span className="connection-text">{paused ? 'Readouts paused' : state.statusText}</span></div>
      {tab === 'monitor' ? <Overview data={state} states={applianceStates} onToggle={index => void toggleAppliance(index)} feedbackMode={feedbackMode} onFeedback={onFeedback} onFault={(scenario, magnitude, label) => void sendCommand('fault', { fault_scenario: scenario, magnitude }, `${label} command accepted`)} onReset={() => { setBusy(true); void sendCommand('reset-relay', { force_clear_latch: false }, 'Safe reset command accepted').finally(() => setBusy(false)) }} toast={toast} busy={busy}/> : <NilmAndFaults data={state} paused={paused}/>}</main>
    <footer className="footer"><span>GridGuard · RP2350A protection controller</span><span className="tabular">{state.telemetry ? `Telemetry frame ${state.telemetry.ts_ms}` : 'Waiting for telemetry'}</span></footer>
  </div>
}
