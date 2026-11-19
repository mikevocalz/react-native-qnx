#!/usr/bin/env node
// Content checks that run in CI after `next build`:
//  1. no banned term anywhere in docs-site sources or built output,
//  2. every internal link in the MDX resolves to a page,
//  3. every page has a title and description,
//  4. llms.txt, SKILL.md and the search index exist in the build.
import { readdirSync, readFileSync, statSync, existsSync } from 'node:fs';
import { dirname, join, relative, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const site = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const docs = join(site, 'content/docs');
const errors = [];

const walk = (dir, out = []) => {
  for (const name of readdirSync(dir)) {
    if (['node_modules', '.next', '.source', 'out'].includes(name)) continue;
    const p = join(dir, name);
    statSync(p).isDirectory() ? walk(p, out) : out.push(p);
  }
  return out;
};

const banned = new RegExp(['mara', 'thon'].join(''), 'i');
for (const f of walk(site).filter((f) => /\.(mdx?|tsx?|json|mjs|txt|css)$/.test(f))) {
  if (f.endsWith('check-content.mjs')) continue;
  if (banned.test(readFileSync(f, 'utf8'))) errors.push(`banned term in ${relative(site, f)}`);
}

const pages = new Set();
for (const f of walk(docs).filter((f) => f.endsWith('.mdx'))) {
  let rel = relative(docs, f).replace(/\.mdx$/, '');
  rel = rel === 'index' ? '' : rel.replace(/\/index$/, '');
  pages.add('/docs' + (rel ? '/' + rel : ''));
}
for (const f of walk(docs).filter((f) => f.endsWith('.mdx'))) {
  const text = readFileSync(f, 'utf8');
  const fm = text.match(/^---\n([\s\S]*?)\n---/);
  if (!fm || !/^title:/m.test(fm[1]) || !/^description:/m.test(fm[1]))
    errors.push(`missing title or description: ${relative(site, f)}`);
  for (const m of text.matchAll(/\]\((\/docs[^)#\s]*)(#[^)]*)?\)|href="(\/docs[^"#]*)/g)) {
    const target = (m[1] ?? m[3]).replace(/\/$/, '');
    if (!pages.has(target)) errors.push(`broken link ${target} in ${relative(site, f)}`);
  }
}

const out = join(site, 'out');
if (existsSync(out)) {
  for (const need of ['llms.txt', 'llms-full.txt', 'SKILL.md', 'api/search', 'docs/index.html'])
    if (!existsSync(join(out, need))) errors.push(`missing build output: out/${need}`);
  for (const f of walk(out).filter((f) => /\.(html|txt|md|json)$/.test(f)))
    if (banned.test(readFileSync(f, 'utf8'))) errors.push(`banned term in out/${relative(out, f)}`);
}

if (errors.length) {
  console.error(errors.join('\n'));
  process.exit(1);
}
console.log(`content ok: ${pages.size} pages${existsSync(out) ? ', build output checked' : ''}`);
