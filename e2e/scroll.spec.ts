import { expect, test } from '@playwright/test'
import { inertiaGet, visitDuring } from './helpers'

test('a scroll prop carries paging metadata in scrollProps [protocol:scroll.metadata]', async ({
  page,
}) => {
  await page.goto('/feed')
  const data = await page.evaluate(() => JSON.parse(document.querySelector('script[data-page]').textContent))
  expect(data.scrollProps).toEqual({
    items: { pageName: 'page', previousPage: null, nextPage: 2, currentPage: 1, reset: false },
  })
  await expect(page.getByRole('listitem')).toHaveCount(10)
  await expect(page.getByRole('button', { name: 'Load more' })).toBeVisible()
})

test('loading the next page asks to append and merges it into the list [protocol:scroll.merge-intent]', async ({
  page,
  request,
}) => {
  await page.goto('/feed')
  const res = await visitDuring(
    page,
    '/feed',
    () => page.getByRole('button', { name: 'Load more' }).click(),
    'GET',
    'items',
  )
  expect(res.request().headers()['x-inertia-infinite-scroll-merge-intent']).toBe('append')
  const body = await res.json()
  expect(body.mergeProps).toEqual(['items.data'])
  expect(body).not.toHaveProperty('prependProps')
  expect(body.scrollProps.items.currentPage).toBe(2)
  // The client may fetch yet another page on its own once the list is long enough to need it,
  // so assert "at least two pages, in order, without duplicates" rather than an exact count.
  const items = page.getByRole('listitem')
  await expect.poll(() => items.count()).toBeGreaterThanOrEqual(20)
  const texts = await items.allTextContents()
  expect(texts).toEqual(texts.map((_text, index) => `Item ${index + 1}`))

  // A prepend intent (loading earlier pages) moves the merge label to prependProps.
  const wire = await inertiaGet(request, '/feed?page=2', {
    'X-Inertia-Partial-Component': 'Feed',
    'X-Inertia-Partial-Data': 'items',
    'X-Inertia-Infinite-Scroll-Merge-Intent': 'prepend',
  })
  const prepended = await wire.json()
  expect(prepended.prependProps).toEqual(['items.data'])
  expect(prepended).not.toHaveProperty('mergeProps')
})

test('a reset scroll prop is flagged and loses its merge label [protocol:scroll.reset]', async ({
  request,
}) => {
  const res = await inertiaGet(request, '/feed?page=2', {
    'X-Inertia-Partial-Component': 'Feed',
    'X-Inertia-Partial-Data': 'items',
    'X-Inertia-Reset': 'items',
  })
  const body = await res.json()
  expect(body.scrollProps.items.reset).toBe(true)
  expect(body).not.toHaveProperty('mergeProps')
  expect(body).not.toHaveProperty('prependProps')
})

test('a deferred scroll prop is deferred with merge metadata but no scrollProps until loaded [protocol:scroll.deferred]', async ({
  page,
}) => {
  const [follow] = await Promise.all([
    page.waitForResponse((r) => r.request().headers()['x-inertia-partial-data'] === 'lazyItems'),
    page.goto('/variants'),
  ])
  const first = await page.evaluate(() => JSON.parse(document.querySelector('script[data-page]').textContent))
  expect(first.deferredProps.lazy).toEqual(['lazyItems'])
  expect(first.mergeProps).toEqual(['lazyItems.data'])
  expect(first).not.toHaveProperty('scrollProps')
  expect(first.props).not.toHaveProperty('lazyItems')

  const loaded = await follow.json()
  expect(loaded.scrollProps.lazyItems.currentPage).toBe(1)
  await expect(page.getByTestId('lazy-loaded')).toBeVisible()
})
