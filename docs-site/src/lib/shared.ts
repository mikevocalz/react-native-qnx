import { createGetUrl } from 'fumadocs-core/source';

export const appName = 'React Native QNX';
export const docsRoute = '/docs';
export const docsImageRoute = '/og/docs';

export const gitConfig = {
  user: 'mikevocalz',
  repo: 'react-native-qnx',
  branch: 'main',
  contentDir: 'docs-site/content/docs',
};

const getImageUrl = createGetUrl(docsImageRoute);

export function getPageImageUrl(page: { slugs: string[]; locale?: string }) {
  const segments = [...page.slugs, 'image.png'];

  return { segments, url: getImageUrl(segments, page.locale) };
}

/** Path prefix of a frozen release build, for example /v/v1.0.0. Empty for latest. */
export const basePath = process.env.NEXT_PUBLIC_DOCS_BASE_PATH ?? '';
