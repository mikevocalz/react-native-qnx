import { createMDX } from 'fumadocs-mdx/next';
import { dirname } from 'node:path';
import { fileURLToPath } from 'node:url';

const withMDX = createMDX();

// GitHub project Pages serves the site under /<repo>. Set DOCS_BASE_PATH=/react-native-qnx
// for that build. Empty (the default) keeps local dev and the CI build at the root.
const basePath = (process.env.DOCS_BASE_PATH ?? '').replace(/\/+$/, '');

/** @type {import('next').NextConfig} */
const config = {
  output: 'export',
  ...(basePath ? { basePath } : {}),
  // Client code reads this to prefix URLs that Next does not rewrite (fetch, raw <img>, metadata).
  env: { NEXT_PUBLIC_BASE_PATH: basePath },
  reactStrictMode: true,
  // A static export has no image optimizer, so next/image would point at /_next/image and 404.
  images: { unoptimized: true },
  // The repository root has its own pnpm lockfile; pin Turbopack to this app.
  turbopack: { root: dirname(fileURLToPath(import.meta.url)) },
};

export default withMDX(config);
