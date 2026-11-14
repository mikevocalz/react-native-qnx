#!/usr/bin/env node
import { main } from './cli.mjs';
import { projectMain } from './project.mjs';
import { doctorMain } from './doctor.mjs';
import { prepareNativeMain } from './prepare-native.mjs';
import { createProjectMain } from './create-project.mjs';
import { buildNativeMain } from './build-native.mjs';
import { desktopEntryMain } from './desktop-entry.mjs';
import { addQnxMain } from './add-qnx.mjs';
import { runMain } from './run.mjs';
import { devMain, startMain } from './dev-server.mjs';
import { runnerMain } from './runner.mjs';
import { marathonMain } from '../../qnx-devtools/src/marathon.mjs';

// Preserve the existing root environment doctor unless this mode is explicit.
const targetRuntimeDoctor =
  process.argv[2] === 'doctor' &&
  process.argv
    .slice(3)
    .some((arg) => arg === '--target-runtime-report' || arg.startsWith('--target-runtime-report='));

try {
  const args = process.argv.slice(2);
  if (targetRuntimeDoctor) process.exitCode = await doctorMain(args.slice(1));
  else if (args[0] === 'project' && args[1] === 'doctor')
    process.exitCode = await doctorMain(args.slice(2));
  else if (args[0] === 'project' && args[1] === 'build-native')
    process.exitCode = await buildNativeMain(args.slice(2));
  else if (args[0] === 'project' && args[1] === 'create')
    process.exitCode = await createProjectMain(args.slice(2));
  else if (args[0] === 'project' && args[1] === 'add-qnx')
    process.exitCode = await addQnxMain(args.slice(2));
  else if (args[0] === 'project' && args[1] === 'run')
    process.exitCode = await runMain(args.slice(2));
  else if (args[0] === 'project' && args[1] === 'start')
    process.exitCode = await startMain(args.slice(2));
  else if (args[0] === 'project' && args[1] === 'dev')
    process.exitCode = await devMain(args.slice(2));
  else if (args[0] === 'project' && args[1] === 'prepare-native')
    process.exitCode = await prepareNativeMain(args.slice(2));
  else if (args[0] === 'project' && args[1] === 'desktop-entry')
    process.exitCode = await desktopEntryMain(args.slice(2));
  else if (args[0] === 'runner') process.exitCode = await runnerMain(args.slice(1));
  else if (args[0] === 'marathon') process.exitCode = await marathonMain(args.slice(1));
  else if (args[0] === 'project') process.exitCode = await projectMain(args.slice(1));
  else process.exitCode = await main(args);
  if (args.length === 0 || args[0] === '--help' || args[0] === '-h') {
    console.log('\nMore commands (each has --help):');
    console.log('  pnpm qnx project --help                 Bundle an app outside this checkout');
    console.log('  pnpm qnx project create --help          Create a shared Expo app');
    console.log('  pnpm qnx project add-qnx --help         Add QNX to an existing Expo app');
    console.log('  pnpm qnx project run --help             Build, deploy and launch');
    console.log(
      '  pnpm qnx project start --help           Metro with Fast Refresh for a dev build'
    );
    console.log(
      '  pnpm qnx project dev --help             Metro + launch the dev build on a board'
    );
    console.log('  pnpm qnx project prepare-native --help  Configure a native application');
    console.log('  pnpm qnx project build-native --help    Build a native consumer application');
    console.log('  pnpm qnx project desktop-entry --help   Write the QNX desktop launcher tile');
    console.log('  pnpm qnx project doctor --help          Check a consumer project');
    console.log(
      '  pnpm qnx doctor --target-runtime-report <file> [--verbose]  Check supplied QNX observations'
    );
    console.log('  pnpm qnx marathon --help                Marathon development client');
    console.log('  pnpm qnx runner --help                  Download a prebuilt development runner');
  }
} catch (error) {
  console.error(`rnqnx: ${error instanceof Error ? error.message : error}`);
  process.exitCode =
    targetRuntimeDoctor || (process.argv[2] === 'project' && process.argv[3] === 'doctor') ? 2 : 1;
}
