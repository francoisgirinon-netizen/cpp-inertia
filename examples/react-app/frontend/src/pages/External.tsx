import { Link } from '@inertiajs/react'
import Layout from './Layout'

export default function External() {
  return (
    <Layout>
      <h1>External</h1>
      {/* The server answers 409 + X-Inertia-Location, so the browser leaves the SPA. */}
      <Link href="/external/go">Leave the app</Link>
    </Layout>
  )
}
