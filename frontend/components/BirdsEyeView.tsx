'use client'
import React, { useRef, useEffect } from 'react'
import { SystemSnapshot, FusedObject, ThreatAssessment } from '@/types/adas'
import { threatHex } from '@/lib/threatColors'

interface Props {
  snapshot: SystemSnapshot | null
  width?:   number
  height?:  number
}

const RANGE_M = 60

function worldToPixel(x: number, y: number, w: number, h: number) {
  const px = w / 2 + (x / RANGE_M) * (w / 2)
  const py = h - 50 - (y / RANGE_M) * (h - 80)
  return { px, py }
}

function BirdsEyeView({ snapshot, width = 380, height = 460 }: Props) {
  const canvasRef = useRef<HTMLCanvasElement>(null)

  useEffect(() => {
    const canvas = canvasRef.current
    if (!canvas) return
    const ctx = canvas.getContext('2d')
    if (!ctx) return

    // Background
    ctx.fillStyle = '#0d1117'
    ctx.fillRect(0, 0, width, height)

    // Grid — range rings
    ctx.strokeStyle = '#1e293b'
    ctx.lineWidth = 1
    ctx.font = '10px monospace'
    ctx.fillStyle = '#334155'
    for (let d = 10; d <= RANGE_M; d += 10) {
      const { py } = worldToPixel(0, d, width, height)
      ctx.beginPath()
      ctx.moveTo(0, py)
      ctx.lineTo(width, py)
      ctx.stroke()
      ctx.fillText(`${d}m`, 4, py - 2)
    }

    // Ego lane boundaries at ±1.75m
    ctx.strokeStyle = '#374151'
    ctx.setLineDash([4, 4])
    const lL = worldToPixel(-1.75, 0, width, height)
    const lR = worldToPixel( 1.75, 0, width, height)
    ctx.beginPath(); ctx.moveTo(lL.px, 0); ctx.lineTo(lL.px, height); ctx.stroke()
    ctx.beginPath(); ctx.moveTo(lR.px, 0); ctx.lineTo(lR.px, height); ctx.stroke()
    ctx.setLineDash([])

    // Center line
    ctx.strokeStyle = '#1e3a5f'
    ctx.beginPath(); ctx.moveTo(width/2, 0); ctx.lineTo(width/2, height); ctx.stroke()

    // Ego vehicle
    const ex = width / 2
    const ey = height - 45
    ctx.fillStyle = '#60a5fa'
    ctx.fillRect(ex - 8, ey - 16, 16, 20)
    // Arrow
    ctx.strokeStyle = '#ffffff'
    ctx.lineWidth = 2
    ctx.beginPath(); ctx.moveTo(ex, ey - 16); ctx.lineTo(ex, ey - 30); ctx.stroke()
    ctx.beginPath()
    ctx.moveTo(ex - 4, ey - 24); ctx.lineTo(ex, ey - 30); ctx.lineTo(ex + 4, ey - 24)
    ctx.stroke()

    // Label
    ctx.fillStyle = '#94a3b8'
    ctx.font = '10px monospace'
    ctx.fillText('EGO', ex - 10, ey + 10)

    if (!snapshot) return

    // Build threat map
    const threatMap = new Map<number, ThreatAssessment>(
      (snapshot?.threats ?? []).map(t => [t.track_id, t])
    )

    // Draw tracked objects
    for (const obj of (snapshot.fused_objects ?? [])) {
      const ta    = threatMap.get(obj.track_id)
      const level = ta?.level ?? 'SAFE'
      const color = threatHex[level]
      const { px, py } = worldToPixel(obj.x, obj.y, width, height)

      if (py < 0 || py > height || px < 0 || px > width) continue

      // Circle
      ctx.beginPath()
      ctx.arc(px, py, 8, 0, Math.PI * 2)
      ctx.fillStyle = color + '33'
      ctx.fill()
      ctx.strokeStyle = color
      ctx.lineWidth = 2
      ctx.stroke()

      // Velocity arrow
      const scale = 6
      const dx =  obj.vx * scale
      const dy = -obj.vy * scale
      if (Math.abs(dx) > 1 || Math.abs(dy) > 1) {
        ctx.strokeStyle = '#fbbf24'
        ctx.lineWidth = 1.5
        ctx.beginPath()
        ctx.moveTo(px, py)
        ctx.lineTo(px + dx, py + dy)
        ctx.stroke()
        // Arrowhead
        const angle = Math.atan2(dy, dx)
        const al = 6
        ctx.beginPath()
        ctx.moveTo(px + dx, py + dy)
        ctx.lineTo(px + dx - al * Math.cos(angle - 0.4),
                   py + dy - al * Math.sin(angle - 0.4))
        ctx.lineTo(px + dx - al * Math.cos(angle + 0.4),
                   py + dy - al * Math.sin(angle + 0.4))
        ctx.closePath()
        ctx.fillStyle = '#fbbf24'
        ctx.fill()
      }

      // Label
      ctx.fillStyle = color
      ctx.font = '9px monospace'
      ctx.fillText(`#${obj.track_id} ${obj.label.slice(0,3)}`, px + 10, py + 3)
    }

    // Title
    ctx.fillStyle = '#475569'
    ctx.font = '11px monospace'
    ctx.fillText("BIRD'S EYE VIEW", 8, 18)

  }, [snapshot, width, height])

  return (
    <canvas
      ref={canvasRef}
      width={width}
      height={height}
      className="rounded"
    />
  )
}

export default React.memo(BirdsEyeView)
