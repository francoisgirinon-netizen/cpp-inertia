import { expect, test } from '@playwright/test'
import { readFileSync, writeFileSync } from 'node:fs'
import { isVisit } from './helpers'

// This test rewrites the Vite manifest that the server hashes per request, so it must not run
// next to any other test.
test.describe.configure({ mode: 'serial' })

const manifest = new URL('../examples/react-app/server/public/build/.vite/manifest.json', import.meta.url)
  .pathname

test('a stale asset version gets 409 + X-Inertia-Location and the client does a full page load [protocol:version.mismatch]', async ({
  page,
}) => {
  const original = readFileSync(manifest, 'utf8')
  try {
    await page.goto('/')
    await expect(page.getByRole('heading', { name: 'Home' })).toBeVisible()
    // Marks the document: a full page load would discard it.
    await page.evaluate(() => {
      document.documentElement.dataset.sameDocument = '1'
    })

    // A "new build": the manifest changes, so the server's version no longer matches the page's.
    writeFileSync(manifest, `${original}\n`)

    const [res] = await Promise.all([
      page.waitForResponse(isVisit('/users')),
      page.getByRole('link', { name: 'Users' }).click(),
    ])
    expect(res.status()).toBe(409)
    expect(res.headers()['x-inertia-location']).toMatch(/\/users$/)

    await expect(page.getByRole('heading', { name: 'Users' })).toBeVisible()
    await expect(page).toHaveURL(/\/users$/)
    await expect(page.locator('html')).not.toHaveAttribute('data-same-document', '1')
  } finally {
    writeFileSync(manifest, original)
  }
})
