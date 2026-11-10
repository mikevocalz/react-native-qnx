import { spawn } from 'node:child_process';
import { randomBytes } from 'node:crypto';
import path from 'node:path';
import { parseArgs } from 'node:util';
import {
  launcherConfiguration,
  launcherIdentity,
  launcherFiles,
  prepareLauncher,
  shellQuote as q,
  targetPath,
  LAUNCHER_GENERATOR,
} from './launcher.mjs';
import { validateAppIdentity } from './prepare-native.mjs';

const HELP = `QNX application launcher integration
  project launcher prepare --project <directory> [--dry-run]
  project launcher install --project <directory> --app-dir <deployed-target-directory>
                           [--executable <filename>] [--no-refresh] [--dry-run]
  project launcher remove  --project <directory> [--no-refresh] [--dry-run]

Reads static app.json: expo.name, expo.icon and expo.extra.rnqnx application identity.
An optional QNX icon/runtime profile is under expo.extra.rnqnx.qnx.
Registers an ALREADY DEPLOYED app, not an executable/SDK installer or boot service.
Uses a private user-owned overlay of the stock QNX demo launcher, preserving OS entries.
Install updates the same app ID; remove unregisters only its launcher files, not app data.
Requires the OS window manager and stock demolauncher; never starts privileged services.
Device commands require QNX_TARGET_IP/USER and QNX_SSH_KEY; QNX_SSH_PORT defaults to 22.
Verified known-host key, strict batch authentication and non-root runtime are mandatory.
`;

/** @param {NodeJS.ProcessEnv} env */
export function launcherTarget(env) {
  const host = env.QNX_TARGET_IP ?? '';
  const user = env.QNX_TARGET_USER ?? 'qnxuser';
  const key = env.QNX_SSH_KEY ?? '';
  const port = env.QNX_SSH_PORT ?? '22';
  if (
    !/^[A-Za-z0-9][A-Za-z0-9.-]*$/.test(host) ||
    !/^[A-Za-z_][A-Za-z0-9_-]*$/.test(user) ||
    user === 'root'
  )
    throw new Error('Select a valid QNX_TARGET_IP and non-root QNX_TARGET_USER.');
  if (!path.isAbsolute(key) || /[\x00-\x1f]/.test(key))
    throw new Error('QNX_SSH_KEY must name an absolute dedicated key path.');
  if (!/^[1-9][0-9]{0,4}$/.test(port) || Number(port) > 65535) throw new Error('Invalid SSH port.');
  return {
    address: `${user}@${host}`,
    options: [
      '-F',
      '/dev/null',
      '-i',
      key,
      '-o',
      'IdentityAgent=none',
      '-o',
      'IdentitiesOnly=yes',
      '-o',
      'BatchMode=yes',
      '-o',
      'StrictHostKeyChecking=yes',
      '-o',
      'ConnectTimeout=10',
      '-o',
      'MACs=hmac-sha2-256',
      '-o',
      'ProxyCommand=none',
      '-o',
      'PermitLocalCommand=no',
    ],
    port,
  };
}

/** @param {string} program @param {string[]} args @param {string|Buffer} [input] @param {number} [timeoutMs] */
export async function runLauncherTool(program, args, input = '', timeoutMs = 45000) {
  if (!Number.isInteger(timeoutMs) || timeoutMs < 100 || timeoutMs > 45000)
    throw new Error('Invalid device command deadline.');
  if (!['linux', 'darwin'].includes(process.platform))
    throw new Error('Launcher deployment currently requires Linux or macOS OpenSSH.');
  return new Promise((resolve, reject) => {
    const child = spawn(program, args, {
      detached: true,
      shell: false,
      stdio: ['pipe', 'pipe', 'pipe'],
    });
    let output = '';
    let errorOutput = '';
    let settled = false;
    /** @type {Error | undefined} */ let failure;
    /** @type {NodeJS.Timeout | undefined} */ let force;
    /** @param {NodeJS.Signals} signal */
    const kill = (signal) => {
      try {
        if (child.pid) process.kill(-child.pid, signal);
      } catch {}
    };
    /** @param {Error} [error] */
    const finish = (error) => {
      if (settled) return;
      settled = true;
      kill('SIGKILL');
      clearTimeout(timer);
      clearTimeout(force);
      process.off('SIGINT', interrupt);
      process.off('SIGTERM', terminate);
      child.stdin.destroy();
      child.stdout.destroy();
      child.stderr.destroy();
      child.unref();
      if (failure || error) reject(failure ?? error);
      else resolve(output);
    };
    /** @param {Error} error */
    const fail = (error) => {
      if (failure || settled) return;
      failure = error;
      kill('SIGTERM');
      // Keep this timer referenced until the entire owned process group is
      // killed, even if the immediate child exits and closes its pipes first.
      force = setTimeout(() => finish(failure), 1000);
    };
    const interrupt = () => fail(new Error('Launcher command interrupted.'));
    const terminate = () => fail(new Error('Launcher command terminated.'));
    const timer = setTimeout(
      () => fail(new Error('Launcher device command exceeded its deadline.')),
      timeoutMs
    );
    process.on('SIGINT', interrupt);
    process.on('SIGTERM', terminate);
    child.stdin.on('error', () => {});
    child.stdout.on('data', (chunk) => {
      if (failure || settled) return;
      output += chunk.toString();
      if (output.length > 1024 * 1024) fail(new Error('Device output exceeded its bound.'));
    });
    child.stderr.on('data', (chunk) => {
      if (failure || settled) return;
      errorOutput += chunk.toString();
      if (errorOutput.length > 1024 * 1024) fail(new Error('Device errors exceeded their bound.'));
    });
    child.once('error', (error) => finish(error));
    child.once('close', (code) => {
      finish(
        code === 0 ? undefined : new Error(`${program} failed (${code}): ${errorOutput.trim()}`)
      );
    });
    child.stdin.end(input);
  });
}

/** @param {string} file @param {string} text */
function writeText(file, text) {
  return `safe_file ${q(file)}\ncat >${q(file)} <<'RNQNX_GENERATED_FILE'\n${text}RNQNX_GENERATED_FILE\n`;
}

/** @param {string} registry @param {string} home @param {string} [stockDirectory] */
function registryPrelude(registry, home, stockDirectory = '/system/etc/desktop_files') {
  targetPath(home, 'target home');
  targetPath(stockDirectory, 'stock launcher directory');
  targetPath(registry, 'registry');
  if (!registry.startsWith(home + '/'))
    throw new Error('Registry must be below the non-root home directory.');
  return `set -eu
test "$(id -u)" -ne 0 || { echo 'Root launcher installation is forbidden' >&2; exit 1; }
test "$HOME" = ${q(home)} || exit 1
umask 077
safe_file() {
  test ! -L "$1" && { test ! -e "$1" || test -f "$1"; } || { echo 'Unsafe launcher file' >&2; exit 1; }
}
hash_file() {
  if command -v sha256sum >/dev/null 2>&1; then sha256sum "$1" | awk '{print $1}';
  elif command -v openssl >/dev/null 2>&1; then openssl dgst -sha256 "$1" | awk '{print $NF}';
  else echo 'A target SHA-256 implementation is required' >&2; return 1; fi
}
safe_directory() {
  current=
  old_ifs=$IFS; IFS=/
  for part in $1; do
    test -n "$part" || continue
    current="$current/$part"
    test ! -L "$current" || { echo 'Symlink in launcher path' >&2; exit 1; }
    test -d "$current" || mkdir "$current"
  done
  IFS=$old_ifs
}
safe_directory ${q(registry)}
if test -f ${q(registry + '/owner')}; then
  test ! -L ${q(registry + '/owner')} && test "$(cat ${q(registry + '/owner')})" = ${q(LAUNCHER_GENERATOR)} || exit 1
else
  test -z "$(ls -A ${q(registry)})" || { echo 'Refusing to adopt unowned launcher directory' >&2; exit 1; }
  printf '%s\\n' ${q(LAUNCHER_GENERATOR)} >${q(registry + '/owner')}
fi
mkdir ${q(registry + '/lock')} || { echo 'Another launcher operation is active' >&2; exit 1; }
trap 'rmdir ${registry}/lock' EXIT
trap 'exit 130' HUP INT TERM
safe_directory ${q(registry + '/apps')}
safe_file ${q(registry + '/base.conf')}
safe_file ${q(registry + '/desktop.conf')}
safe_file ${q(registry + '/desktop.conf.new')}
if test ! -s ${q(registry + '/base.conf')}; then
  for desktop in ${q(stockDirectory)}/*.desktop; do
    name="\${desktop##*/}"
    safe_file "${registry}/$name"
    if test -f "${registry}/$name"; then
      if grep -Fx ${q('X-RNQNX-Generated-By=' + LAUNCHER_GENERATOR)} "${registry}/$name" >/dev/null; then
        echo 'Stock snapshot collides with a registered application' >&2; exit 1
      fi
    fi
    safe_file "${registry}/$name.new"
    cp "$desktop" "${registry}/$name.new"
    mv "${registry}/$name.new" "${registry}/$name"
  done
  safe_file ${q(registry + '/base.conf.new')}
  cp ${q(stockDirectory + '/desktop.conf')} ${q(registry + '/base.conf.new')}
  test -s ${q(registry + '/base.conf.new')} || exit 1
  mv ${q(registry + '/base.conf.new')} ${q(registry + '/base.conf')}
fi
`;
}

/** @param {string} registry @param {string[]} libraryPaths @param {boolean} refresh */
function rebuildRegistry(registry, libraryPaths, refresh) {
  return `safe_file ${q(registry + '/desktop.conf.new')}
cat ${q(registry + '/base.conf')} >${q(registry + '/desktop.conf.new')}
order=$(awk -F= '/^Order=[0-9]+$/ {if ($2 > maximum) maximum=$2} END {print maximum+0}' ${q(registry + '/base.conf')})
for fragment in ${q(registry + '/apps')}/*/entry.conf; do
  test -f "$fragment" || continue
  test ! -L "$fragment" || exit 1
  order=$((order+1))
  awk -v order="$order" '/^Order=/ {print "Order=" order; next} {print}' "$fragment" >>${q(registry + '/desktop.conf.new')}
done
mv ${q(registry + '/desktop.conf.new')} ${q(registry + '/desktop.conf')}
${
  refresh
    ? `command -v on >/dev/null; command -v waitfor >/dev/null
safe_file ${q(registry + '/new.pid')}
safe_file ${q(registry + '/launcher.pid')}
safe_file ${q(registry + '/launcher.log')}
rm -f ${q(registry + '/new.pid')}
on -d /bin/sh -c ${q(`echo "$$" >${registry}/new.pid; exec env LD_LIBRARY_PATH=${libraryPaths.join(':') || '/usr/lib:/lib'} /system/bin/demolauncher -a ${registry}`)} </dev/null >${q(registry + '/launcher.log')} 2>&1
waitfor ${registry}/new.pid 5
pid=$(cat ${registry}/new.pid)
case "$pid" in ''|*[!0-9]*) exit 1;; esac
sleep 1
kill -0 "$pid"
pidin ar | awk -v pid="$pid" '$1 == pid && $2 == "/system/bin/demolauncher" && $3 == "-a" && $4 == "${registry}" && NF == 4 {found=1} END {exit !found}' || { echo 'Launcher startup failed; previous launcher retained' >&2; exit 1; }
for previous in $(pidin ar | awk '$2 == "/system/bin/demolauncher" && $3 == "-a" && NF == 4 && ($4 == "${registry}" || $4 == "/system/etc/desktop_files") {print $1}'); do
  test "$previous" = "$pid" && continue
  kill -0 "$previous" 2>/dev/null || continue
  kill -TERM "$previous"
done
mv ${registry}/new.pid ${registry}/launcher.pid
`
    : ''
}printf 'RNQNX_LAUNCHER_OK\\n'
`;
}

/** @param {Awaited<ReturnType<typeof launcherConfiguration>>['configuration']} config
 * @param {string} home @param {string} appDirectory @param {string} executable @param {boolean} refresh
 * @param {string} [stockDirectory] @param {string} [upload] */
export function installLauncherScript(
  config,
  home,
  appDirectory,
  executable,
  refresh,
  stockDirectory,
  upload = 'incoming-' + config.appId + '.png'
) {
  if (!/^[A-Za-z0-9_.-]+$/.test(upload)) throw new Error('Invalid icon upload filename.');
  if (!/^[a-f0-9]{64}$/.test(config.icon.sha256)) throw new Error('Invalid icon fingerprint.');
  const registry = home + '/.local/share/rnqnx/launcher';
  if (!appDirectory.startsWith(home + '/'))
    throw new Error('App must be deployed under its non-root owner home.');
  const files = launcherFiles(config, appDirectory, registry, executable);
  const entry = registry + '/apps/' + config.appId;
  let script = registryPrelude(registry, home, stockDirectory);
  script += `test -d ${q(appDirectory)} && test ! -L ${q(appDirectory)} && test -x ${q(appDirectory + '/' + executable)} && test ! -L ${q(appDirectory + '/' + executable)} || exit 1
for directory in ${config.runtime.libraryPaths.map(q).join(' ') || "'/usr/lib'"}; do test -d "$directory" || exit 1; done
${config.runtime.qtPluginDirectory ? `test -f ${q(config.runtime.qtPluginDirectory + '/platforms/libqqnx.so')} || exit 1` : ''}
${config.runtime.qtFontDirectory ? `test -d ${q(config.runtime.qtFontDirectory)} || exit 1` : ''}
safe_file ${q(registry + '/' + upload)}
test "$(hash_file ${q(registry + '/' + upload)})" = ${q(config.icon.sha256)} || exit 1
safe_directory ${q(entry)}
safe_file ${q(entry + '/owner')}
safe_file ${q(registry + '/' + config.appId + '.desktop')}
if test -f ${q(registry + '/' + config.appId + '.desktop')}; then
  grep -Fx ${q('X-RNQNX-Generated-By=' + LAUNCHER_GENERATOR)} ${q(registry + '/' + config.appId + '.desktop')} >/dev/null || { echo 'Application ID collides with an OS launcher entry' >&2; exit 1; }
fi
if test -f ${q(entry + '/owner')}; then test "$(cat ${q(entry + '/owner')})" = ${q(files.owner.trim())} || exit 1;
else
  test -z "$(ls -A ${q(entry)})" || { echo 'Unowned app launcher entry' >&2; exit 1; }
  printf '%s\\n' ${q(files.owner.trim())} >${q(entry + '/owner')}
fi
`;
  script += `safe_file ${q(entry + '/icon.png')}\n`;
  for (const name of Object.keys(files)) script += `safe_file ${q(entry + '/' + name)}\n`;
  for (const [name, text] of Object.entries(files))
    script += writeText(entry + '/' + name + '.new', text);
  script += `safe_file ${q(registry + '/' + upload)}
test "$(hash_file ${q(registry + '/' + upload)})" = ${q(config.icon.sha256)} || exit 1
mv ${q(registry + '/' + upload)} ${q(entry + '/icon.png')}
chmod 700 ${q(entry + '/launch.sh.new')}
`;
  for (const name of Object.keys(files))
    script += `mv ${q(entry + '/' + name + '.new')} ${q(entry + '/' + name)}\n`;
  script += `safe_file ${q(registry + '/' + config.appId + '.desktop.new')}
cp ${q(entry + '/app.desktop')} ${q(registry + '/' + config.appId + '.desktop.new')}
mv ${q(registry + '/' + config.appId + '.desktop.new')} ${q(registry + '/' + config.appId + '.desktop')}
`;
  return script + rebuildRegistry(registry, config.runtime.libraryPaths, refresh);
}

/** @param {string} appId @param {string} home @param {boolean} refresh @param {string[]} [libraryPaths] @param {string} [stockDirectory] */
export function removeLauncherScript(appId, home, refresh, libraryPaths = [], stockDirectory) {
  validateAppIdentity({ appId, displayName: 'Application', componentName: 'main' });
  const registry = home + '/.local/share/rnqnx/launcher';
  const entry = registry + '/apps/' + appId;
  return (
    `set -eu\ntest "$(id -u)" -ne 0 || exit 1\ntest -e ${q(registry)} || exit 0\n` +
    registryPrelude(registry, home, stockDirectory) +
    `test ! -L ${q(entry)} || exit 1
if test -e ${q(entry)}; then
  test -f ${q(entry + '/owner')} && test ! -L ${q(entry + '/owner')} && test "$(cat ${q(entry + '/owner')})" = ${q(LAUNCHER_GENERATOR + ':' + appId)} || exit 1
  rm -f ${q(registry + '/' + appId + '.desktop')}
  rm -rf ${q(entry)}
fi
` +
    rebuildRegistry(registry, libraryPaths, refresh)
  );
}

/** @param {string[]} argv */
export async function launcherMain(argv) {
  const { values, positionals, tokens } = parseArgs({
    args: argv,
    allowPositionals: true,
    strict: true,
    tokens: true,
    options: {
      help: { type: 'boolean' },
      project: { type: 'string' },
      'app-dir': { type: 'string' },
      executable: { type: 'string' },
      'no-refresh': { type: 'boolean' },
      'dry-run': { type: 'boolean' },
    },
  });
  const seen = new Set();
  for (const token of tokens)
    if (token.kind === 'option') {
      if (seen.has(token.name)) throw new Error('Duplicate option --' + token.name);
      seen.add(token.name);
    }
  if (values.help || !argv.length) {
    console.log(HELP);
    return 0;
  }
  const command = positionals[0];
  if (positionals.length !== 1 || !['prepare', 'install', 'remove'].includes(command))
    throw new Error('Use launcher prepare, install or remove.');
  if (command !== 'install' && (values['app-dir'] || values.executable))
    throw new Error('Only install accepts --app-dir or --executable.');
  if (command === 'prepare') {
    if (values['no-refresh']) throw new Error('prepare does not accept --no-refresh.');
    console.log(
      JSON.stringify(await prepareLauncher(values.project ?? '.', !!values['dry-run']), null, 2)
    );
    return 0;
  }
  const identity = await launcherIdentity(values.project ?? '.');
  const prepared =
    command === 'install' ? await launcherConfiguration(values.project ?? '.') : null;
  if (command === 'install') targetPath(values['app-dir'], '--app-dir');
  if (values['dry-run']) {
    if (prepared)
      launcherFiles(
        prepared.configuration,
        values['app-dir'] ?? '',
        '/data/home/dryrun/launcher',
        values.executable
      );
    console.log(
      JSON.stringify(
        {
          action: command,
          appId: identity.identity.appId,
          configuration: prepared?.configuration,
          appDirectory: values['app-dir'],
          refreshLauncher: !values['no-refresh'],
          deviceAccessed: false,
        },
        null,
        2
      )
    );
    return 0;
  }
  const target = launcherTarget(process.env);
  const ssh = [...target.options, '-p', target.port, target.address];
  const homeOutput = String(
    await runLauncherTool('ssh', [
      ...ssh,
      'test "$(id -u)" -ne 0 && printf "RNQNX_HOME=%s\\n" "$HOME"',
    ])
  );
  const home = homeOutput.trim().match(/^RNQNX_HOME=(\/[^\n]+)$/)?.[1];
  targetPath(home, 'target home');
  if (!home || home === '/' || !home.includes('/home/'))
    throw new Error('Expected a non-root target home directory.');
  if (prepared) {
    const local = await prepareLauncher(values.project ?? '.');
    const registry = home + '/.local/share/rnqnx/launcher';
    await runLauncherTool(
      'ssh',
      [...ssh, '/bin/sh -s'],
      registryPrelude(registry, home) + 'true\n'
    );
    const upload = 'incoming-' + randomBytes(12).toString('hex') + '.png';
    await runLauncherTool('scp', [
      '-O',
      ...target.options,
      '-P',
      target.port,
      path.join(local.output, 'icon.png'),
      target.address + ':' + registry + '/' + upload,
    ]);
    await runLauncherTool(
      'ssh',
      [...ssh, '/bin/sh -s'],
      installLauncherScript(
        prepared.configuration,
        home,
        values['app-dir'] ?? '',
        values.executable ?? prepared.configuration.appId,
        !values['no-refresh'],
        undefined,
        upload
      )
    );
  } else
    await runLauncherTool(
      'ssh',
      [...ssh, '/bin/sh -s'],
      removeLauncherScript(
        identity.identity.appId,
        home,
        !values['no-refresh'],
        identity.runtime.libraryPaths
      )
    );
  console.log(
    JSON.stringify(
      {
        action: command,
        appId: identity.identity.appId,
        registered: command === 'install',
        appBinariesOrDataRemoved: false,
        bootChanged: false,
        physicalDisplayVerified: false,
      },
      null,
      2
    )
  );
  return 0;
}
