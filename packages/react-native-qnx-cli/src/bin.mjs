#!/usr/bin/env node
import { main } from './cli.mjs';
import { projectMain } from './project.mjs';
import { doctorMain } from './doctor.mjs';
import { prepareNativeMain } from './prepare-native.mjs';
import { createProjectMain } from './create-project.mjs';
import { buildNativeMain } from './build-native.mjs';
import { marathonMain } from '../../qnx-devtools/src/marathon.mjs';

try {
  const args = process.argv.slice(2);
  if (args[0] === 'project' && args[1] === 'doctor')
    process.exitCode = await doctorMain(args.slice(2));
  else if (args[0] === 'project' && args[1] === 'build-native')
    process.exitCode = await buildNativeMain(args.slice(2));
  else if (args[0] === 'project' && args[1] === 'create')
    process.exitCode = await createProjectMain(args.slice(2));
  else if (args[0] === 'project' && args[1] === 'prepare-native')
    process.exitCode = await prepareNativeMain(args.slice(2));
  else if (args[0] === 'marathon') process.exitCode = await marathonMain(args.slice(1));
  else if (args[0] === 'project') process.exitCode = await projectMain(args.slice(1));
  else process.exitCode = await main(args);
  if (args.length === 0 || args[0] === '--help' || args[0] === '-h') {
    console.log(
      '\nExternal apps: rnqnx project --help (bundle/Metro server; no native provisioning).'
    );
    console.log('Create a shared Expo app: rnqnx project create --help.');
    console.log('Native application configuration: rnqnx project prepare-native --help.');
    console.log('Build native consumer application: rnqnx project build-native --help.');
    console.log('Consumer diagnostics: rnqnx project doctor --help.');
    console.log('Marathon development client: rnqnx marathon --help.');
  }
} catch (error) {
  console.error(`rnqnx: ${error instanceof Error ? error.message : error}`);
  process.exitCode = process.argv[2] === 'project' && process.argv[3] === 'doctor' ? 2 : 1;
}
