import { readFileSync } from 'node:fs';
import { join } from 'node:path';

// Read at build time (static export). Paths are relative to docs-site/.
const site = process.cwd();

export interface ComponentRow {
  name: string;
  host: string;
  pi400: string;
}

export function componentRows(): ComponentRow[] {
  const file = join(site, '..', 'docs', 'component-support.json');
  const data = JSON.parse(readFileSync(file, 'utf8')) as { components: ComponentRow[] };
  return data.components.map(({ name, host, pi400 }) => ({ name, host, pi400 }));
}

export interface MergedPull {
  number: string;
  url: string;
  date: string;
  title: string;
  sha: string;
}

export interface PullMonth {
  month: string;
  pulls: MergedPull[];
}

// The generated changelog page is the single source for merged pull requests.
export function mergedPulls(): PullMonth[] {
  const text = readFileSync(join(site, 'content/docs/reference/changelog.mdx'), 'utf8');
  const months: PullMonth[] = [];
  for (const line of text.split('\n')) {
    const heading = line.match(/^## (\d{4}-\d{2})\s*$/);
    if (heading) {
      months.push({ month: heading[1], pulls: [] });
      continue;
    }
    const row = line.match(
      /^\| \[#(\d+)\]\(([^)]+)\) \| (\d{4}-\d{2}-\d{2}) \| (.+) \| `([0-9a-f]+)` \|\s*$/
    );
    if (row && months.length)
      months[months.length - 1].pulls.push({
        number: row[1],
        url: row[2],
        date: row[3],
        title: row[4].replace(/\\\|/g, '|'),
        sha: row[5],
      });
  }
  return months.filter((m) => m.pulls.length);
}

export interface NoteGroup {
  heading: string;
  items: string[];
}

// The hand-written "Unreleased" notes from the repository's CHANGELOG.md.
export function unreleasedNotes(): NoteGroup[] {
  const text = readFileSync(join(site, '..', 'CHANGELOG.md'), 'utf8');
  const start = text.indexOf('\n## Unreleased');
  if (start < 0) return [];
  const rest = text
    .slice(start + 1)
    .split('\n')
    .slice(1);
  const groups: NoteGroup[] = [];
  for (const line of rest) {
    if (/^## /.test(line)) break;
    const heading = line.match(/^### (.+)$/);
    if (heading) {
      groups.push({ heading: heading[1].trim(), items: [] });
      continue;
    }
    const group = groups[groups.length - 1];
    if (!group) continue;
    const bullet = line.match(/^- (.*)$/);
    if (bullet) group.items.push(bullet[1]);
    else if (line.trim() && group.items.length)
      group.items[group.items.length - 1] += ' ' + line.trim();
  }
  return groups.filter((g) => g.items.length);
}
