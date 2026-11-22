// Fresh `npm pack --dry-run --json` for every packages/* directory that has a package.json.
// Flags paths that look like SDK payloads, keys, env files, archives or logs.
import { execFileSync } from 'node:child_process';
import { existsSync, readFileSync, readdirSync } from 'node:fs';
import { join } from 'node:path';

const root = process.argv[2];
const flag =
  /(\.env|\.pem|\.key$|id_rsa|id_ed25519|qnxsdp|\.tar|\.zip|\.tgz|\.qnx$|\.core$|node_modules|\.DS_Store|\.log$)/i;
for (const d of readdirSync(join(root, 'packages')).sort()) {
  const dir = join(root, 'packages', d);
  const pj = join(dir, 'package.json');
  if (!existsSync(pj)) {
    console.log(`${d}|no package.json (not a package)`);
    continue;
  }
  const meta = JSON.parse(readFileSync(pj, 'utf8'));
  let out;
  try {
    out = execFileSync('npm', ['pack', '--dry-run', '--json', '--ignore-scripts'], {
      cwd: dir,
      encoding: 'utf8',
      stdio: ['ignore', 'pipe', 'pipe'],
    });
  } catch (e) {
    console.log(
      `${meta.name}|${meta.private ? 'private' : 'public'}|npm pack failed: ${String(e.stderr).split('\n')[0]}`
    );
    continue;
  }
  const parsed = JSON.parse(out);
  const j = Array.isArray(parsed) ? parsed[0] : Object.values(parsed)[0];
  const bad = j.files.map((f) => f.path).filter((p) => flag.test(p));
  console.log(
    [
      meta.name,
      meta.private ? 'private' : 'public',
      j.version,
      `${j.entryCount} files`,
      `${j.size} packed`,
      `${j.unpackedSize} unpacked`,
      j.shasum,
      `flagged=${JSON.stringify(bad)}`,
    ].join('|')
  );
}
