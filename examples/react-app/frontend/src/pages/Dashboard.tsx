import { Deferred, router } from '@inertiajs/react'
import Layout from './Layout'

type Props = {
  title: string
  metrics?: { visitors: number; orders: number }
  activity?: string[]
  plans: { names: string[]; resolutions: number }
}

export default function Dashboard({ title, metrics, activity, plans }: Props) {
  return (
    <Layout>
      <h1>{title}</h1>

      {/* `metrics` and `activity` are deferred in separate groups: two follow-up requests. */}
      <Deferred data="metrics" fallback={<p>Loading metrics...</p>}>
        <p data-testid="metrics">
          {metrics?.visitors} visitors, {metrics?.orders} orders
        </p>
      </Deferred>
      <Deferred data="activity" fallback={<p>Loading activity...</p>}>
        <ul data-testid="activity">
          {activity?.map((entry) => <li key={entry}>{entry}</li>)}
        </ul>
      </Deferred>

      {/* A once prop: revisiting this page reuses it (X-Inertia-Except-Once-Props) instead of resolving it again. */}
      <p data-testid="plans">
        Plans: {plans.names.join(', ')} (resolved {plans.resolutions}x)
      </p>
      <button onClick={() => router.visit('/dashboard')}>Revisit</button>
    </Layout>
  )
}
