import { NextRequest, NextResponse } from 'next/server'

export async function POST(req: NextRequest) {
  try {
    const { snapshot } = await req.json()

    const tracks = (snapshot.fused_objects || []).map((obj: any) => {
      const ta = (snapshot.threats || []).find((t: any) => t.track_id === obj.track_id)
      return `Track #${obj.track_id} (${obj.label}): dist=${obj.y.toFixed(1)}m, vy=${obj.vy.toFixed(2)}m/s, TTC=${ta?.ttc > 0 ? ta.ttc.toFixed(1)+'s' : 'N/A'}, threat=${ta?.level ?? 'SAFE'}`
    }).join('\n')

    const prompt = `You are an ADAS safety analyst. Scene: ${snapshot.scene}, Overall Threat: ${snapshot.overall_threat}.
Active tracks: ${tracks || 'none'}.
In 3 sentences: identify the primary threat, explain why it is dangerous, and state what ADAS intervention (AEB/FCW/LCW) is appropriate. Be technical and use metric units.`

    // Try Claude first, fall back to Groq
    const anthropicKey = process.env.ANTHROPIC_API_KEY
    const groqKey      = process.env.GROQ_API_KEY

    if (anthropicKey && !anthropicKey.includes('your_key')) {
      const res  = await fetch('https://api.anthropic.com/v1/messages', {
        method: 'POST',
        headers: {
          'Content-Type':      'application/json',
          'x-api-key':         anthropicKey,
          'anthropic-version': '2023-06-01',
        },
        body: JSON.stringify({
          model:      'claude-haiku-4-5-20251001',
          max_tokens: 300,
          messages:   [{ role: 'user', content: prompt }],
        }),
      })
      const data = await res.json()
      if (!data.error) {
        return NextResponse.json({ analysis: data.content?.[0]?.text ?? 'No response', provider: 'claude' })
      }
    }

    // Groq fallback — free tier
    if (groqKey && !groqKey.includes('your-groq')) {
      const res  = await fetch('https://api.groq.com/openai/v1/chat/completions', {
        method: 'POST',
        headers: {
          'Content-Type':  'application/json',
          'Authorization': `Bearer ${groqKey}`,
        },
        body: JSON.stringify({
          model:      'llama-3.1-8b-instant',
          max_tokens: 300,
          messages:   [{ role: 'user', content: prompt }],
        }),
      })
      const data = await res.json()
      const text = data.choices?.[0]?.message?.content
      if (text) {
        return NextResponse.json({ analysis: text, provider: 'groq' })
      }
    }

    return NextResponse.json({
      analysis: 'Configure ANTHROPIC_API_KEY or GROQ_API_KEY in frontend/.env.local'
    })

  } catch (e: any) {
    return NextResponse.json({ analysis: `Error: ${e.message}` }, { status: 500 })
  }
}
