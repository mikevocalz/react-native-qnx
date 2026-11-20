#!/usr/bin/env node
// The search trigger must be the same control in the home layout and the docs layout.
// Fumadocs gives each layout its own trigger classes (rounded-full on one, rounded-xl on
// the other); components/search-trigger.tsx replaces both. This reads the built HTML of one
// page per layout and fails if their triggers differ, so a layout option or a Fumadocs
// upgrade that brings the layout classes back is caught before it ships.
// Run after `next build`.
import { existsSync, readFileSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const out = resolve(dirname(fileURLToPath(import.meta.url)), '../out');
const pages = {
  'home layout, /': 'index.html',
  'home layout, /releases': 'releases.html',
  'docs layout, /docs': 'docs.html',
  'docs layout, /docs/gallery': 'docs/gallery.html',
  'docs layout, a component page': 'docs/reference/components/view.html',
  'docs layout, the status page': 'docs/reference/status.html',
};
const errors = [];
const signatures = new Map(); // sorted trigger class lists -> pages

for (const [name, file] of Object.entries(pages)) {
  const path = join(out, file);
  if (!existsSync(path)) {
    errors.push(`${name}: ${file} is not in the build`);
    continue;
  }
  const html = readFileSync(path, 'utf8');
  const triggers = [...html.matchAll(/<button\b[^>]*\bdata-(search-full|search)=""[^>]*>/g)];
  const kinds = new Set(triggers.map((m) => m[1]));
  for (const need of ['search-full', 'search'])
    if (!kinds.has(need)) errors.push(`${name}: no data-${need} trigger in ${file}`);
  const classes = triggers.map(
    (m) => `${m[1]}: ${(m[0].match(/\bclass="([^"]*)"/) ?? [])[1] ?? ''}`
  );
  for (const c of classes) {
    if (!c.includes('rounded-[var(--rq-r-m)]'))
      errors.push(`${name}: trigger is missing the --rq-r-m radius (${c})`);
    if (/\brounded-(full|xl|lg)\b/.test(c))
      errors.push(`${name}: trigger carries a layout radius class (${c})`);
  }
  const sig = classes.sort().join('\n');
  signatures.set(sig, [...(signatures.get(sig) ?? []), name]);
}
if (signatures.size > 1)
  errors.push(
    `the pages have ${signatures.size} different sets of search triggers:\n` +
      [...signatures]
        .map(([sig, p]) => `  ${p.join(', ')}\n    ${sig.replaceAll('\n', '\n    ')}`)
        .join('\n')
  );

if (errors.length) {
  console.error(errors.map((e) => `search check: ${e}`).join('\n'));
  process.exit(1);
}
console.log(`search ok: identical search triggers on ${Object.keys(pages).length} pages`);
