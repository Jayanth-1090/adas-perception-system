'use client'
import { useState, useEffect, useRef } from 'react'
import { SystemSnapshot } from '@/types/adas'
import { Brain, AlertTriangle, CheckCircle } from 'lucide-react'

interface Props { snapshot: SystemSnapshot | null }

interface DiagResult {
  analysis:  string
  timestamp: number
  threat:    string
}

export default function DiagnosticPanel({ snapshot }: Props) {
  const [result,   setResult]   = useState<DiagResult | null>(null)
  const [loading,  setLoading]  = useState(false)
  const lastCritical = useRef<number>(0)

  // Auto-trigger on CRITICAL — throttled to once per 10s
  useEffect(() => {
    if (!snapshot) return
    if (snapshot.overall_threat !== 'CRITICAL') return
    const now = Date.now()
    if (now - lastCritical.current < 10000) return
    lastCritical.current = now
    runDiagnostic(snapshot)
  }, [snapshot?.overall_threat])

  const runDiagnostic = async (snap: SystemSnapshot) => {
    setLoading(true)
    try {
      const res = await fetch('/api/diagnostic', {
        method:  'POST',
        headers: { 'Content-Type': 'application/json' },
        body:    JSON.stringify({ snapshot: snap }),
      })
      const data = await res.json()
      setResult({
        analysis:  data.analysis,
        timestamp: Date.now(),
        threat:    snap.overall_threat,
      })
    } catch (e) {
      setResult({
        analysis:  'Diagnostic unavailable — check API key in .env.local',
        timestamp: Date.now(),
        threat:    snap.overall_threat,
      })
    } finally {
      setLoading(false)
    }
  }

  return (
    <div className="flex flex-col h-full bg-gray-900/50 rounded-lg border border-gray-700 p-3">
      {/* Header */}
      <div className="flex items-center justify-between mb-3">
        <div className="flex items-center gap-2">
          <Brain className="w-4 h-4 text-purple-400" />
          <span className="text-sm font-mono font-bold text-purple-400">
            AI DIAGNOSTICS
          </span>
        </div>
        <button
          onClick={() => snapshot && runDiagnostic(snapshot)}
          disabled={loading || !snapshot}
          className="text-xs px-2 py-1 rounded bg-purple-900/50 border border-purple-700
                     text-purple-300 hover:bg-purple-800/50 disabled:opacity-40
                     transition-colors font-mono"
        >
          {loading ? 'Analyzing...' : 'Analyze Now'}
        </button>
      </div>

      {/* Status */}
      {!result && !loading && (
        <div className="flex-1 flex flex-col items-center justify-center text-center gap-2">
          <AlertTriangle className="w-8 h-8 text-gray-600" />
          <p className="text-gray-500 text-xs font-mono">
            Auto-triggers on CRITICAL events
          </p>
          <p className="text-gray-600 text-xs font-mono">
            or click Analyze Now
          </p>
        </div>
      )}

      {loading && (
        <div className="flex-1 flex items-center justify-center">
          <div className="text-center">
            <div className="w-6 h-6 border-2 border-purple-500 border-t-transparent
                            rounded-full animate-spin mx-auto mb-2" />
            <p className="text-purple-400 text-xs font-mono">
              Claude is analyzing...
            </p>
          </div>
        </div>
      )}

      {result && !loading && (
        <div className="flex-1 overflow-auto">
          <div className="flex items-center gap-2 mb-2">
            <CheckCircle className="w-3 h-3 text-green-400 flex-shrink-0" />
            <span className="text-xs text-gray-400 font-mono">
              {new Date(result.timestamp).toLocaleTimeString()} — {result.threat}
            </span>
          </div>
          <p className="text-xs text-gray-300 font-mono leading-relaxed
                        whitespace-pre-wrap">
            {result.analysis}
          </p>
        </div>
      )}
    </div>
  )
}
