'use client'
import { SystemSnapshot } from '@/types/adas'
import { threatText, threatBg } from '@/lib/threatColors'
import { Wifi, WifiOff, Activity } from 'lucide-react'

interface Props {
  snapshot:  SystemSnapshot | null
  connected: boolean
  fps:       number
}

export default function StatusBar({ snapshot, connected, fps }: Props) {
  const threat = snapshot?.overall_threat ?? 'SAFE'

  return (
    <div className={`flex items-center justify-between px-4 py-2 border-b
      ${threatBg[threat]} border transition-colors duration-300`}>

      {/* Left: connection */}
      <div className="flex items-center gap-3">
        {connected
          ? <Wifi className="w-4 h-4 text-green-400" />
          : <WifiOff className="w-4 h-4 text-red-400 animate-pulse" />}
        <span className={`text-sm font-mono ${connected ? 'text-green-400' : 'text-red-400'}`}>
          {connected ? 'CONNECTED' : 'DISCONNECTED'}
        </span>
        <span className="text-gray-500 text-sm font-mono">
          {fps} fps
        </span>
      </div>

      {/* Center: scene + frame */}
      {snapshot && (
        <div className="flex items-center gap-4 text-sm font-mono text-gray-300">
          <span>Scene: <span className="text-white font-bold">{snapshot.scene}</span></span>
          <span>Frame: <span className="text-white">{snapshot.frame_idx}</span>/{snapshot.total_frames}</span>
          <span>Tracks: <span className="text-white">{snapshot.fused_objects.length}</span></span>
        </div>
      )}

      {/* Right: overall threat */}
      <div className="flex items-center gap-2">
        <Activity className={`w-4 h-4 ${threatText[threat]}`} />
        <span className={`text-sm font-bold font-mono ${threatText[threat]}`}>
          {threat}
        </span>
      </div>
    </div>
  )
}
