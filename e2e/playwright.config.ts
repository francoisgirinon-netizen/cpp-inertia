import { defineConfig } from '@playwright/test'

// Runs against the production build: the Vite output in
// examples/react-app/server/public/build and the example server. INERTIA_E2E_SERVER picks the
// adapter it runs on: drogon (inertia_example, the default) or httplib (inertia_example_httplib).
// INERTIA_SSR=1 also starts the SSR server and turns on server-side rendering.
const ssr = process.env.INERTIA_SSR === '1'
const root = new URL('..', import.meta.url).pathname
const adapter = process.env.INERTIA_E2E_SERVER || 'drogon'
if (adapter !== 'drogon' && adapter !== 'httplib') {
  throw new Error(`INERTIA_E2E_SERVER must be drogon or httplib, got ${adapter}`)
}
const binary = adapter === 'httplib' ? 'inertia_example_httplib' : 'inertia_example'
const example = process.env.INERTIA_EXAMPLE_BIN ?? `${root}build/dev/examples/react-app/server/${binary}`

export default defineConfig({
  testDir: '.',
  testMatch: '*.spec.ts',
  fullyParallel: false,
  workers: 1,
  reporter: 'list',
  use: { baseURL: 'http://127.0.0.1:8081' },
  webServer: [
    {
      command: example,
      url: 'http://127.0.0.1:8081/',
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
