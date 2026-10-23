import { defineConfig } from '@playwright/test'

// Runs against the production build: the Vite output in
// examples/react-app/server/public/build and the inertia_example binary. INERTIA_SSR=1 also starts
// the SSR server and turns on server-side rendering.
const ssr = process.env.INERTIA_SSR === '1'
const root = new URL('..', import.meta.url).pathname
const example = process.env.INERTIA_EXAMPLE_BIN ?? `${root}build/dev/examples/react-app/server/inertia_example`

export default defineConfig({
  testDir: '.',
  testMatch: '*.spec.ts',
  fullyParallel: false,
  workers: 1,
  reporter: 'list',
  use: { baseURL: 'http://127.0.0.1:8080' },
  webServer: [
    {
      command: example,
      url: 'http://127.0.0.1:8080/',
      env: { INERTIA_SSR: ssr ? '1' : '0' },
      reuseExistingServer: false,
      timeout: 30_000,
    },
    ...(ssr
      ? [
          {
            command: 'bun dist/ssr/ssr.js',
            cwd: `${root}examples/react-app/frontend`,
            url: 'http://127.0.0.1:13714/health',
            reuseExistingServer: false,
            timeout: 30_000,
          },
        ]
      : []),
  ],
})
