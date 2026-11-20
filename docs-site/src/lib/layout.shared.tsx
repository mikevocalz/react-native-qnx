import type { BaseLayoutProps, LinkItemType } from 'fumadocs-ui/layouts/shared';
import { SearchTriggerFull, SearchTriggerIcon } from '@/components/search-trigger';
import { appName, gitConfig } from './shared';

export function Wordmark() {
  return (
    <span className="rq-mark">
      <b>{appName}</b>
      <span>pre-v1</span>
    </span>
  );
}

export const navLinks: LinkItemType[] = [
  { text: 'Docs', url: '/docs', active: 'nested-url' },
  { text: 'Components', url: '/docs/reference/component-matrix', active: 'none' },
  { text: 'Releases', url: '/releases', active: 'url' },
  { text: 'Status', url: '/docs/reference/status', active: 'none' },
];

export function baseOptions(): BaseLayoutProps {
  return {
    nav: {
      title: <Wordmark />,
    },
    links: navLinks,
    // The same search trigger in every layout; see components/search-trigger.tsx.
    slots: { searchTrigger: { full: SearchTriggerFull, sm: SearchTriggerIcon } },
    githubUrl: `https://github.com/${gitConfig.user}/${gitConfig.repo}`,
  };
}
