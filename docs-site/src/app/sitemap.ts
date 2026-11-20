import type { MetadataRoute } from 'next';
import { source } from '@/lib/source';

export const dynamic = 'force-static';

// The site is not deployed from this repository. Set DOCS_SITE_URL when
// building for a host so the sitemap carries real absolute URLs.
const base = (process.env.DOCS_SITE_URL ?? 'http://localhost:3000').replace(/\/$/, '');

export default function sitemap(): MetadataRoute.Sitemap {
  return [{ url: `${base}/` }, ...source.getPages().map((page) => ({ url: `${base}${page.url}` }))];
}
