import type { APIRequestContext, Page, Request, Response } from '@playwright/test'

// True when the suite runs against the server with INERTIA_SSR=1 (see playwright.config.ts).
export const ssrMode = process.env.INERTIA_SSR === '1'

export const inertiaHeaders = { 'X-Inertia': 'true', Accept: 'text/html, application/xhtml+xml' }

// An Inertia XHR (not an asset or a prefetch of the initial document) for `path`, matched on the
// path only so query strings the client adds do not matter.
// `partialData` additionally requires that exact X-Inertia-Partial-Data header (deferred props
// start their own reloads, so a click can race with them).
export function isVisit(path: string, method = 'GET', partialData?: string) {
  return (res: Response) => {
    const req = res.request()
    return (
      new URL(res.url()).pathname === path &&
      req.method() === method &&
      req.headers()['x-inertia'] === 'true' &&
      (partialData === undefined || req.headers()['x-inertia-partial-data'] === partialData)
    )
  }
}

// Resolves with the Inertia response produced by `action`.
export async function visitDuring(
  page: Page,
  path: string,
  action: () => Promise<unknown>,
  method = 'GET',
  partialData?: string,
): Promise<Response> {
  const [res] = await Promise.all([
    page.waitForResponse(isVisit(path, method, partialData)),
    action(),
  ])
  return res
}

export function reqHeader(req: Request, name: string) {
  return req.headers()[name.toLowerCase()]
}

// The asset version the server currently advertises (read from a first visit).
export async function currentVersion(request: APIRequestContext): Promise<string> {
  const html = await (await request.get('/')).text()
  const raw = /<script data-page="app" type="application\/json">(.*?)<\/script>/s.exec(html)?.[1] ?? '{}'
  return JSON.parse(raw).version as string
}

// A hand-built Inertia request, for rules the example's UI cannot trigger (extra headers).
export async function inertiaGet(
  request: APIRequestContext,
  path: string,
  headers: Record<string, string> = {},
) {
  const version = await currentVersion(request)
  return request.get(path, {
    headers: { 'X-Inertia': 'true', 'X-Inertia-Version': version, ...headers },
  })
}

// The response the browser fetched after following the redirect that answered `fromPath`. (The
// followed request is a fresh GET whose own headers Playwright does not report, so it is matched
// through the redirect chain.)
export function followedRedirectFrom(fromPath: string, toPath: string) {
  return (res: Response) =>
    new URL(res.url()).pathname === toPath &&
    res.request().redirectedFrom() !== null &&
    new URL(res.request().redirectedFrom()?.url() ?? "").pathname === fromPath
}

// The "Rendered N times" counter of the Users page, as a number.
export function renderCount(text: string | null) {
  return Number((text ?? '').replace('Rendered ', '').replace(' times', ''))
}
