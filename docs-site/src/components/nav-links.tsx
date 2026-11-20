'use client';
import Link from 'fumadocs-core/link';
import { usePathname } from 'fumadocs-core/framework';
import { isLinkItemActive } from 'fumadocs-ui/layouts/shared';
import type { LinkItemType } from 'fumadocs-ui/layouts/shared';

/**
 * The text links of the top bar, drawn next to the wordmark like the home layout does.
 * DocsLayout otherwise puts them on the right; bench.css hides that copy.
 */
export function NavLinks({ links }: { links: LinkItemType[] }) {
  const pathname = usePathname();
  return (
    <ul className="flex flex-row items-center gap-2 ps-6 max-lg:hidden">
      {links.map((item) =>
        item.type === 'custom' || item.type === 'menu' ? null : (
          <li key={item.url} className="list-none">
            <Link
              href={item.url}
              external={item.external}
              data-active={isLinkItemActive(item, pathname)}
              className="inline-flex items-center p-2 text-sm text-fd-muted-foreground transition-colors hover:text-fd-accent-foreground data-[active=true]:text-fd-primary"
            >
              {item.text}
            </Link>
          </li>
        )
      )}
    </ul>
  );
}
