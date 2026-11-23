import { spawn } from 'node:child_process';
import { access, mkdir, readFile, stat } from 'node:fs/promises';
import { createHash } from 'node:crypto';
import { createRequire } from 'node:module';
import path from 'node:path';

/** Candidate hermesc paths inside the hermes-compiler package, by host platform. */
const HERMESC = {
  darwin: ['hermesc/osx-bin/hermesc'],
  linux: ['hermesc/linux64-bin/hermesc'],
};

/** @param {string} root @param {string | undefined} explicit */
async function resolveHermesc(root, explicit) {
  const fromEnv = explicit ?? process.env.RNQNX_HOST_HERMESC;
  if (fromEnv) {
    await access(fromEnv);
    return path.resolve(fromEnv);
  }
  const require = createRequire(path.join(root, 'package.json'));
  let packageRoot;
  try {
    packageRoot = path.dirname(require.resolve('hermes-compiler/package.json'));
  } catch {
    try {
      // pnpm keeps hermes-compiler as a dependency of react-native, not of the root.
      const fromReactNative = createRequire(require.resolve('react-native/package.json'));
      packageRoot = path.dirname(fromReactNative.resolve('hermes-compiler/package.json'));
    } catch {
      throw new Error(
        'hermes-compiler is not installed. Run pnpm install --frozen-lockfile, or pass --hermesc <path> or set RNQNX_HOST_HERMESC.'
      );
    }
  }
  for (const relative of HERMESC[/** @type {'darwin' | 'linux'} */ (process.platform)] ?? []) {
    const candidate = path.join(packageRoot, relative);
    try {
      await access(candidate);
      return candidate;
    } catch {
      // try the next candidate
    }
  }
  throw new Error(
    `No hermesc for ${process.platform} in hermes-compiler. Pass --hermesc <path> or set RNQNX_HOST_HERMESC.`
  );
}

/** @param {string} executable @param {string[]} args @param {string} cwd */
function execute(executable, args, cwd) {
  return new Promise((resolve, reject) => {
    const child = spawn(executable, args, { cwd, stdio: 'inherit' });
    child.on('error', reject);
    child.on('close', (code) => resolve(code ?? 1));
  });
}

/** @param {string} file */
async function sha256(file) {
  return createHash('sha256')
    .update(await readFile(file))
    .digest('hex');
}

/**
 * pnpm qnx deploy-bundle --app <name>: hermesc bytecode compile, then push over ssh.
 * Replaces the manual hermesc and scp steps of the 2026-11-21 developer-path receipt.
 * @param {string} root
 * @param {{app?: string, bundle?: string, hermesc?: string, 'remote-dir'?: string, runner?: string, run?: boolean, 'dry-run'?: boolean}} values
 * @param {string} cwd
 */
export async function deployBundle(root, values, cwd) {
  const name = values.app;
  if (!name || !/^[a-z][a-z0-9-]{0,62}$/.test(name))
    throw new Error('deploy-bundle needs --app <name> (lowercase letters, digits, hyphens).');
  if (values.runner && !values.run) throw new Error('--runner only applies together with --run.');
  const buildDir = path.join(root, 'build');
  const bundle = path.resolve(root, values.bundle ?? `build/${name}.bundle.js`);
  if (!bundle.startsWith(buildDir + path.sep))
    throw new Error('The bundle must be inside this checkout’s build/ directory.');
  const bytecode = path.join(buildDir, `${name}.hbc`);
  const remoteName = `${name}.hbc`;
  const runner = values.run ? (values.runner ?? 'rnqnx_playground_qnx') : undefined;
  const push = [
    path.join(root, 'scripts', 'deploy-bundle.sh'),
    bytecode,
    remoteName,
    ...(runner ? ['--run', runner] : []),
  ];
  const env = { ...process.env };
  if (values['remote-dir']) env.QNX_BUNDLE_DIR = values['remote-dir'];
  if (values['dry-run']) {
    const hermesc = await resolveHermesc(root, values.hermesc).catch(
      (/** @type {Error} */ error) => `unresolved: ${error.message}`
    );
    console.log(
      JSON.stringify(
        {
          steps: [
            {
              step: 'hermesc',
              executable: hermesc,
              args: ['-O', '-emit-binary', '-out', bytecode, bundle],
            },
            { step: 'push', executable: 'bash', args: push },
          ],
          cwd,
        },
        null,
        2
      )
    );
    return 0;
  }
  try {
    if (!(await stat(bundle)).isFile()) throw new Error('not a file');
  } catch {
    throw new Error(
      `${path.relative(root, bundle)} not found. Run pnpm qnx bundle-qnx --app ${name} first.`
    );
  }
  const hermesc = await resolveHermesc(root, values.hermesc);
  await mkdir(buildDir, { recursive: true });
  const compiled = await execute(hermesc, ['-O', '-emit-binary', '-out', bytecode, bundle], root);
  if (compiled !== 0) {
    console.error(`rnqnx: hermesc exited with code ${compiled}. Read its error above.`);
    return compiled;
  }
  const size = (await stat(bytecode)).size;
  if (size === 0) throw new Error('hermesc wrote an empty bytecode file.');
  console.log(
    `Compiled ${path.relative(root, bytecode)}: ${size} bytes, sha256 ${await sha256(bytecode)}`
  );
  const pushed = await execute('bash', push, root);
  if (pushed !== 0)
    console.error(
      `rnqnx: deploy-bundle exited with code ${pushed}. Check that QNX_TARGET_IP and QNX_TARGET_USER reach the board over SSH and that the runner is installed.`
    );
  return pushed;
}
