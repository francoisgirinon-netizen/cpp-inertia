import { expect, test } from '@playwright/test'
import { inertiaGet, renderCount, visitDuring } from './helpers'

test('a search reload sends X-Inertia-Partial-Data and gets only the requested props [protocol:partial.only]', async ({
  page,
}) => {
  await page.goto('/users')
  const before = renderCount(await page.getByTestId('render-count').textContent())

  const res = await visitDuring(page, '/users', () => page.getByLabel('Search').fill('grace'))
  const headers = res.request().headers()
  expect(headers['x-inertia-partial-data']).toBe('users')
  expect(headers['x-inertia-partial-component']).toBe('Users/Index')

  const body = await res.json()
  expect(Object.keys(body.props)).toContain('users')
  expect(body.props).not.toHaveProperty('filters')
  expect(body.props).not.toHaveProperty('renderCount')
  await expect(page.getByText('Grace Hopper (grace@example.com)')).toBeVisible()
  await expect(page.getByText('Ada Lovelace')).toBeHidden()
  // renderCount was not resolved again, so the page still shows the value from the first load.
  expect(renderCount(await page.getByTestId('render-count').textContent())).toBe(before)
})

test('a partial request for a different component is treated as a full visit [protocol:partial.component-match]', async ({
  request,
}) => {
  const matching = await inertiaGet(request, '/users', {
    'X-Inertia-Partial-Component': 'Users/Index',
    'X-Inertia-Partial-Data': 'users',
  })
  expect(Object.keys((await matching.json()).props)).not.toContain('renderCount')

  const other = await inertiaGet(request, '/users', {
    'X-Inertia-Partial-Component': 'Dashboard',
    'X-Inertia-Partial-Data': 'users',
  })
  const props = (await other.json()).props
  expect(props).toHaveProperty('users')
  expect(props).toHaveProperty('filters')
  expect(props).toHaveProperty('renderCount')
})

test('except reloads every prop but the excluded one [protocol:partial.except]', async ({ page }) => {
  await page.goto('/props')
  const otherBefore = await page.getByTestId('other').textContent()
  const alwaysBefore = await page.getByTestId('always').textContent()

  const res = await visitDuring(page, '/props', () =>
    page.getByRole('button', { name: 'Reload except other' }).click(),
  )
  expect(res.request().headers()['x-inertia-partial-except']).toBe('other')
  const props = (await res.json()).props
  expect(props).not.toHaveProperty('other')
  expect(props).toHaveProperty('title')
  expect(props).toHaveProperty('always')

  await expect(page.getByTestId('always')).not.toHaveText(alwaysBefore ?? '')
  await expect(page.getByTestId('other')).toHaveText(otherBefore ?? '')
})

test('always props are resolved on a partial reload that does not list them [protocol:always.partial]', async ({
  page,
}) => {
  await page.goto('/props')
  const alwaysBefore = await page.getByTestId('always').textContent()
  const otherBefore = await page.getByTestId('other').textContent()

  const res = await visitDuring(page, '/props', () =>
    page.getByRole('button', { name: 'Reload only title' }).click(),
  )
  expect(res.request().headers()['x-inertia-partial-data']).toBe('title')
  const props = (await res.json()).props
  expect(props).toHaveProperty('always')
  expect(props).not.toHaveProperty('other')

  await expect(page.getByTestId('always')).not.toHaveText(alwaysBefore ?? '')
  await expect(page.getByTestId('other')).toHaveText(otherBefore ?? '')
})

test('an optional prop is absent from the first visit [protocol:optional.full]', async ({ page, request }) => {
  await page.goto('/props')
  await expect(page.getByTestId('optional')).toHaveText('optional: not loaded')

  const full = await inertiaGet(request, '/props')
  expect(await full.json()).not.toHaveProperty('props.optional')
})

test('an optional prop is sent when a partial reload asks for it by name [protocol:optional.partial]', async ({
  page,
}) => {
  await page.goto('/props')
  const res = await visitDuring(page, '/props', () =>
    page.getByRole('button', { name: 'Load optional' }).click(),
  )
  expect(res.request().headers()['x-inertia-partial-data']).toBe('optional')
  expect((await res.json()).props.optional).toMatch(/^optional #\d+$/)
  await expect(page.getByTestId('optional')).toHaveText(/^optional #\d+$/)
})

test('partial paths are dot-paths and top-level dot keys unpack into nested objects [protocol:partial.dot-notation]', async ({
  page,
}) => {
  await page.goto('/variants')
  // "auth.name" and "auth.role" were registered as dot keys; the first visit nests them.
  await expect(page.getByTestId('props')).toContainText('"auth":{"name":"Ada","role":"admin"}')

  const res = await visitDuring(
    page,
    '/variants',
    () => page.getByRole('button', { name: 'Reload auth.name' }).click(),
    'GET',
    'auth.name',
  )
  expect(res.request().headers()['x-inertia-partial-data']).toBe('auth.name')
  expect((await res.json()).props.auth).toEqual({ name: 'Ada' })
})

test('children of a resolved closure bypass the partial filters [protocol:partial.resolved-children]', async ({
  page,
}) => {
  await page.goto('/variants')
  const res = await visitDuring(
    page,
    '/variants',
    () => page.getByRole('button', { name: 'Reload bag.x' }).click(),
    'GET',
    'bag.x',
  )
  expect(res.request().headers()['x-inertia-partial-data']).toBe('bag.x')
  // Only bag.x was asked for, yet the closure's other child y arrives with it.
  const bag = (await res.json()).props.bag
  expect(bag.x).toBe(1)
  expect(bag).toHaveProperty('y')
  await expect(page.getByTestId('props')).toContainText('"bag":{"x":1,"y":')
})
