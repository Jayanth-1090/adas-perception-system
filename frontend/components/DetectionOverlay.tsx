'use client'
import { useRef, useEffect, useMemo } from 'react'
import { SystemSnapshot, FusedObject, ThreatAssessment } from '@/types/adas'
import { threatHex } from '@/lib/threatColors'

interface Props {
  snapshot:    SystemSnapshot | null
  videoWidth:  number
  videoHeight: number
  srcWidth?:   number
  srcHeight?:  number
}

export default function DetectionOverlay({
  snapshot, videoWidth, videoHeight,
  srcWidth = 1920, srcHeight = 1080
}: Props) {
  const canvasRef = useRef<HTMLCanvasElement>(null)

  const threatMap = useMemo(() =>
    new Map<number, ThreatAssessment>(
      (snapshot?.threats ?? []).map(t => [t.track_id, t])
    ),
  [snapshot?.threats])

  useEffect(() => {
    const canvas = canvasRef.current
    if (!canvas) return
    const ctx = canvas.getContext('2d')
    if (!ctx) return

    ctx.clearRect(0, 0, videoWidth, videoHeight)
    if (!snapshot) return

    const sx = videoWidth  / srcWidth
    const sy = videoHeight / srcHeight

    // Build detection lookup by label + proximity
    const detMap = new Map<string, typeof snapshot.raw_detections[0]>()
    for (const d of snapshot.raw_detections) {
      detMap.set(`${d.label}_${Math.round(d.y)}`, d)
    }

    for (const obj of snapshot.fused_objects) {
      const ta    = threatMap.get(obj.track_id)
      const level = ta?.level ?? 'SAFE'
      const color = threatHex[level]

      // Find matching raw detection for real bbox
      let x1 = 0, y1 = 0, x2 = 0, y2 = 0
      let found = false

      // Match strategy: single label = direct match, multiple = closest
      const sameLabel = snapshot.raw_detections.filter(d => d.label === obj.label)
      if (sameLabel.length === 1 && sameLabel[0].bbox_x2 > sameLabel[0].bbox_x1) {
        x1 = sameLabel[0].bbox_x1; y1 = sameLabel[0].bbox_y1
        x2 = sameLabel[0].bbox_x2; y2 = sameLabel[0].bbox_y2
        found = true
      } else {
        let bestDist = 999
        for (const d of snapshot.raw_detections) {
          if (d.label !== obj.label) continue
          const dist = Math.sqrt((d.x - obj.x)**2 + (d.y - obj.y)**2)
          if (dist < bestDist && d.bbox_x2 > d.bbox_x1) {
            bestDist = dist
            x1 = d.bbox_x1; y1 = d.bbox_y1
            x2 = d.bbox_x2; y2 = d.bbox_y2
            found = true
          }
        }
      }

      if (!found) continue

      // Scale to display
      const dx1 = x1 * sx, dy1 = y1 * sy
      const dx2 = x2 * sx, dy2 = y2 * sy
      const bw = dx2 - dx1, bh = dy2 - dy1
      const cx = (dx1 + dx2) / 2

      // Bbox
      ctx.strokeStyle = color
      ctx.lineWidth = 2
      ctx.strokeRect(dx1, dy1, bw, bh)

      // Corner accents
      const cl = Math.min(20, bw * 0.2)
      ctx.lineWidth = 3
      ;[[dx1,dy1],[dx2,dy1],[dx1,dy2],[dx2,dy2]].forEach(([x,y], i) => {
        const sx2 = i % 2 === 0 ? 1 : -1
        const sy2 = i < 2 ? 1 : -1
        ctx.beginPath()
        ctx.moveTo(x, y); ctx.lineTo(x + sx2 * cl, y)
        ctx.moveTo(x, y); ctx.lineTo(x, y + sy2 * cl)
        ctx.stroke()
      })

      // Label pill
      const label = `#${obj.track_id} ${obj.label} ${Math.round(obj.confidence*100)}%`
      ctx.font = 'bold 11px monospace'
      const tw = ctx.measureText(label).width
      const lx = Math.max(0, dx1)
      const ly = Math.max(16, dy1)

      ctx.fillStyle = color + 'dd'
      ctx.beginPath()
      ctx.roundRect(lx, ly - 16, tw + 8, 18, 3)
      ctx.fill()

      ctx.fillStyle = '#000000'
      ctx.fillText(label, lx + 4, ly - 3)

      // TTC
      if (ta && ta.ttc > 0) {
        ctx.font = 'bold 10px monospace'
        ctx.fillStyle = color
        ctx.fillText(`TTC: ${ta.ttc.toFixed(1)}s`, dx1, dy2 + 14)
      }

      // Velocity arrow
      if (Math.abs(obj.vy) > 0.5) {
        const len = Math.min(Math.abs(obj.vy) * 5, 50)
        const dir = obj.vy < 0 ? -1 : 1
        const ay = (dy1 + dy2) / 2
        ctx.strokeStyle = '#fbbf24'
        ctx.lineWidth = 2
        ctx.beginPath()
        ctx.moveTo(cx, ay)
        ctx.lineTo(cx, ay - dir * len)
        ctx.stroke()
      }
    }
  }, [snapshot, videoWidth, videoHeight, srcWidth, srcHeight])

  return (
    <canvas
      ref={canvasRef}
      width={videoWidth}
      height={videoHeight}
      className="absolute inset-0 pointer-events-none"
    />
  )
}
