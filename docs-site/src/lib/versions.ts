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
  const pkg = JSON.parse(readFileSync(join(process.cwd(), '..', 'package.json'), 'utf8')) as {
    version: string;
  };
  const { snapshots } = JSON.parse(readFileSync(join(process.cwd(), 'versions.json'), 'utf8')) as {
    snapshots: Snapshot[];
  };
  // A frozen build has DOCS_BASE_PATH=<root>/v/<tag>. Links between copies go
  // through <root>, which is the deploy base without that suffix.
  const root = basePath.replace(/\/v\/[^/]+$/, '');
  const options: VersionOption[] = [
    { label: `latest (main, ${pkg.version})`, href: `${root}/docs` },
    ...snapshots.map((s) => ({ label: s.tag, href: `${root}/v/${s.tag}/docs` })),
  ];
  const current = `${basePath}/docs`;
  // A frozen build predates its own versions.json entry, so list it explicitly.
  if (!options.some((o) => o.href === current))
    options.push({ label: basePath.replace(/^.*\/v\//, ''), href: current });
  return { options, current };
}
