import { readFileSync } from 'node:fs';
import { join } from 'node:path';
import type { VersionOption } from '@/components/version-select';
import { basePath } from './shared';

interface Snapshot {
  /** Release tag the copy was built from, for example v1.0.0. */
  tag: string;
}

/**
 * Version list for the selector. "latest" always describes main and is labelled
 * with the version field of the repository's root package.json at build time.
 * Frozen copies come from versions.json and live under /v/<tag>/.
 */
export function getVersions(): { options: VersionOption[]; current: string } {
  const root = JSON.parse(readFileSync(join(process.cwd(), '..', 'package.json'), 'utf8')) as {
    version: string;
  };
  const { snapshots } = JSON.parse(
    readFileSync(join(process.cwd(), 'versions.json'), 'utf8')
  ) as { snapshots: Snapshot[] };
  const options: VersionOption[] = [
    { label: `latest (main, ${root.version})`, href: '/docs' },
    ...snapshots.map((s) => ({ label: s.tag, href: `/v/${s.tag}/docs` })),
  ];
  const current = basePath ? `${basePath}/docs` : '/docs';
  // A frozen build predates its own versions.json entry, so list it explicitly.
  if (!options.some((o) => o.href === current))
    options.push({ label: basePath.replace(/^\/v\//, ''), href: current });
  return { options, current };
}
