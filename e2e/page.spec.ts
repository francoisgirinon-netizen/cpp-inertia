import { expect, test } from '@playwright/test'
import { followedRedirectFrom, inertiaGet, isVisit } from './helpers'

function historyState(page) {
  return page.evaluate(() => JSON.stringify(history.state))
}

test('a flash message arrives once in the flash field and is gone on the next visit [protocol:page.flash]', async ({
  page,
}) => {
  // Unique per run: the example keeps users in memory across tests.
  const name = `Flash ${Date.now()}`
  await page.goto('/users/create')
  await page.getByLabel('Name').fill(name)
  await page.getByLabel('Email').fill('flash@example.com')
  const back = page.waitForResponse(followedRedirectFrom('/users', '/users'))
  await page.getByRole('button', { name: 'Create' }).click()

  const body = await (await back).json()
  expect(body.flash).toEqual({ success: 'User created.' })
  await expect(page.getByRole('status')).toHaveText('User created.')
  await expect(page.getByText(`${name} (flash@example.com)`)).toBeVisible()

  // The session value was pulled: the next visit carries no flash and the message disappears.
  const next = page.waitForResponse(isVisit('/'))
  await page.getByRole('link', { name: 'Home' }).click()
  expect(await (await next).json()).not.toHaveProperty('flash')
  await expect(page.getByRole('heading', { name: 'Home' })).toBeVisible()
  await expect(page.getByRole('status')).toHaveCount(0)
})

test('an encryptHistory page keeps its props unreadable in the browser history [protocol:page.encrypt-history]', async ({
  page,
  request,
}) => {
  expect(await (await inertiaGet(request, '/history')).json()).toMatchObject({ encryptHistory: true })
  expect(await (await inertiaGet(request, '/users')).json()).not.toHaveProperty('encryptHistory')

  await page.goto('/history')
  await expect(page.getByText('s3cret-token-42')).toBeVisible()
  // The client stores an encrypted blob in history.state: no component name, no props.
  await expect
    .poll(async () => JSON.parse(await historyState(page)).page, { message: 'encrypted state' })
    .toEqual({})
  expect(await historyState(page)).not.toContain('s3cret-token-42')

  // Control: an ordinary page is stored in the clear.
  await page.goto('/users')
  await expect.poll(async () => await historyState(page)).toContain('Users/Index')
})

test('clearHistory is sent once after the server asks for it and the flash rides along [protocol:page.clear-history]', async ({
  page,
}) => {
  await page.goto('/history')
  const post = page.waitForResponse(isVisit('/history/clear', 'POST'))
  const back = page.waitForResponse(followedRedirectFrom('/history/clear', '/history'))
  await page.getByRole('button', { name: 'Clear history' }).click()

  expect([302, 303]).toContain((await post).status())
  const body = await (await back).json()
  expect(body.clearHistory).toBe(true)
  expect(body.encryptHistory).toBe(true)
  await expect(page.getByRole('status')).toHaveText('History cleared.')

  // The session flag is pulled once: a later render of the page no longer carries it.
  const again = await page.request.get('/history', {
    headers: { 'X-Inertia': 'true', 'X-Inertia-Version': body.version },
  })
  expect(await again.json()).not.toHaveProperty('clearHistory')
})

test('preserveFragment is flagged on the page after the redirect and only once [protocol:page.preserve-fragment]', async ({
  page,
}) => {
  await page.goto('/fragment')
  const visit = page.waitForResponse(isVisit('/fragment'))
  await page.getByRole('button', { name: 'Jump to section' }).click()

  const body = await (await visit).json()
  expect(body.preserveFragment).toBe(true)
  await expect(page).toHaveURL(/\/fragment#section$/)

  const again = await page.request.get('/fragment', {
    headers: { 'X-Inertia': 'true', 'X-Inertia-Version': body.version },
  })
  expect(await again.json()).not.toHaveProperty('preserveFragment')
})
