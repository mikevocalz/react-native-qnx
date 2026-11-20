import type { MetadataRoute } from 'next';
import { basePath } from '@/lib/shared';

export const dynamic = 'force-static';

// DOCS_SITE_URL is the origin (https://mikevocalz.github.io). The base path is added here.
const base = `${(process.env.DOCS_SITE_URL ?? 'http://localhost:3000').replace(/\/$/, '')}${basePath}`;

export default function robots(): MetadataRoute.Robots {
  return {
    rules: { userAgent: '*', allow: '/' },
    sitemap: `${base}/sitemap.xml`,
  };
}
