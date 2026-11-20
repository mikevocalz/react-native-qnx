import { createMDX } from 'fumadocs-mdx/next';
import { dirname } from 'node:path';
import { fileURLToPath } from 'node:url';

const withMDX = createMDX();

/** @type {import('next').NextConfig} */
const config = {
  output: 'export',
  // Frozen release copies are built with NEXT_PUBLIC_DOCS_BASE_PATH=/v/<tag>.
  basePath: process.env.NEXT_PUBLIC_DOCS_BASE_PATH || undefined,
  reactStrictMode: true,
  // A static export has no image optimizer, so next/image would point at /_next/image and 404.
  images: { unoptimized: true },
  // The repository root has its own pnpm lockfile; pin Turbopack to this app.
  turbopack: { root: dirname(fileURLToPath(import.meta.url)) },
};

export default withMDX(config);
