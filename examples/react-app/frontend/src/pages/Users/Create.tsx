import { Link, useForm } from '@inertiajs/react'
import type { FormEvent } from 'react'
import Layout from '../Layout'

export default function Create() {
  const { data, setData, post, processing, errors } = useForm({ name: '', email: '' })

  const submit = (event: FormEvent) => {
    event.preventDefault()
    post('/users') // on success the server redirects to /users with a flash message
  }

  return (
    <Layout>
      <h1>New user</h1>
      <form onSubmit={submit}>
        <label>
          Name
          <input value={data.name} onChange={(event) => setData('name', event.target.value)} />
        </label>
        {errors.name && <p role="alert">{errors.name}</p>}
        <label>
          Email
          <input value={data.email} onChange={(event) => setData('email', event.target.value)} />
        </label>
        {errors.email && <p role="alert">{errors.email}</p>}
        <button type="submit" disabled={processing}>
          Create
        </button>
      </form>
      <Link href="/users">Back to users</Link>
    </Layout>
  )
}
