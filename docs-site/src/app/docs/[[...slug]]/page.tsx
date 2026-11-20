import { source } from '@/lib/source';
import {
  DocsBody,
  DocsDescription,
  DocsPage,
  DocsTitle,
  MarkdownCopyButton,
  ViewOptionsPopover,
} from 'fumadocs-ui/layouts/docs/page';
import { notFound } from 'next/navigation';
import { getMDXComponents } from '@/components/mdx';
import type { Metadata } from 'next';
import { createRelativeLink } from 'fumadocs-ui/mdx';
import { getPageImageUrl, gitConfig, withBase } from '@/lib/shared';

function generatedSource(path: string): string | null {
  if (path.startsWith('reference/components/') || path === 'reference/component-matrix.mdx')
    return 'docs/component-support.json';
  if (path === 'reference/release-gates.mdx') return 'docs/releases/v1-readiness.json';
  if (path === 'explanation/adr-index.mdx') return 'docs/adr/index.md';
  if (path === 'reference/changelog.mdx') return 'CHANGELOG.md';
  return null;
}

export default async function Page(props: PageProps<'/docs/[[...slug]]'>) {
  const params = await props.params;
  const page = source.getPage(params.slug);
  if (!page) notFound();

  const MDX = page.data.body;
  const markdownUrl = withBase(`/llms.mdx${page.url}.md`);
  const repoUrl = `https://github.com/${gitConfig.user}/${gitConfig.repo}`;
  const sourcePath = `${gitConfig.contentDir}/${page.path}`;
  // Generated pages are rebuilt from their data source, so an edit to the MDX
  // would be overwritten. Send the editor to the data file instead.
  const dataSource = generatedSource(page.path);
  const editUrl = `${repoUrl}/edit/${gitConfig.branch}/${encodeURI(dataSource ?? sourcePath)}`;
  const issueUrl = `${repoUrl}/issues/new?title=${encodeURIComponent(`docs: ${page.data.title}`)}&body=${encodeURIComponent(`Page: ${page.url}\nSource: ${dataSource ?? sourcePath}\n\nWhat is wrong:\n`)}`;

  return (
    <DocsPage toc={page.data.toc} full={page.data.full}>
      <DocsTitle>{page.data.title}</DocsTitle>
      <DocsDescription className="mb-0">{page.data.description}</DocsDescription>
      <div className="flex flex-row gap-2 items-center border-b pb-6">
        <MarkdownCopyButton markdownUrl={markdownUrl} />
        <ViewOptionsPopover
          markdownUrl={markdownUrl}
          githubUrl={`https://github.com/${gitConfig.user}/${gitConfig.repo}/blob/${gitConfig.branch}/${gitConfig.contentDir}/${encodeURI(page.path)}`}
        />
      </div>
      <DocsBody>
        <MDX
          components={getMDXComponents({
            // this allows you to link to other pages with relative file paths
            a: createRelativeLink(source, page),
          })}
        />
      </DocsBody>
      <div className="flex flex-row flex-wrap gap-x-6 gap-y-2 border-t pt-6 text-sm">
        <a
          className="text-fd-muted-foreground underline underline-offset-4 hover:text-fd-foreground"
          href={editUrl}
          rel="noreferrer noopener"
          target="_blank"
        >
          Edit this page on GitHub
        </a>
        <a
          className="text-fd-muted-foreground underline underline-offset-4 hover:text-fd-foreground"
          href={issueUrl}
          rel="noreferrer noopener"
          target="_blank"
        >
          Report a problem with this page
        </a>
      </div>
    </DocsPage>
  );
}

export async function generateStaticParams() {
  return source.generateParams();
}

export async function generateMetadata(props: PageProps<'/docs/[[...slug]]'>): Promise<Metadata> {
  const params = await props.params;
  const page = source.getPage(params.slug);
  if (!page) notFound();

  return {
    title: page.data.title,
    description: page.data.description,
    openGraph: {
      images: getPageImageUrl(page).url,
    },
  };
}
