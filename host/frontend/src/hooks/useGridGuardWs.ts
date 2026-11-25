import { useCallback, useEffect, useRef, useState } from 'react'
import type { EventMessage, GridData, Health, Nilm, Protection, Telemetry, Waveform } from '../types'

const initial: GridData = { mode: null, connected: false, statusText: 'Connecting to GridGuard API', telemetry: null, protection: null, waveform: null, nilm: null, health: null, lastFrameAt: null, history: [] }
type Buffered = { mode: EventMessage['mode']; items: Record<string, Record<string, unknown>> }

export function useGridGuardWs(paused = false) {
  const [state, setState] = useState<GridData>(initial)
  const socketRef = useRef<WebSocket | null>(null)
  const pendingRef = useRef<Buffered>({ mode: 'LIVE', items: {} })
  const pausedRef = useRef(paused)
  pausedRef.current = paused

  useEffect(() => {
    let active = true
    let retry: number | undefined
    let lastEnergyTimestamp: number | undefined
    let applianceEnergy = [0, 0, 0, 0]
    const connect = () => {
      const scheme = window.location.protocol === 'https:' ? 'wss:' : 'ws:'
      const socket = new WebSocket(`${scheme}//${window.location.host}/ws/live`)
      socketRef.current = socket
      socket.onopen = () => { pendingRef.current.items.status = { connected: true }; pendingRef.current.mode = 'LIVE' }
      socket.onmessage = (event: MessageEvent<string>) => {
        try {
          const msg = JSON.parse(event.data) as EventMessage
          pendingRef.current.mode = msg.mode
          pendingRef.current.items[msg.kind] = msg.data
        } catch { /* ignore malformed transport frames */ }
      }
      socket.onclose = () => {
        if (!active) return
        pendingRef.current.items.status = { connected: false, detail: 'WebSocket disconnected; reconnecting' }
        retry = window.setTimeout(connect, 1500)
      }
      socket.onerror = () => socket.close()
    }
    const flush = window.setInterval(() => {
      if (pausedRef.current) return
      const buffered = pendingRef.current
      if (!Object.keys(buffered.items).length) return
      pendingRef.current = { mode: buffered.mode, items: {} }
      setState(current => {
        const next: GridData = { ...current, mode: buffered.mode }
        const status = buffered.items.status
        if (status) {
          next.connected = Boolean(status.connected)
          next.statusText = next.connected ? 'Pico CDC connected' : String(status.detail ?? 'Waiting for Pico CDC')
        }
        const telemetryData = buffered.items.telemetry
        if (telemetryData) {
          const telemetry = telemetryData as unknown as Telemetry
          if (lastEnergyTimestamp !== undefined && next.nilm) {
            const dtHours = Math.max(0, telemetry.ts_ms - lastEnergyTimestamp) / 3_600_000
            applianceEnergy = applianceEnergy.map((energy, index) => energy + (((next.nilm?.active_mask ?? 0) & (1 << index)) ? (next.nilm?.est_power_w[index] ?? 0) * dtHours : 0))
            next.nilm = { ...next.nilm, est_energy_wh: [...applianceEnergy] }
          }
          lastEnergyTimestamp = telemetry.ts_ms
          next.telemetry = telemetry
          next.lastFrameAt = Date.now()
        }
        const protectionData = buffered.items.protection
        if (protectionData) {
          const protection = protectionData as unknown as Protection
          next.protection = protection
          next.history = [...current.history, { ts: Number(protectionData.ts_ms), source: Number(protectionData.v_src_v), load: Number(protectionData.v_load_v) }].slice(-120)
        }
        if (buffered.items.waveform) next.waveform = buffered.items.waveform as unknown as Waveform
        if (buffered.items.nilm) next.nilm = { ...(buffered.items.nilm as unknown as Nilm), est_energy_wh: next.nilm?.est_energy_wh ?? applianceEnergy }
        if (buffered.items.health) next.health = buffered.items.health as unknown as Health
        if (buffered.mode === 'REPLAY') next.connected = true
        return next
      })
    }, 400)
    connect()
    return () => { active = false; window.clearTimeout(retry); window.clearInterval(flush); socketRef.current?.close() }
  }, [])

  const command = useCallback(async (endpoint: string, body: object) => {
    const response = await fetch(`/api/cmd/${endpoint}`, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) })
    if (!response.ok) {
      let detail = `Command failed (${response.status})`
      try { const payload = await response.json() as { detail?: string }; detail = payload.detail ?? detail } catch { /* keep status fallback */ }
      throw new Error(detail)
    }
    return response.json() as Promise<{ accepted: boolean; mode: string }>
  }, [])
  return { state, command }
}
