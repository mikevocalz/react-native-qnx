import type { BaseLayoutProps } from 'fumadocs-ui/layouts/shared';
import { appName, gitConfig } from './shared';

export function Wordmark() {
  return (
    <span className="rq-mark">
      <b>{appName}</b>
      <span>pre-v1</span>
    </span>
  );
}

export function baseOptions(): BaseLayoutProps {
  return {
    nav: {
      title: <Wordmark />,
    },
    links: [
      { text: 'Docs', url: '/docs', active: 'nested-url' },
      { text: 'Components', url: '/docs/reference/component-matrix', active: 'none' },
      { text: 'Releases', url: '/releases', active: 'url' },
      { text: 'Status', url: '/docs/reference/status', active: 'none' },
    ],
    githubUrl: `https://github.com/${gitConfig.user}/${gitConfig.repo}`,
  };
}
