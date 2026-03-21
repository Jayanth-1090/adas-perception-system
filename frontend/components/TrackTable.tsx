'use client'
import { SystemSnapshot, ThreatAssessment } from '@/types/adas'
import { threatBadge, threatText } from '@/lib/threatColors'

interface Props { snapshot: SystemSnapshot | null }

export default function TrackTable({ snapshot }: Props) {
  if (!snapshot || snapshot.fused_objects.length === 0) {
    return (
      <div className="flex items-center justify-center h-full text-gray-600 text-sm font-mono">
        No active tracks
      </div>
    )
  }

  const threatMap = new Map<number, ThreatAssessment>(
    snapshot.threats.map(t => [t.track_id, t])
  )

  return (
    <div className="overflow-auto h-full">
      <table className="w-full text-xs font-mono">
        <thead className="sticky top-0 bg-gray-900 border-b border-gray-700">
          <tr className="text-gray-400">
            {['ID','Label','Dist(m)','Lat(m)','Vx(m/s)','Vy(m/s)','TTC(s)','Threat'].map(h => (
              <th key={h} className="px-3 py-2 text-left font-medium">{h}</th>
            ))}
          </tr>
        </thead>
        <tbody>
          {snapshot.fused_objects.map((obj, i) => {
            const ta     = threatMap.get(obj.track_id)
            const level  = ta?.level ?? 'SAFE'
            const ttc    = ta?.ttc ?? -1
            const isEven = i % 2 === 0

            return (
              <tr key={obj.track_id}
                  className={`border-b border-gray-800 transition-colors
                    ${isEven ? 'bg-gray-900/50' : 'bg-gray-800/30'}`}>
                <td className="px-3 py-1.5 text-blue-400 font-bold">#{obj.track_id}</td>
                <td className="px-3 py-1.5 text-white">{obj.label}</td>
                <td className="px-3 py-1.5 text-gray-300">{obj.y.toFixed(1)}</td>
                <td className="px-3 py-1.5 text-gray-300">{obj.x >= 0 ? '+' : ''}{obj.x.toFixed(2)}</td>
                <td className="px-3 py-1.5 text-gray-300">{obj.vx >= 0 ? '+' : ''}{obj.vx.toFixed(2)}</td>
                <td className={`px-3 py-1.5 font-medium ${obj.vy < -1.5 ? threatText['WARNING'] : 'text-gray-300'}`}>
                  {obj.vy >= 0 ? '+' : ''}{obj.vy.toFixed(2)}
                </td>
                <td className="px-3 py-1.5 text-gray-300">
                  {ttc > 0 ? ttc.toFixed(1) : '--'}
                </td>
                <td className="px-3 py-1.5">
                  <span className={`px-2 py-0.5 rounded text-xs font-bold ${threatBadge[level]}`}>
                    {level}
                  </span>
                </td>
              </tr>
            )
          })}
        </tbody>
      </table>
    </div>
  )
}
