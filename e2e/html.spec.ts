import { expect, test } from '@playwright/test'
import { inertiaGet, ssrMode } from './helpers'

test('first visit is HTML with a data-page script element and a root div [protocol:html.script-element]', async ({
  request,
  page,
}) => {
  const res = await request.get('/')
  expect(res.status()).toBe(200)
  expect(res.headers()['content-type']).toContain('text/html')
  const html = await res.text()
  expect(html).toMatch(/<script data-page="app" type="application\/json">\{.*\}<\/script>/s)
  expect(html).toMatch(/<div [^>]*id="app"/)

  // The official client boots from exactly that markup.
  await page.goto('/')
  await expect(page.getByRole('heading', { name: 'Home' })).toBeVisible()
  await expect(page.locator('script[data-page="app"]')).toHaveAttribute('type', 'application/json')
})

test('page JSON in the script element escapes slash, < and > yet parses to the original [protocol:html.json-escaping]', async ({
  request,
  page,
}) => {
  const res = await request.get('/users?search=%3Cb%3E')
  const html = await res.text()
  const raw = /<script data-page="app" type="application\/json">(.*?)<\/script>/s.exec(html)?.[1] ?? ''
  expect(raw).toContain('"component":"Users\\/Index"')
  expect(raw).toMatch(/\\u003[cC]b/)
  expect(raw).not.toContain('<')
  expect(raw).not.toContain('&lt;')
  expect(raw).not.toContain('&#')
  if (!ssrMode) {
    // With SSR the script element is the SSR server's own output, which leaves ">" alone.
    expect(raw).toContain('\\u003Cb\\u003E')
    expect(raw).not.toContain('>')
  }

  await page.goto('/users?search=%3Cb%3E')
  await expect(page.getByLabel('Search')).toHaveValue('<b>')
})

test('the root element id is the default app and matches data-page [protocol:html.root-id]', async ({
  page,
}) => {
  await page.goto('/')
  await expect(page.locator('#app')).toBeVisible()
  // The client mounts into the element whose id equals the data-page attribute.
  await expect(page.locator('script[data-page]')).toHaveAttribute('data-page', 'app')
  await expect(page.locator('#app').getByRole('heading', { name: 'Home' })).toBeVisible()
})

test('an Inertia request gets 200 JSON with X-Inertia and the page object [protocol:json.response]', async ({
  page,
}) => {
  await page.goto('/')
  const [res] = await Promise.all([
    page.waitForResponse((r) => new URL(r.url()).pathname === '/users' && r.request().headers()['x-inertia'] === 'true'),
    page.getByRole('link', { name: 'Users' }).click(),
  ])
  expect(res.status()).toBe(200)
  expect(res.headers()['x-inertia']).toBe('true')
  expect(res.headers()['content-type']).toContain('application/json')
  const body = await res.json()
  expect(body.component).toBe('Users/Index')
  expect(body.url).toBe('/users')
  await expect(page.getByRole('heading', { name: 'Users' })).toBeVisible()
})

test('every response carries Vary: X-Inertia, HTML, JSON and 409 alike [protocol:vary.always]', async ({
  page,
  request,
}) => {
  const html = await request.get('/')
  expect(html.headers()['vary']).toMatch(/X-Inertia/i)

  await page.goto('/')
  const [json] = await Promise.all([
    page.waitForResponse((r) => new URL(r.url()).pathname === '/users' && r.request().headers()['x-inertia'] === 'true'),
    page.getByRole('link', { name: 'Users' }).click(),
  ])
  expect(json.headers()['vary']).toMatch(/X-Inertia/i)

  const conflict = await request.get('/users', {
    headers: { 'X-Inertia': 'true', 'X-Inertia-Version': 'stale' },
  })
  expect(conflict.status()).toBe(409)
  expect(conflict.headers()['vary']).toMatch(/X-Inertia/i)
})

test('page.shared-props: a shared prop reaches every page and is listed in sharedProps [protocol:page.shared-props]', async ({
  page,
  request,
}) => {
  await page.goto('/')
  await expect(page.getByText('cpp-inertia demo')).toBeVisible()
  await page.getByRole('link', { name: 'Props' }).click()
  await expect(page.getByRole('heading', { name: 'Props' })).toBeVisible()
  await expect(page.getByText('cpp-inertia demo')).toBeVisible()

  const res = await inertiaGet(request, '/props')
  const body = await res.json()
  expect(body.props.appName).toBe('cpp-inertia demo')
  expect(body.sharedProps).toEqual(expect.arrayContaining(['appName', 'errors']))
})

test('page.bigint: integers beyond 2^53 arrive as {"$bigint"} and keep their digits [protocol:page.bigint]', async ({
  page,
  request,
}) => {
  const res = await inertiaGet(request, '/variants')
  const text = await res.text()
  expect(text).toContain('"$bigint":"9007199254740993"')
  expect(JSON.parse(text).preserveBigIntegers).toBe(true)

  // The client reads the same markup without rounding the number to 9007199254740992.
  await page.goto('/variants')
  const dump = await page.getByTestId('props').textContent()
  expect(dump).toContain('9007199254740993')
})

