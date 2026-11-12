import type { ConfigT, InputConfigT } from 'metro-config';

export declare const MARATHON_PLATFORM: 'marathon';

export declare function createMarathonResolveRequest(
  marathonOverridesRoot?: string,
  qnxOverridesRoot?: string
): (context: unknown, moduleName: string, platform: string | null) => unknown;

export declare function getMarathonMetroConfig(
  projectRoot: string,
  overrides?: InputConfigT
): ConfigT;
