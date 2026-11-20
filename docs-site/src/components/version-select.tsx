'use client';
import { useId } from 'react';

export interface VersionOption {
  /** Label shown in the list, for example "latest (0.1.0)" or "v1.0.0". */
  label: string;
  /** Site-absolute path of that copy of the docs. */
  href: string;
}

/**
 * Switches between the live docs (latest) and frozen copies built at a release
 * tag. With no snapshots it still shows which version latest describes.
 */
export function VersionSelect({ options, current }: { options: VersionOption[]; current: string }) {
  const id = useId();
  const single = options.length < 2;
  return (
    <div className="flex items-center gap-2 text-sm">
      <label htmlFor={id} className="text-fd-muted-foreground">
        Version
      </label>
      <select
        id={id}
        className="rounded-md border bg-fd-background px-2 py-1 text-fd-foreground"
        value={current}
        disabled={single}
        onChange={(e) => {
          const next = options.find((o) => o.href === e.target.value);
          if (next) window.location.assign(next.href);
        }}
      >
        {options.map((o) => (
          <option key={o.href} value={o.href}>
            {o.label}
          </option>
        ))}
      </select>
    </div>
  );
}
