import { useCallback, useEffect, useRef, useState } from 'react'
import type { EventMessage, GridData, Health, Nilm, Protection, Telemetry, Waveform } from '../types'

const initial: GridData = { mode: null, connected: false, statusText: 'Connecting to GridGuard API', telemetry: null, protection: null, waveform: null, nilm: null, health: null, history: [] }

export function useGridGuardWs() {
  const [state, setState] = useState<GridData>(initial)
  const socketRef = useRef<WebSocket | null>(null)
  useEffect(() => {
    let active = true
    let retry: number | undefined
    let lastEnergyTimestamp: number | undefined
    let applianceEnergy = [0, 0, 0, 0]
    const connect = () => {
      const scheme = window.location.protocol === 'https:' ? 'wss:' : 'ws:'
      const socket = new WebSocket(`${scheme}//${window.location.host}/ws/live`)
      socketRef.current = socket
      socket.onopen = () => setState((s) => ({ ...s, statusText: 'Transport connected' }))
      socket.onmessage = (event: MessageEvent<string>) => {
        const msg = JSON.parse(event.data) as EventMessage
        setState((s) => {
          const next: GridData = { ...s, mode: msg.mode }
          if (msg.kind === 'status') {
            const connected = Boolean(msg.data.connected)
            next.connected = connected
            next.statusText = connected ? 'Pico CDC connected' : String(msg.data.detail ?? 'Waiting for Pico CDC')
          } else if (msg.kind === 'telemetry') {
            const telemetry = msg.data as unknown as Telemetry
            if (lastEnergyTimestamp !== undefined && s.nilm) {
              const dtHours = Math.max(0, telemetry.ts_ms - lastEnergyTimestamp) / 3_600_000
              applianceEnergy = applianceEnergy.map((energy, index) => energy + (((s.nilm?.active_mask ?? 0) & (1 << index)) ? (s.nilm?.est_power_w[index] ?? 0) * dtHours : 0))
              next.nilm = { ...s.nilm, est_energy_wh: [...applianceEnergy] }
            }
            lastEnergyTimestamp = telemetry.ts_ms
            next.telemetry = telemetry
          }
          else if (msg.kind === 'protection') {
            next.protection = msg.data as unknown as Protection
            next.history = [...s.history, { ts: Number(msg.data.ts_ms), source: Number(msg.data.v_src_v), load: Number(msg.data.v_load_v) }].slice(-120)
          } else if (msg.kind === 'waveform') next.waveform = msg.data as unknown as Waveform
          else if (msg.kind === 'nilm') next.nilm = { ...(msg.data as unknown as Nilm), est_energy_wh: s.nilm?.est_energy_wh ?? applianceEnergy }
          else if (msg.kind === 'health') next.health = msg.data as unknown as Health
          next.connected = msg.mode === 'REPLAY' || s.connected
          return next
        })
      }
      socket.onclose = () => {
        if (!active) return
        setState((s) => ({ ...s, connected: false, statusText: 'WebSocket disconnected; reconnecting' }))
        retry = window.setTimeout(connect, 1500)
      }
      socket.onerror = () => socket.close()
    }
    connect()
    return () => { active = false; window.clearTimeout(retry); socketRef.current?.close() }
  }, [])

  const command = useCallback(async (path: string, body: object) => {
    const response = await fetch(`/api/cmd/${path}`, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) })
    if (!response.ok) {
      const detail = await response.text()
      throw new Error(detail || `Command failed (${response.status})`)
    }
    return response.json() as Promise<{ accepted: boolean; mode: string }>
  }, [])
  return { state, command }
}
