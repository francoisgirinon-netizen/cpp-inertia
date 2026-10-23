import { useForm } from '@inertiajs/react'
import type { FormEvent } from 'react'
import Layout from './Layout'

export default function Login() {
  const { data, setData, post, processing, errors } = useForm({ password: '' })

  const submit = (event: FormEvent) => {
    event.preventDefault()
    // errorBag: the server's errors are read from the 'login' bag only. The password is "secret".
    post('/login', { errorBag: 'login' })
  }

  return (
    <Layout>
      <h1>Login</h1>
      <form onSubmit={submit}>
        <label>
          Password
          <input
            type="password"
            value={data.password}
            onChange={(event) => setData('password', event.target.value)}
          />
        </label>
        {errors.password && <p role="alert">{errors.password}</p>}
        <button type="submit" disabled={processing}>
          Log in
        </button>
      </form>
    </Layout>
  )
}
