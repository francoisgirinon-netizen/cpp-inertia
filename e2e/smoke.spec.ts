import { expect, test } from '@playwright/test'

test('GET / renders the Home page through the Inertia client', async ({ page }) => {
  await page.goto('/')
  await expect(page.getByRole('heading', { name: 'Home' })).toBeVisible()

  // Client-side navigation proves the real client booted from <script data-page>.
  await page.getByRole('link', { name: 'Users' }).click()
  await expect(page.getByRole('heading', { name: 'Users' })).toBeVisible()
  await expect(page).toHaveURL(/\/users$/)
})
