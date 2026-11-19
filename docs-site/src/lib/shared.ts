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
