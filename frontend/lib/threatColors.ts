import { ThreatLevel } from '@/types/adas'

export const threatBg: Record<ThreatLevel, string> = {
  SAFE:     'bg-green-900/40 border-green-500',
  WARNING:  'bg-yellow-900/40 border-yellow-500',
  CRITICAL: 'bg-red-900/40 border-red-500',
}

export const threatText: Record<ThreatLevel, string> = {
  SAFE:     'text-green-400',
  WARNING:  'text-yellow-400',
  CRITICAL: 'text-red-400',
}

export const threatHex: Record<ThreatLevel, string> = {
  SAFE:     '#22c55e',
  WARNING:  '#eab308',
  CRITICAL: '#ef4444',
}

export const threatBadge: Record<ThreatLevel, string> = {
  SAFE:     'bg-green-500/20 text-green-400 border border-green-500/50',
  WARNING:  'bg-yellow-500/20 text-yellow-400 border border-yellow-500/50',
  CRITICAL: 'bg-red-500/20 text-red-400 border border-red-500/50',
}
