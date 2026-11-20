import { Archivo, Azeret_Mono } from 'next/font/google';

// Archivo carries a width axis: the wordmark and display lines use it expanded
// (font-stretch: 125%), body text stays at normal width. One family, two voices.
export const sans = Archivo({
  subsets: ['latin'],
  axes: ['wdth'],
  style: ['normal', 'italic'],
  display: 'swap',
  variable: '--font-archivo',
  fallback: ['Helvetica Neue', 'Helvetica', 'Arial', 'system-ui', 'sans-serif'],
});

// Figures, code and measured labels.
export const mono = Azeret_Mono({
  subsets: ['latin'],
  display: 'swap',
  variable: '--font-azeret',
  fallback: ['ui-monospace', 'SF Mono', 'Menlo', 'Consolas', 'Liberation Mono', 'monospace'],
});
