'use client'
import { useRef, useState, useEffect } from 'react'
import { useADASStream } from '@/hooks/useADASStream'
import StatusBar      from '@/components/StatusBar'
import TrackTable     from '@/components/TrackTable'
import BirdsEyeView   from '@/components/BirdsEyeView'
import DetectionOverlay from '@/components/DetectionOverlay'
import DiagnosticPanel from '@/components/DiagnosticPanel'

const WS_URL = process.env.NEXT_PUBLIC_WS_URL ?? 'ws://localhost:9002/ws'

const SCENES = [
  { id: 0, name: 'DAY',   file: '/videos/nD_1.mp4' },
  { id: 1, name: 'RAIN',  file: '/videos/rD_2.mp4' },
  { id: 2, name: 'NIGHT', file: '/videos/nN_1.mp4' },
]

export default function Dashboard() {
  const { snapshot, connected, fps, error, sendMessage } = useADASStream(WS_URL)
  const [activeSceneIdx, setActiveSceneIdx] = useState(0)

  const switchScene = (idx: number) => {
    setActiveSceneIdx(idx)
    sendMessage(JSON.stringify({ cmd: 'switch_scene', scene: idx }))
  }
  const videoRef = useRef<HTMLVideoElement>(null)
  const [vidSize, setVidSize] = useState({ w: 960, h: 540 })
  const containerRef = useRef<HTMLDivElement>(null)

  // Sync video to pipeline timestamp
  // Strategy: video is paused and manually scrubbed to each detection timestamp
  // This gives frame-accurate sync since detections were extracted at specific frames
  const lastTs = useRef<number>(-1)
  useEffect(() => {
    const video = videoRef.current
    if (!video || !snapshot) return
    if (snapshot.timestamp_ms === lastTs.current) return
    lastTs.current = snapshot.timestamp_ms

    // Pause video — we control it manually via timestamp seeking
    if (!video.paused) video.pause()

    const target = snapshot.timestamp_ms / 1000
    // Only seek if needed — avoid redundant seeks
    if (Math.abs(video.currentTime - target) > 0.05) {
      video.currentTime = target
    }
  }, [snapshot?.timestamp_ms])

  // Track video display size for overlay
  useEffect(() => {
    const obs = new ResizeObserver(entries => {
      for (const e of entries) {
        setVidSize({ w: e.contentRect.width, h: e.contentRect.height })
      }
    })
    if (containerRef.current) obs.observe(containerRef.current)
    return () => obs.disconnect()
  }, [])

  const currentScene = SCENES[activeSceneIdx] ?? SCENES[0]



  return (
    <div className="flex flex-col h-screen bg-gray-950 text-white overflow-hidden">

      {/* Header */}
      <div className="flex items-center justify-between px-4 py-2 bg-gray-900
                      border-b border-gray-800">
        <h1 className="text-sm font-mono font-bold text-blue-400 tracking-widest">
          ADAS PERCEPTION SIMULATOR
        </h1>
        <div className="flex items-center gap-2">
          {error && (
            <span className="text-xs text-red-400 font-mono">{error}</span>
          )}
        </div>
      </div>

      {/* Status bar */}
      <StatusBar snapshot={snapshot} connected={connected} fps={fps} onSceneSwitch={switchScene} activeSceneIdx={activeSceneIdx} />

      {/* Main content */}
      <div className="flex flex-1 overflow-hidden">

        {/* Left: Video + overlay */}
        <div className="flex flex-col flex-1 min-w-0">

          {/* Video panel */}
          <div ref={containerRef}
               className="relative flex-1 bg-black overflow-hidden">
            <video
              key={currentScene.file}
              ref={videoRef}
              src={currentScene.file}
              className="w-full h-full object-contain"
              muted
              playsInline
              preload="auto"
              onLoadedData={() => {
                const v = videoRef.current
                if (v) v.pause()
              }}
            />
            <DetectionOverlay
              snapshot={snapshot}
              videoWidth={vidSize.w}
              videoHeight={vidSize.h}
              srcWidth={snapshot?.raw_detections?.[0]?.src_frame_w ?? 1920}
              srcHeight={snapshot?.raw_detections?.[0]?.src_frame_h ?? 1080}
            />

            {/* Overlay: no connection */}
            {!connected && (
              <div className="absolute inset-0 flex items-center justify-center
                              bg-black/70 backdrop-blur-sm">
                <div className="text-center">
                  <div className="w-8 h-8 border-2 border-blue-500
                                  border-t-transparent rounded-full
                                  animate-spin mx-auto mb-3" />
                  <p className="text-blue-400 font-mono text-sm">
                    Connecting to simulator...
                  </p>
                  <p className="text-gray-500 font-mono text-xs mt-1">
                    Run: ./build/adas_simulator 0
                  </p>
                </div>
              </div>
            )}
          </div>

          {/* Track table */}
          <div className="h-36 border-t border-gray-800 bg-gray-900/80">
            <TrackTable snapshot={snapshot} />
          </div>
        </div>

        {/* Right sidebar */}
        <div className="w-96 flex flex-col border-l border-gray-800 bg-gray-900/30">

          {/* Bird's eye view */}
          <div className="flex-1 flex items-center justify-center p-3">
            <BirdsEyeView
              snapshot={snapshot}
              width={360}
              height={340}
            />
          </div>

          {/* AI Diagnostics */}
          <div className="h-64 border-t border-gray-800 p-3">
            <DiagnosticPanel snapshot={snapshot} />
          </div>
        </div>
      </div>
    </div>
  )
}
