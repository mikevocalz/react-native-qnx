import type { MetadataRoute } from 'next';
import { source } from '@/lib/source';
import { basePath } from '@/lib/shared';

export const dynamic = 'force-static';

// Set DOCS_SITE_URL to the origin (https://mikevocalz.github.io) when building for a host,
// so the sitemap carries real absolute URLs. The base path is added here.
const base = `${(process.env.DOCS_SITE_URL ?? 'http://localhost:3000').replace(/\/$/, '')}${basePath}`;

export default function sitemap(): MetadataRoute.Sitemap {
  return [
    { url: `${base}/` },
    { url: `${base}/releases` },
    ...source.getPages().map((page) => ({ url: `${base}${page.url}` })),
  ];
}
