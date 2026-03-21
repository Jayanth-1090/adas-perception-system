'use client'
import { useEffect, useRef, useState, useCallback } from 'react'
import { SystemSnapshot } from '@/types/adas'

interface StreamState {
  snapshot:    SystemSnapshot | null
  connected:   boolean
  fps:         number
  error:       string | null
}

export function useADASStream(url: string): StreamState {
  const [snapshot,  setSnapshot]  = useState<SystemSnapshot | null>(null)
  const [connected, setConnected] = useState(false)
  const [fps,       setFps]       = useState(0)
  const [error,     setError]     = useState<string | null>(null)

  const wsRef        = useRef<WebSocket | null>(null)
  const frameCount   = useRef(0)
  const fpsTimer     = useRef<NodeJS.Timeout | null>(null)
  const reconnectRef = useRef<NodeJS.Timeout | null>(null)

  const connect = useCallback(() => {
    try {
      const ws = new WebSocket(url)
      wsRef.current = ws

      ws.onopen = () => {
        setConnected(true)
        setError(null)
        // FPS counter — measure over 2s window for stability
        fpsTimer.current = setInterval(() => {
          setFps(Math.round(frameCount.current / 2))
          frameCount.current = 0
        }, 2000)
      }

      ws.onmessage = (e) => {
        try {
          const data = JSON.parse(e.data) as SystemSnapshot
          setSnapshot(data)
          frameCount.current++
        } catch {}
      }

      ws.onclose = () => {
        setConnected(false)
        if (fpsTimer.current) clearInterval(fpsTimer.current)
        // Reconnect after 2s
        reconnectRef.current = setTimeout(connect, 2000)
      }

      ws.onerror = () => {
        setError('WebSocket connection failed — is the simulator running?')
      }
    } catch (e) {
      setError('Failed to connect')
    }
  }, [url])

  useEffect(() => {
    connect()
    return () => {
      wsRef.current?.close()
      if (fpsTimer.current)     clearInterval(fpsTimer.current)
      if (reconnectRef.current) clearTimeout(reconnectRef.current)
    }
  }, [connect])

  return { snapshot, connected, fps, error }
}
