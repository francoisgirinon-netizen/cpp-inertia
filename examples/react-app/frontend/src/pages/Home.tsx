import Layout from './Layout'

export default function Home({ greeting }: { greeting: string }) {
  return (
    <Layout>
      <h1>Home</h1>
      <p>{greeting}</p>
    </Layout>
  )
}
