'use client'
import { SystemSnapshot } from '@/types/adas'
import { threatText, threatBg } from '@/lib/threatColors'
import { Wifi, WifiOff, Activity } from 'lucide-react'

interface Props {
  snapshot:        SystemSnapshot | null
  connected:       boolean
  fps:             number
  onSceneSwitch?:  (idx: number) => void
  activeSceneIdx?: number
}

export default function StatusBar({ snapshot, connected, fps, onSceneSwitch, activeSceneIdx = 0 }: Props) {
  const threat = (snapshot?.overall_threat ?? 'SAFE') as keyof typeof threatBg

  return (
    <div className={`flex items-center justify-between px-4 py-2 border-b ${threatBg[threat]} border transition-colors duration-300`}>
      <div className="flex items-center gap-3">
        {connected ? <Wifi className="w-4 h-4 text-green-400" /> : <WifiOff className="w-4 h-4 text-red-400 animate-pulse" />}
        <span className={`text-sm font-mono ${connected ? 'text-green-400' : 'text-red-400'}`}>
          {connected ? 'CONNECTED' : 'DISCONNECTED'}
        </span>
        <span className="text-gray-500 text-sm font-mono">{fps} fps</span>
      </div>

      <div className="flex items-center gap-3">
        {['DAY','RAIN','NIGHT'].map((name, idx) => (
          <button
            key={name}
            onClick={() => onSceneSwitch?.(idx)}
            className={`px-3 py-1 text-xs font-mono rounded border transition-colors ${
              activeSceneIdx === idx
                ? 'bg-blue-600 border-blue-400 text-white'
                : 'bg-gray-800 border-gray-600 text-gray-400 hover:border-blue-500 hover:text-white'
            }`}
          >
            {name}
          </button>
        ))}
        <span className="text-gray-400 text-xs font-mono">
          Frame {snapshot?.frame_idx ?? 0}/{snapshot?.total_frames ?? 0} · {snapshot?.fused_objects?.length ?? 0} tracks
        </span>
      </div>

      <div className="flex items-center gap-2">
        <Activity className={`w-4 h-4 ${threatText[threat]}`} />
        <span className={`text-sm font-bold font-mono ${threatText[threat]}`}>{threat}</span>
      </div>
    </div>
  )
}
