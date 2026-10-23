import { expect, test } from '@playwright/test'
import { followedRedirectFrom, isVisit } from './helpers'

test('a PUT answered with 302 reaches the client as 303 and the browser follows with GET [protocol:redirect.303]', async ({
  page,
}) => {
  await page.goto('/users')
  const put = page.waitForResponse(isVisit('/users/1/promote', 'PUT'))
  const follow = page.waitForResponse(followedRedirectFrom('/users/1/promote', '/users'))
  await page.getByRole('button', { name: 'Promote' }).first().click()

  expect((await put).status()).toBe(303)
  const next = await follow
  expect(next.status()).toBe(200)
  expect(next.request().method()).toBe('GET')
  await expect(page.getByRole('status')).toHaveText('User promoted.')
  await expect(page.getByText('Ada Lovelace (ada@example.com) - admin')).toBeVisible()
})

test('a redirect keeps the #fragment when the server asks to preserve it [protocol:redirect.fragment]', async ({
  page,
}) => {
  await page.goto('/fragment')
  // The browser would drop the fragment of a 302, so the server answers 409 + X-Inertia-Redirect
  // and the client visits that URL itself, fragment included.
  const post = page.waitForResponse(isVisit('/fragment/jump', 'POST'))
  const visit = page.waitForResponse(isVisit('/fragment'))
  await page.getByRole('button', { name: 'Jump to section' }).click()

  const res = await post
  expect(res.status()).toBe(409)
  expect(res.headers()['x-inertia-redirect']).toBe('/fragment#section')
  expect((await (await visit).json()).preserveFragment).toBe(true)
  await expect(page).toHaveURL(/\/fragment#section$/)
  await expect(page.getByRole('heading', { name: 'Section' })).toBeInViewport()
})

test('an external location answers 409 + X-Inertia-Location and the client leaves the app [protocol:redirect.external]', async ({
  page,
}) => {
  await page.goto('/external')
  const [res] = await Promise.all([
    page.waitForResponse(isVisit('/external/go')),
    page.getByRole('link', { name: 'Leave the app' }).click(),
  ])
  expect(res.status()).toBe(409)
  expect(res.headers()['x-inertia-location']).toBe('/external/landing')

  await expect(page).toHaveURL(/\/external\/landing$/)
  await expect(page.getByRole('heading', { name: 'Landed outside Inertia' })).toBeVisible()
})

test('a 200 with an empty body becomes a redirect back to the page [protocol:response.empty]', async ({
  page,
}) => {
  await page.goto('/props')
  const empty = page.waitForResponse(isVisit('/props/empty', 'POST'))
  const back = page.waitForResponse(followedRedirectFrom('/props/empty', '/props'))
  await page.getByRole('button', { name: 'Empty response' }).click()

  const res = await empty
  expect([302, 303]).toContain(res.status())
  expect(res.headers()['location']).toMatch(/\/props$/)
  expect((await back).status()).toBe(200)
  await expect(page).toHaveURL(/\/props$/)
  await expect(page.getByRole('heading', { name: 'Props' })).toBeVisible()
})
