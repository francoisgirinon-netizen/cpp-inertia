import { InfiniteScroll } from '@inertiajs/react'
import Layout from './Layout'

type Item = { id: number; title: string }

export default function Feed({ items }: { items: { data: Item[] } }) {
  return (
    <Layout>
      <h1>Feed</h1>
      {/* Loads the next page when the end of the list scrolls into view, merging `items.data`. */}
      <InfiniteScroll
        data="items"
        buffer={100}
        loading={<p>Loading...</p>}
        next={({ fetch, hasNext, loading }) =>
          hasNext && !loading && <button onClick={fetch}>Load more</button>
        }
      >
        <ul>
          {items.data.map((item) => (
            <li key={item.id} style={{ height: 80 }}>
              {item.title}
            </li>
          ))}
        </ul>
      </InfiniteScroll>
    </Layout>
  )
}
