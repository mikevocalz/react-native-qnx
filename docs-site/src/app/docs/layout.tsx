import { source } from '@/lib/source';
import { DocsLayout } from 'fumadocs-ui/layouts/notebook';
import { baseOptions, navLinks } from '@/lib/layout.shared';
import { NavLinks } from '@/components/nav-links';
import { VersionSelect } from '@/components/version-select';
import { getVersions } from '@/lib/versions';

export default function Layout({ children }: LayoutProps<'/docs'>) {
  const { options, current } = getVersions();
  return (
    <DocsLayout
      tree={source.getPageTree()}
      {...baseOptions()}
      nav={{ ...baseOptions().nav, mode: 'top', children: <NavLinks links={navLinks} /> }}
      sidebar={{
        defaultOpenLevel: 1,
        banner: <VersionSelect options={options} current={current} />,
      }}
    >
      {children}
    </DocsLayout>
  );
}
