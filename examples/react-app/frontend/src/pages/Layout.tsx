import { Link, usePage } from '@inertiajs/react'
import type { ReactNode } from 'react'

const links = [
  ['/', 'Home'],
  ['/users', 'Users'],
  ['/dashboard', 'Dashboard'],
  ['/feed', 'Feed'],
  ['/posts', 'Posts'],
  ['/history', 'History'],
  ['/external', 'External'],
  ['/fragment', 'Fragment'],
  ['/props', 'Props'],
  ['/login', 'Login'],
] as const

// Shared by every page: the navigation and the flash messages sent by the server.
export default function Layout({ children }: { children: ReactNode }) {
  const { props, flash } = usePage<{ appName: string }>()
  return (
    <>
      <nav style={{ display: 'flex', gap: 12 }}>
        <strong>{props.appName}</strong>
        {links.map(([href, label]) => (
          <Link key={href} href={href}>
            {label}
          </Link>
        ))}
      </nav>
      {flash.success && <p role="status">{String(flash.success)}</p>}
      <main>{children}</main>
    </>
  )
}
