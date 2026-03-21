import { NextRequest, NextResponse } from 'next/server'
import { SystemSnapshot } from '@/types/adas'

export async function POST(req: NextRequest) {
  const { snapshot } = await req.json() as { snapshot: SystemSnapshot }

  const tracks = snapshot.fused_objects.map(obj => {
    const ta = snapshot.threats.find(t => t.track_id === obj.track_id)
    return `Track #${obj.track_id} (${obj.label}): ` +
           `dist=${obj.y.toFixed(1)}m, lat=${obj.x.toFixed(2)}m, ` +
           `vy=${obj.vy.toFixed(2)}m/s, ` +
           `TTC=${ta?.ttc && ta.ttc > 0 ? ta.ttc.toFixed(1)+'s' : 'N/A'}, ` +
           `threat=${ta?.level ?? 'SAFE'}`
  }).join('\n')

  const prompt = `You are an ADAS safety analyst. Analyze this real-time perception snapshot:

Scene: ${snapshot.scene} | Frame: ${snapshot.frame_idx}/${snapshot.total_frames}
Overall Threat: ${snapshot.overall_threat}

Active Tracks:
${tracks || 'No active tracks'}

In 3-4 sentences: identify the primary threat, explain why it is dangerous, 
and state what ADAS intervention (AEB, LDW, FCW) would be appropriate.
Be specific and technical. Use metric units.`

  const apiKey = process.env.ANTHROPIC_API_KEY
  if (!apiKey || apiKey === 'your_claude_api_key_here') {
    return NextResponse.json({
      analysis: 'Claude API key not configured. Add ANTHROPIC_API_KEY to .env.local'
    })
  }

  try {
    const res = await fetch('https://api.anthropic.com/v1/messages', {
      method:  'POST',
      headers: {
        'Content-Type':      'application/json',
        'x-api-key':         apiKey,
        'anthropic-version': '2023-06-01',
      },
      body: JSON.stringify({
        model:      'claude-haiku-4-5-20251001',
        max_tokens: 300,
        messages:   [{ role: 'user', content: prompt }],
      }),
    })

    const data = await res.json()
    const analysis = data.content?.[0]?.text ?? 'No response from Claude'
    return NextResponse.json({ analysis })

  } catch (e) {
    return NextResponse.json({ analysis: 'Diagnostic request failed' }, { status: 500 })
  }
}
