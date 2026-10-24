import { expect, test } from '@playwright/test'
import { visitDuring } from './helpers'

function titles(page, testId) {
  return page.getByTestId(testId).getByRole('listitem')
}

test('a merge prop appends the next page to the list already on screen [protocol:merge.append]', async ({
  page,
}) => {
  await page.goto('/posts')
  await expect(titles(page, 'posts')).toHaveText(['Post 1', 'Post 2', 'Post 3'])

  const res = await visitDuring(
    page,
    '/posts',
    () => page.getByRole('button', { name: 'Load more' }).click(),
    'GET',
    'posts,page',
  )
  const body = await res.json()
  expect(body.mergeProps).toContain('posts')
  // Post 3 is re-sent (edited) for matchOn to replace; the other three are the new page.
  expect(body.props.posts.map((post) => post.id)).toEqual([3, 4, 5, 6])
  await expect(titles(page, 'posts')).toHaveText([
    'Post 1',
    'Post 2',
    'Post 3 (edited)',
    'Post 4',
    'Post 5',
    'Post 6',
  ])
})

test('a prepend prop puts new entries before the existing ones [protocol:merge.prepend]', async ({
  page,
}) => {
  await page.goto('/posts')
  await expect(titles(page, 'latest')).toHaveText(['Post 100'])

  const res = await visitDuring(
    page,
    '/posts',
    () => page.getByRole('button', { name: 'Prepend latest' }).click(),
    'GET',
    'latest',
  )
  expect((await res.json()).prependProps).toContain('latest')
  await expect(titles(page, 'latest')).toHaveText(['Post 101', 'Post 100'])
})

test('a deep merge prop merges nested objects instead of replacing them [protocol:merge.deep]', async ({
  page,
}) => {
  await page.goto('/posts')
  await expect(page.getByTestId('tags')).toHaveText('Tags: tag1')

  const res = await visitDuring(
    page,
    '/posts',
    () => page.getByRole('button', { name: 'Add tag' }).click(),
    'GET',
    'settings',
  )
  const body = await res.json()
  expect(body.deepMergeProps).toEqual(['settings'])
  expect(body.props.settings.filters.tags).toEqual(['tag2']) // only the new tag is sent
  await expect(page.getByTestId('tags')).toHaveText('Tags: tag1, tag2')
})

test('nested merge paths append one field and prepend another [protocol:merge.nested-paths]', async ({
  page,
}) => {
  await page.goto('/posts')
  await expect(titles(page, 'thread-data')).toHaveText(['Post 1', 'Post 2'])
  await expect(titles(page, 'thread-meta')).toHaveText(['Post 101'])

  const res = await visitDuring(
    page,
    '/posts',
    () => page.getByRole('button', { name: 'Grow thread' }).click(),
    'GET',
    'thread',
  )
  const body = await res.json()
  expect(body.mergeProps).toEqual(['thread.data'])
  expect(body.prependProps).toEqual(['thread.meta'])
  await expect(titles(page, 'thread-data')).toHaveText(['Post 1', 'Post 2', 'Post 3', 'Post 4'])
  await expect(titles(page, 'thread-meta')).toHaveText(['Post 102', 'Post 101'])
})

test('matchOn keys travel in matchPropsOn so the client replaces entries with the same id [protocol:merge.match-on]', async ({
  page,
}) => {
  await page.goto('/posts')
  await expect(titles(page, 'posts')).toHaveText(['Post 1', 'Post 2', 'Post 3'])
  const res = await visitDuring(
    page,
    '/posts',
    () => page.getByRole('button', { name: 'Load more' }).click(),
    'GET',
    'posts,page',
  )
  const body = await res.json()
  expect(body.matchPropsOn).toEqual(expect.arrayContaining(['posts.id']))
  // The reload carries post 3, already on screen. The client replaces it in place: the list
  // keeps six entries, post 3 shows its new title, and no id appears twice.
  expect(body.props.posts.map((post) => post.id)).toContain(3)
  await expect(titles(page, 'posts')).toHaveText([
    'Post 1',
    'Post 2',
    'Post 3 (edited)',
    'Post 4',
    'Post 5',
    'Post 6',
  ])
})

test('reset replaces the merged list instead of merging into it [protocol:merge.reset]', async ({
  page,
}) => {
  await page.goto('/posts')
  await page.getByRole('button', { name: 'Load more' }).click()
  await expect(titles(page, 'posts')).toHaveCount(6)

  const res = await visitDuring(
    page,
    '/posts',
    () => page.getByRole('button', { name: 'Reset' }).click(),
  )
  expect(res.request().headers()['x-inertia-reset']).toBe('posts')
  expect(await res.json()).not.toHaveProperty('mergeProps') // no merge label for a reset prop
  await expect(titles(page, 'posts')).toHaveText(['Post 1', 'Post 2', 'Post 3'])
})

test('merge metadata only lists paths that the partial reload includes [protocol:merge.partial-filter]', async ({
  page,
}) => {
  await page.goto('/posts')
  const full = await page.evaluate(() => JSON.parse(document.querySelector('script[data-page]').textContent))
  expect(full.mergeProps).toEqual(['posts', 'thread.data'])

  const res = await visitDuring(
    page,
    '/posts',
    () => page.getByRole('button', { name: 'Add tag' }).click(),
    'GET',
    'settings',
  )
  const body = await res.json()
  expect(body).not.toHaveProperty('mergeProps')
  expect(body).not.toHaveProperty('prependProps')
  expect(body.deepMergeProps).toEqual(['settings'])
})
