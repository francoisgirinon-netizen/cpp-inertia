import { router } from '@inertiajs/react'
import Layout from './Layout'

export default function History({ secret }: { secret: string }) {
  return (
    <Layout>
      <h1>History</h1>
      <p>
        This page's props are encrypted in the browser history: <code>{secret}</code>
      </p>
      {/* The redirect carries clearHistory, so the client drops the encrypted entries. */}
      <button onClick={() => router.post('/history/clear')}>Clear history</button>
    </Layout>
  )
}
