import { expect, test } from '@playwright/test'
import { inertiaGet, visitDuring } from './helpers'

function exceptOnce(res) {
  return (res.request().headers()['x-inertia-except-once-props'] ?? '').split(',')
}

test('a once prop is listed in onceProps with its key and expiry [protocol:once.metadata]', async ({
  page,
}) => {
  await page.goto('/dashboard')
  const data = await page.evaluate(() => JSON.parse(document.querySelector('script[data-page]').textContent))
  expect(data.onceProps).toEqual({ plans: { prop: 'plans', expiresAt: null } })
  await expect(page.getByTestId('plans')).toContainText('Plans: Free, Pro')
})

test('a revisit sends X-Inertia-Except-Once-Props and the server skips the remembered prop [protocol:once.except-header]', async ({
  page,
}) => {
  await page.goto('/dashboard')
  const before = await page.getByTestId('plans').textContent()

  const res = await visitDuring(
    page,
    '/dashboard',
    () => page.getByRole('button', { name: 'Revisit' }).click(),
    'GET',
    null,
  )
  expect(exceptOnce(res)).toContain('plans')
  const body = await res.json()
  expect(body.props).not.toHaveProperty('plans')
  expect(body.onceProps.plans).toEqual({ prop: 'plans', expiresAt: null }) // still advertised
  // The client keeps showing the prop it already holds, resolved once.
  await expect(page.getByTestId('plans')).toHaveText(before ?? '')
})

test('a partial reload ignores the except-once header [protocol:once.partial-ignores-header]', async ({
  request,
}) => {
  // Server-contract check: the UI cannot send a partial reload with X-Inertia-Except-Once-Props.
  const res = await inertiaGet(request, '/dashboard', {
    'X-Inertia-Except-Once-Props': 'plans',
    'X-Inertia-Partial-Component': 'Dashboard',
    'X-Inertia-Partial-Data': 'plans',
  })
  const body = await res.json()
  expect(body.props.plans.names).toEqual(['Free', 'Pro'])

  const full = await inertiaGet(request, '/dashboard', { 'X-Inertia-Except-Once-Props': 'plans' })
  expect(await full.json()).not.toHaveProperty('props.plans')
})

test('a fresh once prop is resolved again although the client holds it [protocol:once.fresh]', async ({
  page,
}) => {
  await page.goto('/variants')
  const dump = page.getByTestId('props')
  await expect(dump).toContainText('"fresh":"fresh #')
  const before = JSON.parse((await dump.textContent()) ?? '{}')

  const res = await visitDuring(
    page,
    '/variants',
    () => page.getByRole('button', { name: 'Revisit' }).click(),
    'GET',
    null,
  )
  expect(exceptOnce(res)).toContain('fresh')
  const body = await res.json()
  expect(body.props.fresh).toMatch(/^fresh #\d+$/)
  expect(body.props.fresh).not.toBe(before.fresh)
  expect(body.props).not.toHaveProperty('plain') // the ordinary once prop is skipped
  await expect(dump).toContainText(`"fresh":"${body.props.fresh}"`)
  await expect(dump).toContainText(`"plain":"${before.plain}"`)
})

test('the once custom key, not the prop path, is what the client sends back [protocol:once.custom-key]', async ({
  page,
}) => {
  await page.goto('/variants')
  await expect(page.getByTestId('props')).toContainText('"custom":"custom #')
  const initial = await page.evaluate(() => JSON.parse(document.querySelector('script[data-page]').textContent))
  expect(initial.onceProps['shared-plan']).toEqual({ prop: 'custom', expiresAt: null })

  const res = await visitDuring(
    page,
    '/variants',
    () => page.getByRole('button', { name: 'Revisit' }).click(),
    'GET',
    null,
  )
  expect(exceptOnce(res)).toContain('shared-plan')
  expect(exceptOnce(res)).not.toContain('custom')
  expect(await res.json()).not.toHaveProperty('props.custom')
})

test('a deferred once prop the client already holds is not deferred again [protocol:once.defer]', async ({
  page,
  request,
}) => {
  const [first] = await Promise.all([
    page.waitForResponse((r) => r.request().headers()['x-inertia-partial-data'] === 'lazyOnce'),
    page.goto('/variants'),
  ])
  const loaded = (await first.json()).props.lazyOnce
  expect(loaded).toMatch(/^lazy once #\d+$/)
  await expect(page.getByTestId('props')).toContainText(`"lazyOnce":"${loaded}"`)

  const res = await visitDuring(
    page,
    '/variants',
    () => page.getByRole('button', { name: 'Revisit' }).click(),
    'GET',
    null,
  )
  expect(exceptOnce(res)).toContain('lazyOnce')
  const body = await res.json()
  expect(body.deferredProps).not.toHaveProperty('once') // nothing left to defer in that group
  expect(body.deferredProps.lazy).toEqual(['lazyItems'])
  expect(body.onceProps.lazyOnce).toEqual({ prop: 'lazyOnce', expiresAt: null })
  await expect(page.getByTestId('props')).toContainText(`"lazyOnce":"${loaded}"`)

  // The same rule on the wire, without the client.
  const wire = await inertiaGet(request, '/variants', { 'X-Inertia-Except-Once-Props': 'lazyOnce' })
  expect((await wire.json()).deferredProps).not.toHaveProperty('once')
})
