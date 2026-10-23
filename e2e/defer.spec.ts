import { expect, test } from '@playwright/test'
import { isVisit } from './helpers'

test('deferred props in different groups load by separate follow-up requests [protocol:defer.groups]', async ({
  page,
}) => {
  const metrics = page.waitForResponse(isVisit('/dashboard', 'GET', 'metrics'))
  const activity = page.waitForResponse(isVisit('/dashboard', 'GET', 'activity'))
  const first = page.waitForResponse(
    (r) => new URL(r.url()).pathname === '/dashboard' && r.request().headers()['x-inertia'] === undefined,
  )
  await page.goto('/dashboard')

  const initial = await (await first).text()
  const raw = /<script data-page="app" type="application\/json">(.*?)<\/script>/s.exec(initial)?.[1] ?? '{}'
  const deferred = JSON.parse(raw).deferredProps
  expect(deferred.metrics).toEqual(['metrics'])
  expect(deferred.activity).toEqual(['activity'])

  await metrics
  await activity
  await expect(page.getByTestId('metrics')).toHaveText('1280 visitors, 42 orders')
  await expect(page.getByTestId('activity')).toContainText('Ada signed in')
})

test('the follow-up request for a group is a partial reload of just that group [protocol:defer.partial]', async ({
  page,
}) => {
  const [res] = await Promise.all([
    page.waitForResponse(isVisit('/dashboard', 'GET', 'metrics')),
    page.goto('/dashboard'),
  ])
  const headers = res.request().headers()
  expect(headers['x-inertia-partial-component']).toBe('Dashboard')
  expect(headers['x-inertia-partial-data']).toBe('metrics')

  const body = await res.json()
  expect(body.props.metrics).toEqual({ visitors: 1280, orders: 42 })
  expect(body.props).not.toHaveProperty('activity')
  expect(body).not.toHaveProperty('deferredProps') // never emitted on a partial response
  await expect(page.getByTestId('metrics')).toBeVisible()
})

test('a deferred prop whose resolver throws is rescued: the response succeeds and the page shows the rescue [protocol:defer.rescue]', async ({
  page,
}) => {
  const [res] = await Promise.all([
    page.waitForResponse(isVisit('/dashboard', 'GET', 'risky')),
    page.goto('/dashboard'),
  ])
  expect(res.status()).toBe(200)
  const body = await res.json()
  expect(body.rescuedProps).toEqual(['risky'])
  expect(body.props).not.toHaveProperty('risky')

  await expect(page.getByRole('alert')).toHaveText('Report unavailable')
  await expect(page.getByText('Report ready')).toBeHidden()
  // The other deferred props are unaffected.
  await expect(page.getByTestId('metrics')).toBeVisible()
})

test('every reload of a deferred merge prop appends to the list [protocol:defer.merge]', async ({
  page,
}) => {
  const [first] = await Promise.all([
    page.waitForResponse(isVisit('/dashboard', 'GET', 'events')),
    page.goto('/dashboard'),
  ])
  expect((await first.json()).mergeProps).toContain('events')
  const events = page.getByTestId('events').getByRole('listitem')
  await expect(events).toHaveCount(1)
  const firstLabel = await events.first().textContent()

  const [second] = await Promise.all([
    page.waitForResponse(isVisit('/dashboard', 'GET', 'events')),
    page.getByRole('button', { name: 'More events' }).click(),
  ])
  expect((await second.json()).mergeProps).toContain('events')
  await expect(events).toHaveCount(2)
  await expect(events.first()).toHaveText(firstLabel ?? '')
  await expect(events.last()).not.toHaveText(firstLabel ?? '')
})
