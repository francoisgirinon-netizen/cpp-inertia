import { router } from '@inertiajs/react'
import Layout from './Layout'

type Post = { id: number; title: string }

type Props = {
  posts: Post[]
  latest: Post[]
  page: number
  settings: { filters: { tags: string[] } }
  thread: { data: Post[]; meta: Post[] }
}

export default function Posts({ posts, latest, page, settings, thread }: Props) {
  const step = settings.filters.tags.length + 1
  // `settings` is a deepMerge prop: the new tag joins the existing ones.
  const addTag = () => router.reload({ only: ['settings'], data: { step } })
  // `thread` merges at nested paths: `data` is appended, `meta` is prepended.
  const growThread = () => router.reload({ only: ['thread'], data: { step: thread.data.length / 2 + 1 } })

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
      <p data-testid="tags">Tags: {settings.filters.tags.join(', ')}</p>
      <ul data-testid="thread-data">
        {thread.data.map((post) => (
          <li key={post.id}>{post.title}</li>
        ))}
      </ul>
      <ul data-testid="thread-meta">
        {thread.meta.map((post) => (
          <li key={post.id}>{post.title}</li>
        ))}
      </ul>
      <button onClick={addTag}>Add tag</button>
      <button onClick={growThread}>Grow thread</button>
      <button onClick={loadMore}>Load more</button>
      <button onClick={addLatest}>Prepend latest</button>
      <button onClick={reset}>Reset</button>
    </Layout>
  )
}
