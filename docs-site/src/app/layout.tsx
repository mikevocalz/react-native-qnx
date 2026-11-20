import type { Metadata } from 'next';
import { Provider } from '@/components/provider';
import { mono, sans } from './fonts';
import './global.css';
import './bench.css';

export const metadata: Metadata = {
  // Origin only: Open Graph URLs resolve against it. The base path is already in each URL.
  metadataBase: new URL(process.env.DOCS_SITE_URL ?? 'http://localhost:3000'),
  title: { default: 'React Native QNX', template: '%s | React Native QNX' },
  description:
    'Documentation for React Native QNX: React Native 0.88 on QNX with Fabric, Hermes, Yoga and Qt Quick.',
};

export default function Layout({ children }: LayoutProps<'/'>) {
  return (
    <html lang="en" className={`${sans.variable} ${mono.variable}`} suppressHydrationWarning>
      <body className="flex flex-col min-h-screen">
        <Provider>{children}</Provider>
      </body>
    </html>
  );
}
