import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { parseWindowSpec, renderWindowBindings } from '../../scripts/window-api-codegen.mjs';
const types = readFileSync('packages/qnx-nitro-runtime/specs/window/WindowSnapshot.ts', 'utf8');
const spec = readFileSync('packages/qnx-nitro-runtime/specs/window/Window.nitro.ts', 'utf8');
const generated = readFileSync(
  'packages/qnx-nitro-runtime/cpp/generated/WindowBindings.hpp',
  'utf8'
);
assert.equal(renderWindowBindings(parseWindowSpec(types, spec)), generated);
for (const changed of [
  spec.replace('readonly snapshot:', 'snapshot:'),
  spec.replace('setTitle(title: string)', 'setTitle(title?: string)'),
  spec.replace('setTitle(title: string)', 'static setTitle(title: string)'),
  spec.replace('setTitle(title: string)', 'setTitle<T>(title: string)'),
  spec.replace('setTitle(title: string)', 'setTitle(...title: string[])'),
  spec.replace('setTitle(title: string)', "setTitle(title: string = 'x')"),
  spec.replace('setTitle(title: string)', "setTitle(title: 'fixed')"),
  spec.replace('extends HybridObject', 'extends Other'),
  spec.replace('declare class Window extends', 'declare class Window<T> extends'),
  spec.replace('remove(): void;', 'remove(): number;'),
  spec.replace('dispose(): void;', 'dispose(): void; arbitrary(): void;'),
])
  assert.throws(() => parseWindowSpec(types, changed));
assert.throws(() => parseWindowSpec(types.replace('readonly top:', 'top:'), spec));
assert.throws(() =>
  parseWindowSpec(types.replace('readonly top: number;', 'readonly top?: number;'), spec)
);
assert.match(generated, /HybridClass::create<Class>/);
assert.match(generated, /Promise<void>/);
assert.match(generated, /Callback<void\(WindowSnapshot\)>/);
// Resource handles have no process-global constructor selector; the per-runtime
// host returns them. Base HybridObject owns its final disposal entry point.
assert.doesNotMatch(generated, /registerHybridConstructor|registerHybridMethod\("dispose"/);
console.log(
  'window-api-codegen: deterministic exact-contract generation and unsupported-shape controls pass'
);
