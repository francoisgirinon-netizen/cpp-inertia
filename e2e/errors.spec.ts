import { expect, test } from '@playwright/test'
import { followedRedirectFrom, inertiaGet, isVisit } from './helpers'

test('errors is an empty object, never an array, when nothing failed [protocol:errors.empty]', async ({
  page,
  request,
}) => {
  const res = await inertiaGet(request, '/users/create')
  const text = await res.text()
  expect(JSON.parse(text).props.errors).toEqual({})
  expect(text).toContain('"errors":{}')

  await page.goto('/users/create')
  await expect(page.getByRole('heading', { name: 'New user' })).toBeVisible()
  await expect(page.getByRole('alert')).toHaveCount(0)
})

test('validation errors reach the form as one message per field [protocol:errors.first-message]', async ({
  page,
}) => {
  await page.goto('/users/create')
  const post = page.waitForResponse(isVisit('/users', 'POST'))
  const back = page.waitForResponse(followedRedirectFrom('/users', '/users/create'))
  await page.getByRole('button', { name: 'Create' }).click()

  expect([302, 303]).toContain((await post).status())
  const errors = (await (await back).json()).props.errors
  expect(errors).toEqual({ name: 'The name is required.', email: 'The email must contain @.' })
  await expect(page.getByRole('alert')).toHaveText(['The name is required.', 'The email must contain @.'])
})

test('an error bag header scopes the errors to that bag [protocol:errors.bag]', async ({ page }) => {
  await page.goto('/login')
  await page.getByLabel('Password').fill('wrong')
  const post = page.waitForResponse(isVisit('/login', 'POST'))
  const back = page.waitForResponse(followedRedirectFrom('/login', '/login'))
  await page.getByRole('button', { name: 'Log in' }).click()

  expect((await post).request().headers()['x-inertia-error-bag']).toBe('login')
  const errors = (await (await back).json()).props.errors
  expect(errors).toEqual({ login: { password: 'The password is wrong.' } })
  await expect(page.getByRole('alert')).toHaveText('The password is wrong.')

  // The right password clears the error and logs in.
  await page.getByLabel('Password').fill('secret')
  await page.getByRole('button', { name: 'Log in' }).click()
  await expect(page.getByRole('status')).toHaveText('Logged in.')
})
