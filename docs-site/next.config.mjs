import { createMDX } from 'fumadocs-mdx/next';
import { dirname } from 'node:path';
import { fileURLToPath } from 'node:url';

const withMDX = createMDX();

/** @type {import('next').NextConfig} */
const config = {
  output: 'export',
  reactStrictMode: true,
  // A static export has no image optimizer, so next/image would point at /_next/image and 404.
  images: { unoptimized: true },
  // The repository root has its own pnpm lockfile; pin Turbopack to this app.
  turbopack: { root: dirname(fileURLToPath(import.meta.url)) },
};

export default withMDX(config);
