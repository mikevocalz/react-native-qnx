import { createGetUrl } from 'fumadocs-core/source';

export const appName = 'React Native QNX';
// Set at build time from DOCS_BASE_PATH (see next.config.mjs). Empty at the site root.
export const basePath = process.env.NEXT_PUBLIC_BASE_PATH ?? '';

/** Prefix a root-relative URL with the deploy base path. Use for fetch(), <img> and metadata URLs. */
export const withBase = (url: string) => (url.startsWith('/') ? `${basePath}${url}` : url);

export const docsRoute = '/docs';
export const docsImageRoute = '/og/docs';

export const gitConfig = {
  user: 'mikevocalz',
  repo: 'react-native-qnx',
  branch: process.env.DOCS_SOURCE_COMMIT ?? 'main',
  contentDir: 'docs-site/content/docs',
};

const getImageUrl = createGetUrl(docsImageRoute);

export function getPageImageUrl(page: { slugs: string[]; locale?: string }) {
  const segments = [...page.slugs, 'image.png'];

  return { segments, url: withBase(getImageUrl(segments, page.locale)) };
}
