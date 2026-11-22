import assert from 'node:assert/strict';
import { existsSync, readdirSync, readFileSync } from 'node:fs';
import path from 'node:path';

// docs/releases/release-units.json must name files and packages that exist, and
// must not claim a unit is published or release-eligible while its package
// manifest says otherwise.
const root = process.cwd();
const read = (file) => JSON.parse(readFileSync(path.join(root, file), 'utf8'));
const units = read('docs/releases/release-units.json');
assert.equal(units.schema, 1);

const ids = units.units.map((unit) => unit.id);
assert.equal(new Set(ids).size, ids.length, 'unit ids are unique');

const packages = new Set(
  readdirSync(path.join(root, 'packages')).filter((name) =>
    existsSync(path.join(root, 'packages', name, 'package.json'))
  )
);

for (const unit of units.units) {
  assert.equal(unit.published, false, `${unit.id} is published`);
  for (const key of ['packageManifest', 'closureCheck', 'assembledBy', 'offer', 'buildRecipe']) {
    if (unit[key]) assert.ok(existsSync(path.join(root, unit[key])), `${unit.id}.${key} missing`);
  }
  for (const required of unit.requires ?? []) {
    assert.ok(ids.includes(required), `${unit.id} requires unknown unit ${required}`);
  }
  for (const member of unit.members ?? []) {
    assert.ok(packages.has(member), `${unit.id} member ${member} is not under packages/`);
  }
  for (const member of Object.keys(unit.excluded ?? {})) {
    assert.ok(packages.has(member), `${unit.id} excluded ${member} is not under packages/`);
  }
  if (unit.members) {
    const covered = new Set([...unit.members, ...Object.keys(unit.excluded ?? {})]);
    assert.deepEqual(
      [...packages].sort(),
      [...covered].sort(),
      'every package is a member or excluded'
    );
  }
  if (unit.packageManifest) {
    const manifest = read(unit.packageManifest);
    assert.equal(manifest.unit, unit.id);
    assert.equal(manifest.releaseEligible, unit.releaseEligible);
    assert.equal(Object.keys(manifest.files).length, manifest.counts.hashedFiles);
    if (!manifest.releaseEligible) assert.ok(manifest.notEligibleBecause.length > 0);
  }
}
console.log('release-units: ok');
