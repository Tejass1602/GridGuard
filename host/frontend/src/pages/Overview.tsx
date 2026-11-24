import { useMemo } from 'react'
import ReactECharts from 'echarts-for-react'
import { Panel, MetricCard, StatusBanner, EmptyChart } from '../components'
import type { GridData } from '../types'

const fmt = (value: number | undefined, digits = 1) => value === undefined || !Number.isFinite(value) ? '—' : value.toFixed(digits)

export default function Overview({ data }: { data: GridData }) {
  const t = data.telemetry; const p = data.protection
  const barOption = useMemo(() => ({ animation: false, grid: { left: 44, right: 18, top: 16, bottom: 25 }, xAxis: { type: 'value', max: 15, axisLabel: { color: '#64748b' }, splitLine: { lineStyle: { color: '#1e293b' } } }, yAxis: { type: 'category', data: ['Source', 'Load'], axisLabel: { color: '#94a3b8' } }, series: [{ type: 'bar', data: [p?.v_src_v, p?.v_load_v].map(v => v ?? 0), barWidth: 18, itemStyle: { color: (params: { dataIndex: number }) => params.dataIndex === 0 ? '#38bdf8' : '#a78bfa', borderRadius: [0, 4, 4, 0] }, label: { show: true, position: 'right', color: '#cbd5e1', formatter: (x: { value: number }) => `${x.value.toFixed(2)} V` } }] }), [p?.v_src_v, p?.v_load_v])
  const historyOption = useMemo(() => ({ animation: false, grid: { left: 45, right: 14, top: 12, bottom: 25 }, xAxis: { type: 'category', data: data.history.map((_, i) => i), axisLabel: { show: false }, boundaryGap: false }, yAxis: { type: 'value', max: 15, min: 0, axisLabel: { color: '#64748b' }, splitLine: { lineStyle: { color: '#1e293b' } } }, series: [{ name: 'Source', type: 'line', showSymbol: false, data: data.history.map(x => x.source), lineStyle: { color: '#38bdf8', width: 2 } }, { name: 'Load', type: 'line', showSymbol: false, data: data.history.map(x => x.load), lineStyle: { color: '#a78bfa', width: 2 } }], tooltip: { trigger: 'axis' }, legend: { textStyle: { color: '#94a3b8' }, right: 0, top: -2 } }), [data.history])
  return <div className="space-y-4">
    <StatusBanner data={data} />
    <div className="grid gap-4 xl:grid-cols-[1.25fr_.75fr]">
      <Panel title="IEEE 1459 · Live metrology"><div className="grid grid-cols-2 gap-3 sm:grid-cols-3 lg:grid-cols-4">
        <MetricCard label="Voltage RMS" value={fmt(t?.v_rms)} unit="V" /> <MetricCard label="Current RMS" value={fmt(t?.i_rms, 3)} unit="A" />
        <MetricCard label="Active power" value={fmt(t?.p_w)} unit="W" /> <MetricCard label="Fundamental Q₁" value={fmt(t?.q1_var)} unit="var" />
        <MetricCard label="Apparent power" value={fmt(t?.s_va)} unit="VA" /> <MetricCard label="True power factor" value={fmt(t?.pf, 3)} />
        <MetricCard label="Grid frequency" value={fmt(t?.freq_hz, 2)} unit="Hz" /> <MetricCard label="Current THD" value={fmt(t?.thd_i_pct)} unit="%" tone={(t?.thd_i_pct ?? 0)>35?'text-amber-300':'text-slate-100'} />
      </div></Panel>
      <Panel title="Independent disconnect feedback"><div className="h-[180px]">{p ? <ReactECharts option={barOption} style={{height:'100%',width:'100%'}} notMerge /> : <EmptyChart label="AWAITING ADC / FEEDBACK FRAME" />}</div>
        <div className="mt-2 flex flex-wrap gap-2 text-xs"><span className={`rounded border px-2 py-1 ${p?.aux_state ? 'border-emerald-700 text-emerald-300':'border-slate-700 text-slate-500'}`}>AUX {p?.aux_state ? 'OPEN':'UNAVAILABLE / CLOSED'}</span><span className="rounded border border-slate-700 px-2 py-1 text-slate-400">{p ? `MODE ${p.feedback_mode}` : 'NO FEEDBACK'}</span></div>
      </Panel>
    </div>
    <div className="grid gap-4 lg:grid-cols-[1fr_1.3fr]">
      <Panel title="IEC 60255 · Thermal accumulator"><div className="flex items-end justify-between"><div className="metric text-3xl font-semibold">{fmt(t?.thermal_pct)}<span className="ml-1 text-sm text-slate-500">%</span></div><div className="eyebrow">EI · pickup 8.0 A · TMS 0.15</div></div><div className="mt-4 h-2 overflow-hidden rounded-full bg-slate-800"><div className={`h-full rounded-full transition-all ${(t?.thermal_pct ?? 0)>80?'bg-rose-500':'bg-amber-400'}`} style={{width:`${Math.min(100,Math.max(0,t?.thermal_pct ?? 0))}%`}} /></div><div className="mt-3 flex justify-between text-[11px] text-slate-500"><span>COOL</span><span>TRIP THRESHOLD</span></div></Panel>
      <Panel title="Source / load voltage trend"><div className="h-[150px]">{data.history.length ? <ReactECharts option={historyOption} style={{height:'100%',width:'100%'}} notMerge /> : <EmptyChart label="TREND BEGINS WITH FIRST PROTECTION FRAME" />}</div></Panel>
    </div>
  </div>
}
