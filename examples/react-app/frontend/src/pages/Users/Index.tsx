import { Link, router } from '@inertiajs/react'
import Layout from '../Layout'

type User = { id: number; name: string; email: string; admin: boolean }

type Props = {
  users: User[]
  filters: { search: string }
  renderCount: number
}

export default function Index({ users, filters, renderCount }: Props) {
  // Reload only the `users` prop: the server skips `filters` and `renderCount`.
  const search = (term: string) =>
    router.get('/users', { search: term }, { only: ['users'], preserveState: true, replace: true })

  return (
    <Layout>
      <h1>Users</h1>
      <input
        aria-label="Search"
        defaultValue={filters.search}
        onChange={(event) => search(event.target.value)}
      />
      <p data-testid="render-count">Rendered {renderCount} times</p>
      <ul>
        {users.map((user) => (
          <li key={user.id}>
            {user.name} ({user.email}){user.admin && ' - admin'}{' '}
            {/* The server answers a PUT redirect with 303. */}
            <button onClick={() => router.put(`/users/${user.id}/promote`)}>Promote</button>
          </li>
        ))}
      </ul>
      <Link href="/users/create">New user</Link>
    </Layout>
  )
}
