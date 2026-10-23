import { createInertiaApp } from '@inertiajs/react'
import createServer from '@inertiajs/react/server'
import type { ComponentType } from 'react'
import ReactDOMServer from 'react-dom/server'

const pages = import.meta.glob<{ default: ComponentType }>('./pages/**/*.tsx', { eager: true })

// Listens on :13714. The Drogon server POSTs each first-visit page object to /render and embeds
// the returned head and body in its HTML; the browser then hydrates it (see app.tsx).
createServer((page) =>
  createInertiaApp({
    page,
    render: ReactDOMServer.renderToString,
    resolve: (name) => pages[`./pages/${name}.tsx`],
    setup: ({ App, props }) => <App {...props} />,
  }),
)
