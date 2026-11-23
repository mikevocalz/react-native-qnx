import { mkdir, writeFile } from 'node:fs/promises';
import path from 'node:path';
import { loadRegistry, packTemplate, packVerified, receipt, writeLock } from './templates.mjs';

/**
 * pnpm qnx templates list | pack --out <dir> | lock
 * @param {string} root @param {string[]} rest @param {{out?: string, 'dry-run'?: boolean}} values @param {string} cwd
 */
export async function templatesCommand(root, rest, values, cwd) {
  const [action, ...extra] = rest;
  if (extra.length || !['list', 'pack', 'lock'].includes(action ?? ''))
    throw new Error('Use pnpm qnx templates list | pack --out <dir> | lock.');
  const registry = await loadRegistry(root);
  if (action === 'lock') {
    if (values['dry-run']) {
      for (const name of Object.keys(registry)) {
        const packed = await packTemplate(name, root);
        console.log(`${name} ${packed.version} ${packed.contentSha256}`);
      }
      return 0;
    }
    const written = await writeLock(root);
    console.log(JSON.stringify(written, null, 2));
    return 0;
  }
  const names = Object.keys(registry).sort();
  /** @type {ReturnType<typeof receipt>[]} */
  const rows = [];
  for (const name of names) {
    const packed = await packVerified(name, root);
    rows.push(receipt(packed));
    if (action === 'pack') {
      if (!values.out?.trim()) throw new Error('templates pack needs --out <dir>.');
      if (!values['dry-run']) {
        const dir = path.resolve(cwd, values.out);
        await mkdir(dir, { recursive: true });
        await writeFile(path.join(dir, `${name}-${packed.version}.tgz`), packed.tarball);
      }
    }
  }
  const manifest = {
    schemaVersion: 1,
    note: 'contentSha256 is the SHA-256 of the uncompressed tar and is pinned in template/templates.lock.json. tarballSha256 can differ between zlib builds.',
    templates: rows.map((row) => ({
      ...row,
      commands: registry[row.template].commands,
      file: `${row.template}-${row.version}.tgz`,
    })),
  };
  if (action === 'pack' && !values['dry-run'])
    await writeFile(
      path.join(path.resolve(cwd, /** @type {string} */ (values.out)), 'templates-manifest.json'),
      JSON.stringify(manifest, null, 2) + '\n'
    );
  console.log(JSON.stringify(manifest, null, 2));
  return 0;
}
