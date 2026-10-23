import { router } from '@inertiajs/react'
import Layout from './Layout'

type Props = { title: string; always: string; optional?: string; other: string }

export default function PropsPage({ title, always, optional, other }: Props) {
  return (
    <Layout>
      <h1>{title}</h1>
      <p data-testid="always">{always}</p>
      <p data-testid="optional">{optional ?? 'optional: not loaded'}</p>
      <p data-testid="other">{other}</p>

      {/* `always` still arrives although `only` names another prop. */}
      <button onClick={() => router.reload({ only: ['title'] })}>Reload only title</button>
      {/* `optional` is only sent when requested by name. */}
      <button onClick={() => router.reload({ only: ['optional'] })}>Load optional</button>
      {/* `except` reloads everything but the listed props. */}
      <button onClick={() => router.reload({ except: ['other'] })}>Reload except other</button>
      {/* The server answers 200 with an empty body; Inertia turns it into a redirect back here. */}
      <button onClick={() => router.post('/props/empty')}>Empty response</button>
    </Layout>
  )
}
