import react from '@vitejs/plugin-react'
import fs from 'node:fs'
import path from 'node:path'
import { defineConfig, type Plugin } from 'vite'

// The Drogon server serves the build from, and looks for the hot file in, this directory.
const publicDir = path.resolve(import.meta.dirname, '../server/public')
const hotFile = path.join(publicDir, 'hot')
// The URL the *browser* uses to reach the dev server (it listens on 0.0.0.0 inside the container).
const devUrl = 'http://localhost:5173'

// While the dev server runs, public/hot holds its URL so the server emits dev tags.
function hotFilePlugin(): Plugin {
  return {
    name: 'hot-file',
    apply: 'serve',
    configureServer(server) {
      server.httpServer?.once('listening', () => {
        fs.mkdirSync(publicDir, { recursive: true })
        fs.writeFileSync(hotFile, devUrl)
      })
      const remove = () => fs.rmSync(hotFile, { force: true })
      server.httpServer?.once('close', remove)
      process.once('exit', remove)
      for (const signal of ['SIGINT', 'SIGTERM'] as const) {
        process.once(signal, () => process.exit())
      }
    },
  }
}

export default defineConfig(({ command, isSsrBuild }) => ({
  base: command === 'build' ? '/build/' : '/',
  plugins: [react(), hotFilePlugin()],
  server: { host: '0.0.0.0', port: 5173, strictPort: true, origin: devUrl },
  build: {
    // `vite build` writes the client bundle where the server serves it; `vite build --ssr` writes
    // the Node/Bun SSR bundle (dist/ssr/ssr.js) elsewhere, so it is never served to browsers.
    manifest: !isSsrBuild,
    outDir: isSsrBuild ? 'dist/ssr' : '../server/public/build',
    emptyOutDir: true,
    rollupOptions: { input: isSsrBuild ? 'src/ssr.tsx' : 'src/app.tsx' },
  },
}))
