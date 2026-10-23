import { router } from '@inertiajs/react'
import Layout from './Layout'

export default function Fragment() {
  return (
    <Layout>
      <h1>Fragment</h1>
      <button onClick={() => router.post('/fragment/jump')}>Jump to section</button>
      <div style={{ height: 1500 }} />
      <h2 id="section">Section</h2>
      <p>You were redirected here with the fragment preserved.</p>
    </Layout>
  )
}
