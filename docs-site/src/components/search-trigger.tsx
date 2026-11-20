'use client';
import { FullSearchTrigger, SearchTrigger } from 'fumadocs-ui/layouts/shared/slots/search-trigger';
import { cn } from '@/lib/cn';

/*
 * One search control for every layout. Fumadocs' HomeLayout and DocsLayout each pass their
 * own className to the trigger (a full pill capped at 240px on one, a 12px box up to 384px on
 * the other), and each hides it at a different breakpoint. Both layouts use these slots instead,
 * so the radius, height, width, border, fill and hover are written once, here.
 *
 * Radius is --rq-r-m (10px): the radius of the Bench primary buttons. The 6px --rq-r-s is for
 * chips and inline code, and --rq-r-l is for cards and the dialog.
 */
const chrome =
  'h-9 rounded-[var(--rq-r-m)] border border-fd-border bg-fd-secondary text-sm text-fd-muted-foreground transition-colors hover:bg-fd-accent hover:text-fd-accent-foreground';

const full = cn(chrome, 'my-auto w-60 shrink-0 py-0 ps-2.5 pe-1.5');
const icon = cn(chrome, 'my-auto size-9 shrink-0 p-0 [&_svg]:size-4');

type SlotProps = { hideIfDisabled?: boolean };

/** Full search field from 1024px up, the icon button from 768px to 1023px. Below that the layout's own icon slot shows. */
export function SearchTriggerFull({ hideIfDisabled }: SlotProps) {
  return (
    <>
      <FullSearchTrigger hideIfDisabled={hideIfDisabled} className={cn(full, 'max-lg:hidden')} />
      <SearchTrigger
        hideIfDisabled={hideIfDisabled}
        className={cn(icon, 'hidden md:max-lg:inline-flex')}
      />
    </>
  );
}

/** Icon button for the small-screen slot of either layout. */
export function SearchTriggerIcon({ hideIfDisabled }: SlotProps) {
  return <SearchTrigger hideIfDisabled={hideIfDisabled} className={icon} />;
}
