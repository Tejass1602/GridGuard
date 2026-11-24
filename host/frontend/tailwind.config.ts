import type { Config } from 'tailwindcss'

export default {
  content: ['./index.html', './src/**/*.{ts,tsx}'],
  theme: { extend: { colors: { ink: '#020617', panel: '#0b1220', line: '#1e293b' }, fontFamily: { mono: ['JetBrains Mono', 'ui-monospace', 'monospace'] } } },
  plugins: [],
} satisfies Config
