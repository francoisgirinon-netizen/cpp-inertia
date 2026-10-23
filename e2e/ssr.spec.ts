import { expect, test } from '@playwright/test'
import { ssrMode } from './helpers'

// JavaScript is off, so what the page shows is exactly what the server sent. The suite runs once
// without the SSR server (INERTIA_SSR unset) and once with it (INERTIA_SSR=1); the same test
// asserts the matching expectation for each mode.
test.use({ javaScriptEnabled: false })

test('server-side rendering fills the root element, and client-side mode leaves it empty [protocol:ssr.render]', async ({
  page,
}) => {
  await page.goto('/')
  const root = page.locator('#app')
  if (ssrMode) {
    await expect(root).toHaveAttribute('data-server-rendered', 'true')
    await expect(root.getByRole('heading', { name: 'Home' })).toBeVisible()
    await expect(root.getByText('Hello from C++')).toBeVisible()
    await expect(root.getByRole('link', { name: 'Users' })).toHaveAttribute('href', '/users')
  } else {
    await expect(root).not.toHaveAttribute('data-server-rendered', 'true')
    await expect(root).toBeEmpty()
  }
  // Either way the page object is in the markup for the client to boot from.
  await expect(page.locator('script[data-page="app"]')).toHaveCount(1)
})
