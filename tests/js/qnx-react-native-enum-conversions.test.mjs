import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { mkdtemp, readFile, rm, writeFile } from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';

const view = new URL(
  '../../node_modules/react-native/ReactCommon/react/renderer/components/view/',
  import.meta.url
);
const conversions = await readFile(new URL('accessibilityPropsConversions.h', view), 'utf8');
const primitives = await readFile(new URL('AccessibilityPrimitives.h', view), 'utf8');

/** @param {string} source @param {string} marker */
function block(source, marker) {
  const start = source.indexOf(marker);
  assert.notEqual(start, -1, 'Missing pinned React Native declaration: ' + marker);
  const open = source.indexOf('{', start);
  let depth = 0;
  for (let index = open; index < source.length; index++) {
    if (source[index] === '{') depth++;
    if (source[index] === '}' && --depth === 0) return source.slice(start, index + 1);
  }
  throw new Error('Unterminated pinned declaration: ' + marker);
}

const functions = [
  'inline std::string toString(const ImportantForAccessibility &importantForAccessibility)',
  'inline std::string toString(AccessibilityLiveRegion accessibilityLiveRegion)',
  'inline std::string toString(AccessibilityState::CheckedState state)',
].map((signature) => block(conversions, signature));
assert.ok(conversions.includes('\n#include <cstdlib>\n'), 'Missing standard abort declaration.');
for (const source of functions) assert.match(source, /\n  std::abort\(\);\n\}$/);

const compiler = spawnSync('c++', ['--version'], { encoding: 'utf8', timeout: 15000 });
const missingCompiler =
  compiler.error && 'code' in compiler.error && compiler.error.code === 'ENOENT';
if (process.platform !== 'linux' || missingCompiler) {
  console.log('Installed enum fallbacks verified; host C++ execution not run on this host.');
} else {
  assert.equal(compiler.status, 0, compiler.stderr);
  const root = await mkdtemp(path.join(os.tmpdir(), 'rnqnx-enum-conversions-'));
  try {
    const source = path.join(root, 'conversions.cpp');
    const executable = path.join(root, 'conversions');
    await writeFile(
      source,
      `#include <cstdint>
#include <cstdlib>
#include <string>
namespace facebook::react {
${block(primitives, 'enum class ImportantForAccessibility')};
${block(primitives, 'enum class AccessibilityLiveRegion')};
struct AccessibilityState { ${block(primitives, 'enum CheckedState')}; };
${functions.join('\n')}
}
int main(int argc, char** argv) {
  using namespace facebook::react;
  if (argc == 2) {
    if (std::string(argv[1]) == "important") toString(static_cast<ImportantForAccessibility>(255));
    else if (std::string(argv[1]) == "live") toString(static_cast<AccessibilityLiveRegion>(255));
    return 99;
  }
  if (toString(ImportantForAccessibility::Auto) != "auto" ||
      toString(ImportantForAccessibility::Yes) != "yes" ||
      toString(ImportantForAccessibility::No) != "no" ||
      toString(ImportantForAccessibility::NoHideDescendants) != "no-hide-descendants" ||
      toString(AccessibilityLiveRegion::None) != "none" ||
      toString(AccessibilityLiveRegion::Polite) != "polite" ||
      toString(AccessibilityLiveRegion::Assertive) != "assertive" ||
      toString(AccessibilityState::Unchecked) != "Unchecked" ||
      toString(AccessibilityState::Checked) != "Checked" ||
      toString(AccessibilityState::Mixed) != "Mixed" ||
      toString(AccessibilityState::None) != "None") return 1;
  return 0;
}
`
    );
    const build = spawnSync(
      'c++',
      ['-std=c++20', '-O0', '-Wall', '-Wextra', '-Werror', source, '-o', executable],
      {
        encoding: 'utf8',
        timeout: 30000,
      }
    );
    assert.equal(build.status, 0, build.stderr + String(build.error ?? ''));
    /** @param {string[]} args */
    const run = (args) =>
      spawnSync('sh', ['-c', 'ulimit -c 0; exec "$@"', 'sh', executable, ...args], {
        encoding: 'utf8',
        timeout: 15000,
      });
    assert.equal(run([]).status, 0, 'Valid enum conversions must retain their strings.');
    for (const name of ['important', 'live']) {
      assert.equal(
        run([name]).signal,
        'SIGABRT',
        'Invalid fixed-underlying enum must abort: ' + name
      );
    }
    console.log(
      'React Native enum conversions: 11 valid values and 2 invalid-value abort paths passed (host snippets only; no device claim).'
    );
  } finally {
    await rm(root, { recursive: true, force: true });
  }
}
