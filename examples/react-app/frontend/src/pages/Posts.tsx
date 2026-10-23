import { router } from '@inertiajs/react'
import Layout from './Layout'

type Post = { id: number; title: string }

export default function Posts({ posts, latest, page }: { posts: Post[]; latest: Post[]; page: number }) {
  // `posts` is merged (appended), so only the new page travels over the wire.
  const loadMore = () => router.reload({ only: ['posts', 'page'], data: { page: page + 1 } })
  // `latest` is prepended: new entries show up before the existing ones.
  const addLatest = () => router.reload({ only: ['latest'], data: { newest: latest.length } })
  // `reset` replaces the merged list instead of merging into it.
  const reset = () => router.reload({ only: ['posts', 'page'], reset: ['posts'], data: { page: 1 } })

  return (
    <Layout>
      <h1>Posts</h1>
      <ul data-testid="latest">
        {latest.map((post) => (
          <li key={post.id}>{post.title}</li>
        ))}
      </ul>
      <ul data-testid="posts">
        {posts.map((post) => (
          <li key={post.id}>{post.title}</li>
        ))}
      </ul>
      <button onClick={loadMore}>Load more</button>
      <button onClick={addLatest}>Prepend latest</button>
      <button onClick={reset}>Reset</button>
    </Layout>
  )
}
