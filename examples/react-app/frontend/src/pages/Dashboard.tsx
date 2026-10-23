import { Deferred, router } from '@inertiajs/react'
import Layout from './Layout'

type Props = {
  title: string
  metrics?: { visitors: number; orders: number }
  activity?: string[]
  events?: { id: number; label: string }[]
  plans: { names: string[]; resolutions: number }
}

export default function Dashboard({ title, metrics, activity, events, plans }: Props) {
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

      {/* A deferred prop whose resolver throws: `rescue` renders instead of the content. */}
      <Deferred data="risky" fallback={<p>Loading report...</p>} rescue={<p role="alert">Report unavailable</p>}>
        <p>Report ready</p>
      </Deferred>

      {/* A deferred merge prop: each reload appends the new event to the list. */}
      <Deferred data="events" fallback={<p>Loading events...</p>}>
        <ul data-testid="events">
          {events?.map((event) => <li key={event.id}>{event.label}</li>)}
        </ul>
      </Deferred>
      <button onClick={() => router.reload({ only: ['events'] })}>More events</button>

      {/* A once prop: revisiting this page reuses it (X-Inertia-Except-Once-Props) instead of resolving it again. */}
      <p data-testid="plans">
        Plans: {plans.names.join(', ')} (resolved {plans.resolutions}x)
      </p>
      <button onClick={() => router.visit('/dashboard')}>Revisit</button>
    </Layout>
  )
}
