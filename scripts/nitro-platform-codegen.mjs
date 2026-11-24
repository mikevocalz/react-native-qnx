#!/usr/bin/env node
// Experimental codegen adapter for Nitro v1's C++ HybridClass binding contract.
// Not upstream Nitrogen: Nitrogen currently exposes Apple and Android targets.
import assert from 'node:assert/strict';
import { readFile, writeFile } from 'node:fs/promises';
import { createRequire } from 'node:module';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const require = createRequire(import.meta.url);
// TS 7 exposes only its native CLI; the compiler AST API lives in this
// official side-by-side compatibility package.
const ts = require('@typescript/typescript6');
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const specPath = 'packages/qnx-nitro-runtime/specs/PlatformInfo.nitro.ts';
const windowsSpecPath = 'packages/qnx-nitro-runtime/specs/QnxWindows.nitro.ts';
const windowsTarget = {
  className: 'QnxWindows',
  output: 'packages/qnx-nitro-runtime/cpp/generated/QnxWindows.binding.cpp',
};
const targets = [
  {
    className: 'QnxPlatformInfo',
    output: 'packages/qnx-nitro-runtime/cpp/generated/QnxPlatformInfo.binding.cpp',
  },
  {
    className: 'MarathonPlatformInfo',
    output: 'native/platform/linux-host/nitro/generated/MarathonPlatformInfo.binding.cpp',
  },
];

/** @param {string} source */
export function parseSpec(source) {
  const file = ts.createSourceFile('PlatformInfo.nitro.ts', source, ts.ScriptTarget.Latest, true);
  const diagnostics = 'parseDiagnostics' in file ? file.parseDiagnostics : [];
  assert.equal(
    Array.isArray(diagnostics) ? diagnostics.length : 0,
    0,
    'Invalid TypeScript spec syntax'
  );
  const classes = file.statements.filter(ts.isClassDeclaration);
  assert.equal(classes.length, 1, 'Exactly one exported Nitro HybridObject class required');
  const cls = classes[0];
  assert.equal(cls.name?.text, 'PlatformInfo');
  // This adapter emits fixed public instance bindings. Valid TypeScript forms
  // such as static/optional/generic members must not silently change that API.
  const mods = cls.modifiers ?? [];
  assert.ok(mods.some((m) => m.kind === ts.SyntaxKind.ExportKeyword));
  assert.ok(mods.some((m) => m.kind === ts.SyntaxKind.DeclareKeyword));
  assert.ok(
    mods.every(
      (m) => m.kind === ts.SyntaxKind.ExportKeyword || m.kind === ts.SyntaxKind.DeclareKeyword
    ),
    'Only export declare class modifiers supported'
  );
  assert.equal(cls.typeParameters, undefined, 'No generic class support');
  assert.equal(cls.heritageClauses?.length, 1, 'Only one extends clause supported');
  const heritage = cls.heritageClauses?.[0];
  assert.equal(heritage?.token, ts.SyntaxKind.ExtendsKeyword, 'Class must extend HybridObject');
  assert.equal(heritage?.types.length, 1, 'Only the HybridObject base class supported');
  assert.equal(heritage?.types[0]?.expression.getText(file), 'HybridObject');
  assert.equal(heritage?.types[0]?.typeArguments, undefined, 'No generic base class support');
  const props = [];
  const methods = [];
  for (const m of cls.members) {
    if (m.name == null || !ts.isIdentifier(m.name)) {
      throw new Error('Computed/string-literal members not supported');
    }
    if (ts.isPropertyDeclaration(m)) {
      const modifiers = m.modifiers ?? [];
      assert.ok(
        modifiers.every(
          (mod) =>
            mod.kind === ts.SyntaxKind.ReadonlyKeyword || mod.kind === ts.SyntaxKind.PublicKeyword
        ),
        'Only public readonly instance properties supported'
      );
      assert.ok(modifiers.some((mod) => mod.kind === ts.SyntaxKind.ReadonlyKeyword));
      assert.equal(m.type?.kind, ts.SyntaxKind.StringKeyword, 'Only string getters supported');
      assert.equal(m.questionToken, undefined, 'No optional property support');
      assert.equal(m.exclamationToken, undefined, 'No definite-assignment property support');
      assert.equal(m.initializer, undefined, 'No property initializer support');
      props.push(m.name.text);
    } else if (ts.isMethodDeclaration(m)) {
      assert.ok(
        (m.modifiers ?? []).every((mod) => mod.kind === ts.SyntaxKind.PublicKeyword),
        'Only public instance methods supported'
      );
      assert.equal(m.questionToken, undefined, 'No optional method support');
      assert.equal(m.typeParameters, undefined, 'No generic method support');
      assert.equal(m.asteriskToken, undefined, 'No generator method support');
      assert.equal(m.body, undefined, 'Only method signatures supported');
      assert.equal(m.type?.kind, ts.SyntaxKind.NumberKeyword, 'Only numeric methods supported');
      assert.equal(m.parameters.length, 2, 'Only two-argument methods supported');
      for (const p of m.parameters) {
        assert.ok(
          ts.isIdentifier(p.name) && p.name.text !== 'this',
          'Only named value parameters supported'
        );
        assert.equal(p.modifiers?.length ?? 0, 0, 'No parameter modifier support');
        assert.equal(p.dotDotDotToken, undefined, 'No rest parameter support');
        assert.equal(p.type?.kind, ts.SyntaxKind.NumberKeyword);
        assert.equal(p.questionToken, undefined);
        assert.equal(p.initializer, undefined);
      }
      methods.push(m.name.text);
    } else {
      throw new Error('Unsupported Nitro spec member');
    }
  }
  assert.deepEqual(
    props,
    ['platform', 'architecture'],
    'Platform proof fields changed; review codegen'
  );
  assert.deepEqual(methods, ['add'], 'Platform proof methods changed; review codegen');
  return { props, methods };
}

/** @param {{props: string[], methods: string[]}} spec
 *  @param {{className: string, output: string}} target */
export function renderBinding(spec, target) {
  const { className } = target;
  const fieldLines = spec.props.map(
    (name) =>
      `        descriptor.registerHybridProperty("${name}", &${className}::get${name[0].toUpperCase()}${name.slice(1)});`
  );
  const methodLines = spec.methods.map(
    (name) => `        descriptor.registerHybridMethod("${name}", &${className}::${name});`
  );
  return [
    '// Generated by scripts/nitro-platform-codegen.mjs; do not modify by hand.',
    `#include "${className}.hpp"`,
    '',
    '#include <NitroModules/HybridClass.hpp>',
    '',
    'namespace rnqnx::nitro {',
    '',
    'const std::shared_ptr<const margelo::nitro::HybridClass>&',
    `${className}::Class::get() {`,
    '  static const auto cls = margelo::nitro::HybridClass::create<Class>(',
    '      [](margelo::nitro::HybridClass& descriptor) {',
    `        descriptor.registerHybridConstructor(&${className}::create);`,
    ...fieldLines,
    ...methodLines,
    '      });',
    '  return cls;',
    '}',
    '',
    '} // namespace rnqnx::nitro',
    '',
  ].join('\n');
}

// ---------------------------------------------------------------------------
// General spec path (type table). The PlatformInfo path above stays as it is;
// this one covers the windows API: number, boolean, string, string[], named
// structs, Promise<T> and callback parameters. Anything else is rejected.
// ---------------------------------------------------------------------------

const SCALARS = {
  number: { cpp: 'double' },
  boolean: { cpp: 'bool' },
  string: { cpp: 'std::string' },
};

/** @typedef {{kind: 'scalar', name: string} | {kind: 'strings'} | {kind: 'struct', name: string} | {kind: 'void'} | {kind: 'promise', of: any} | {kind: 'callback', params: any[]}} TypeRef */

/** @param {any} node @param {Set<string>} structs @param {any} file @param {{promise?: boolean, callback?: boolean, void?: boolean}} allow @returns {TypeRef} */
function resolveType(node, structs, file, allow) {
  assert.ok(node, 'Every member needs an explicit type');
  switch (node.kind) {
    case ts.SyntaxKind.NumberKeyword:
      return { kind: 'scalar', name: 'number' };
    case ts.SyntaxKind.BooleanKeyword:
      return { kind: 'scalar', name: 'boolean' };
    case ts.SyntaxKind.StringKeyword:
      return { kind: 'scalar', name: 'string' };
    case ts.SyntaxKind.VoidKeyword:
      assert.ok(allow.void, 'void is only allowed as a return type');
      return { kind: 'void' };
    case ts.SyntaxKind.ArrayType:
      assert.equal(
        node.elementType.kind,
        ts.SyntaxKind.StringKeyword,
        'Only string[] arrays supported'
      );
      return { kind: 'strings' };
    case ts.SyntaxKind.FunctionType: {
      assert.ok(allow.callback, 'Callbacks are only allowed as parameters');
      assert.equal(node.type.kind, ts.SyntaxKind.VoidKeyword, 'Callbacks must return void');
      assert.equal(node.typeParameters, undefined, 'No generic callbacks');
      const params = node.parameters.map((/** @type {any} */ p) => {
        assert.ok(ts.isIdentifier(p.name), 'Only named callback parameters');
        assert.equal(p.questionToken, undefined);
        assert.equal(p.dotDotDotToken, undefined);
        return resolveType(p.type, structs, file, {});
      });
      return { kind: 'callback', params };
    }
    case ts.SyntaxKind.TypeReference: {
      const name = node.typeName.getText(file);
      if (name === 'Promise') {
        assert.ok(allow.promise, 'Promise is only allowed as a return type');
        assert.equal(node.typeArguments?.length, 1, 'Promise needs one type argument');
        return {
          kind: 'promise',
          of: resolveType(node.typeArguments[0], structs, file, { void: true }),
        };
      }
      assert.equal(node.typeArguments, undefined, 'No generic type references');
      assert.ok(structs.has(name), `Unknown type ${name}`);
      return { kind: 'struct', name };
    }
    default:
      throw new Error(`Unsupported type: ${node.getText(file)}`);
  }
}

/** @param {TypeRef} type @returns {string} */
export function cppType(type) {
  switch (type.kind) {
    case 'scalar':
      return /** @type {Record<string, {cpp: string}>} */ (SCALARS)[type.name].cpp;
    case 'strings':
      return 'std::vector<std::string>';
    case 'struct':
      return type.name;
    case 'void':
      return 'void';
    case 'promise':
      return `margelo::nitro::Promise<${cppType(type.of)}>`;
    case 'callback':
      return `margelo::nitro::Callback<void(${type.params.map(cppType).join(', ')})>`;
  }
}

/** @param {string} source @param {string} expectedClass */
export function parseHybridSpec(source, expectedClass) {
  const file = ts.createSourceFile('spec.nitro.ts', source, ts.ScriptTarget.Latest, true);
  const diagnostics = 'parseDiagnostics' in file ? file.parseDiagnostics : [];
  assert.equal(
    Array.isArray(diagnostics) ? diagnostics.length : 0,
    0,
    'Invalid TypeScript spec syntax'
  );
  const interfaces = file.statements.filter(ts.isInterfaceDeclaration);
  const classes = file.statements.filter(ts.isClassDeclaration);
  for (const st of file.statements) {
    assert.ok(
      ts.isInterfaceDeclaration(st) || ts.isClassDeclaration(st) || ts.isImportDeclaration(st),
      'Only imports, interfaces and one class allowed'
    );
  }
  assert.equal(classes.length, 1, 'Exactly one Nitro HybridObject class required');
  const structNames = new Set(interfaces.map((i) => i.name.text));
  assert.equal(structNames.size, interfaces.length, 'Duplicate struct name');
  const structs = interfaces.map((decl) => {
    assert.equal(decl.typeParameters, undefined, 'No generic structs');
    assert.equal(decl.heritageClauses, undefined, 'No struct inheritance');
    const fields = decl.members.map((m) => {
      assert.ok(ts.isPropertySignature(m) && ts.isIdentifier(m.name), 'Only named struct fields');
      assert.equal(m.questionToken, undefined, 'No optional struct fields');
      const type = resolveType(m.type, structNames, file, {});
      assert.ok(type.kind === 'scalar', 'Struct fields are number, boolean or string');
      return { name: m.name.text, type };
    });
    assert.ok(fields.length > 0, 'Empty struct');
    return { name: decl.name.text, fields };
  });
  const cls = classes[0];
  assert.equal(cls.name?.text, expectedClass);
  const mods = cls.modifiers ?? [];
  assert.ok(mods.some((m) => m.kind === ts.SyntaxKind.ExportKeyword));
  assert.ok(mods.some((m) => m.kind === ts.SyntaxKind.DeclareKeyword));
  assert.ok(
    mods.every(
      (m) => m.kind === ts.SyntaxKind.ExportKeyword || m.kind === ts.SyntaxKind.DeclareKeyword
    ),
    'Only export declare class modifiers supported'
  );
  assert.equal(cls.typeParameters, undefined, 'No generic class support');
  assert.equal(cls.heritageClauses?.length, 1, 'Only one extends clause supported');
  const heritage = cls.heritageClauses[0];
  assert.equal(heritage.token, ts.SyntaxKind.ExtendsKeyword);
  assert.equal(heritage.types.length, 1);
  assert.equal(heritage.types[0].expression.getText(file), 'HybridObject');
  assert.equal(heritage.types[0].typeArguments, undefined);
  const props = [];
  const methods = [];
  const seen = new Set();
  for (const m of cls.members) {
    assert.ok(m.name && ts.isIdentifier(m.name), 'Computed/string-literal members not supported');
    assert.ok(!seen.has(m.name.text), `Duplicate member ${m.name.text}`);
    seen.add(m.name.text);
    if (ts.isPropertyDeclaration(m)) {
      const modifiers = m.modifiers ?? [];
      assert.ok(
        modifiers.every(
          (mod) =>
            mod.kind === ts.SyntaxKind.ReadonlyKeyword || mod.kind === ts.SyntaxKind.PublicKeyword
        ),
        'Only public readonly instance properties supported'
      );
      assert.ok(modifiers.some((mod) => mod.kind === ts.SyntaxKind.ReadonlyKeyword));
      assert.equal(m.questionToken, undefined, 'No optional property support');
      assert.equal(m.exclamationToken, undefined);
      assert.equal(m.initializer, undefined);
      const type = resolveType(m.type, structNames, file, {});
      assert.ok(
        type.kind === 'scalar' || type.kind === 'strings',
        'Property types: scalar or string[]'
      );
      props.push({ name: m.name.text, type });
    } else if (ts.isMethodDeclaration(m)) {
      assert.ok(
        (m.modifiers ?? []).every((mod) => mod.kind === ts.SyntaxKind.PublicKeyword),
        'Only public instance methods supported'
      );
      assert.equal(m.questionToken, undefined);
      assert.equal(m.typeParameters, undefined);
      assert.equal(m.asteriskToken, undefined);
      assert.equal(m.body, undefined, 'Only method signatures supported');
      const params = m.parameters.map((p) => {
        assert.ok(ts.isIdentifier(p.name) && p.name.text !== 'this', 'Only named value parameters');
        assert.equal(p.modifiers?.length ?? 0, 0);
        assert.equal(p.dotDotDotToken, undefined);
        assert.equal(p.questionToken, undefined, 'No optional parameters');
        assert.equal(p.initializer, undefined);
        const type = resolveType(p.type, structNames, file, { callback: true });
        assert.notEqual(type.kind, 'void');
        return { name: p.name.text, type };
      });
      const returns = resolveType(m.type, structNames, file, { promise: true, void: true });
      assert.notEqual(returns.kind, 'callback');
      methods.push({ name: m.name.text, params, returns });
    } else {
      throw new Error('Unsupported Nitro spec member');
    }
  }
  return { className: expectedClass, structs, props, methods };
}

const cap = (/** @type {string} */ name) => `${name[0].toUpperCase()}${name.slice(1)}`;

/** @param {ReturnType<typeof parseHybridSpec>} spec */
export function renderGeneralBinding(spec) {
  const { className } = spec;
  const lines = [
    '// Generated by scripts/nitro-platform-codegen.mjs; do not modify by hand.',
    `#include "${className}.hpp"`,
    '',
    '#include <NitroModules/HybridClass.hpp>',
    '#include <NitroModules/JSIConverter.hpp>',
    '',
    '#include <tuple>',
    '#include <type_traits>',
    '',
  ];
  if (spec.structs.length > 0) {
    lines.push('namespace margelo::nitro {', '');
    for (const st of spec.structs) {
      const qualified = `rnqnx::nitro::${st.name}`;
      lines.push(
        `// ${st.name} <> { ${st.fields.map((f) => f.name).join(', ')} }`,
        'template <>',
        `struct JSIConverter<${qualified}> final {`,
        `  static inline ${qualified} fromJSI(jsi::Runtime& runtime, const jsi::Value& arg) {`,
        '    jsi::Object object = arg.asObject(runtime);',
        `    return ${qualified}{`,
        ...st.fields.map(
          (f) =>
            `        .${f.name} = JSIConverter<${cppType(f.type)}>::fromJSI(runtime, object.getProperty(runtime, "${f.name}")),`
        ),
        '    };',
        '  }',
        `  static inline jsi::Value toJSI(jsi::Runtime& runtime, const ${qualified}& arg) {`,
        '    jsi::Object object(runtime);',
        ...st.fields.map(
          (f) =>
            `    object.setProperty(runtime, "${f.name}", JSIConverter<${cppType(f.type)}>::toJSI(runtime, arg.${f.name}));`
        ),
        '    return object;',
        '  }',
        '  static inline bool canConvert(jsi::Runtime& runtime, const jsi::Value& value) {',
        '    if (!value.isObject()) return false;',
        '    jsi::Object object = value.getObject(runtime);',
        '    return ' +
          st.fields
            .map(
              (f) =>
                `JSIConverter<${cppType(f.type)}>::canConvert(runtime, object.getProperty(runtime, "${f.name}"))`
            )
            .join('\n        && ') +
          ';',
        '  }',
        '};',
        ''
      );
    }
    lines.push('} // namespace margelo::nitro', '');
  }
  lines.push(
    'namespace rnqnx::nitro {',
    '',
    'namespace {',
    '// The spec is the source of truth: a native signature that drifts from it',
    '// fails to compile here instead of misbehaving at the JS boundary.',
    'template <typename Member>',
    'struct MemberSignature;',
    'template <typename R, typename C, typename... A>',
    'struct MemberSignature<R (C::*)(A...)> {',
    '  using Return = R;',
    '  using Args = std::tuple<std::decay_t<A>...>;',
    '};',
    ''
  );
  for (const p of spec.props) {
    lines.push(
      `static_assert(std::is_same_v<MemberSignature<decltype(&${className}::get${cap(p.name)})>::Return, ${cppType(p.type)}>, "${p.name} drifted from the spec");`
    );
  }
  for (const m of spec.methods) {
    lines.push(
      `static_assert(std::is_same_v<MemberSignature<decltype(&${className}::${m.name})>::Return, ${cppType(m.returns)}>, "${m.name} return type drifted from the spec");`,
      `static_assert(std::is_same_v<MemberSignature<decltype(&${className}::${m.name})>::Args, std::tuple<${m.params.map((p) => cppType(p.type)).join(', ')}>>, "${m.name} parameters drifted from the spec");`
    );
  }
  lines.push(
    '} // namespace',
    '',
    'const std::shared_ptr<const margelo::nitro::HybridClass>&',
    `${className}::Class::get() {`,
    '  static const auto cls = margelo::nitro::HybridClass::create<Class>(',
    '      [](margelo::nitro::HybridClass& descriptor) {',
    `        descriptor.registerHybridConstructor(&${className}::create);`,
    ...spec.props.map(
      (p) =>
        `        descriptor.registerHybridProperty("${p.name}", &${className}::get${cap(p.name)});`
    ),
    ...spec.methods.map(
      (m) => `        descriptor.registerHybridMethod("${m.name}", &${className}::${m.name});`
    ),
    '      });',
    '  return cls;',
    '}',
    '',
    '} // namespace rnqnx::nitro',
    ''
  );
  return lines.join('\n');
}

/** @param {'--write' | '--check'} mode */
export async function generate(mode) {
  const spec = parseSpec(await readFile(path.join(root, specPath), 'utf8'));
  for (const target of targets) {
    const output = path.join(root, target.output);
    const expected = renderBinding(spec, target);
    if (mode === '--write') {
      await writeFile(output, expected, 'utf8');
    } else {
      const actual = await readFile(output, 'utf8');
      assert.equal(actual, expected, `Out-of-date generated Nitro binding: ${target.output}`);
    }
  }
  const windows = parseHybridSpec(
    await readFile(path.join(root, windowsSpecPath), 'utf8'),
    windowsTarget.className
  );
  const windowsOutput = path.join(root, windowsTarget.output);
  const windowsExpected = renderGeneralBinding(windows);
  if (mode === '--write') {
    await writeFile(windowsOutput, windowsExpected, 'utf8');
  } else {
    assert.equal(
      await readFile(windowsOutput, 'utf8'),
      windowsExpected,
      `Out-of-date generated Nitro binding: ${windowsTarget.output}`
    );
  }
  console.log(`Platform Nitro v1 binding ${mode === '--check' ? 'check' : 'generation'} passed`);
}

if (import.meta.main) {
  const mode = process.argv[2];
  if (mode !== '--write' && mode !== '--check') {
    throw new Error('Usage: node scripts/nitro-platform-codegen.mjs --check|--write');
  }
  await generate(mode);
}
