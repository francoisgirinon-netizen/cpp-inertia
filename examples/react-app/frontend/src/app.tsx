import { createInertiaApp } from '@inertiajs/react'
import type { ComponentType } from 'react'

const pages = import.meta.glob<{ default: ComponentType }>('./pages/**/*.tsx', { eager: true })

createInertiaApp({
  resolve: (name) => pages[`./pages/${name}.tsx`],
})
