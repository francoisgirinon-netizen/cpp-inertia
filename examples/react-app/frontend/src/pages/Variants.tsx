import { Deferred, router, usePage } from '@inertiajs/react'
import Layout from './Layout'

// Shows the page props as JSON so a visit or reload reveals exactly what the server sent. The
// client turns {"$bigint"} values into real BigInts, which JSON.stringify needs help with.
export default function Variants() {
  const { props } = usePage()
  return (
    <Layout>
      <h1>Variants</h1>
      <pre data-testid="props">{JSON.stringify(props, (_key, value) => (typeof value === 'bigint' ? `${value}n` : value))}</pre>
      <Deferred data="lazyItems" fallback={<p>Loading items...</p>}>
        <p data-testid="lazy-loaded">Lazy items loaded</p>
      </Deferred>
      <button onClick={() => router.reload({ only: ['auth.name'] })}>Reload auth.name</button>
      <button onClick={() => router.reload({ only: ['bag.x'] })}>Reload bag.x</button>
      <button onClick={() => router.visit('/variants')}>Revisit</button>
    </Layout>
  )
}
