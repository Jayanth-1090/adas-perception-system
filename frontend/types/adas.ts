export type SensorType = 'CAMERA' | 'RADAR' | 'ULTRASONIC'
export type ThreatLevel = 'SAFE' | 'WARNING' | 'CRITICAL'

export interface Detection {
  id: number
  label: string
  confidence: number
  x: number
  y: number
  vx: number
  vy: number
  bbox_x1: number
  bbox_y1: number
  bbox_x2: number
  bbox_y2: number
  src_frame_w: number
  src_frame_h: number
  sensor: SensorType
}

export interface FusedObject {
  track_id: number
  label: string
  confidence: number
  x: number
  y: number
  vx: number
  vy: number
  width: number
  length: number
  contributing_sensors: SensorType[]
}

export interface ThreatAssessment {
  track_id: number
  level: ThreatLevel
  ttc: number
  reason: string
}

export interface SystemSnapshot {
  timestamp_ms: number
  overall_threat: ThreatLevel
  raw_detections: Detection[]
  fused_objects: FusedObject[]
  threats: ThreatAssessment[]
  frame_idx: number
  total_frames: number
  scene: string
}
