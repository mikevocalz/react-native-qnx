import { source } from '@/lib/source';
import { DocsLayout } from 'fumadocs-ui/layouts/docs';
import { baseOptions } from '@/lib/layout.shared';
import { VersionSelect } from '@/components/version-select';
import { getVersions } from '@/lib/versions';

export default function Layout({ children }: LayoutProps<'/docs'>) {
  const { options, current } = getVersions();
  return (
    <DocsLayout
      tree={source.getPageTree()}
      {...baseOptions()}
      sidebar={{ banner: <VersionSelect options={options} current={current} /> }}
    >
      {children}
    </DocsLayout>
  );
}
