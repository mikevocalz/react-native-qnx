import type { ConfigT, InputConfigT } from 'metro-config';

export declare const MARATHON_PLATFORM: 'marathon';

/** Metro resolution result for a source file. */
export interface MarathonResolution {
  type: string;
  filePath?: string;
  [key: string]: unknown;
}

export interface MarathonResolveContext {
  originModulePath: string;
  resolveRequest(
    context: MarathonResolveContext,
    moduleName: string,
    platform: string | null
  ): MarathonResolution;
  [key: string]: unknown;
}

export declare function createMarathonResolveRequest(
  marathonOverridesRoot?: string,
  qnxOverridesRoot?: string
): (
  context: MarathonResolveContext,
  moduleName: string,
  platform: string | null
) => MarathonResolution;

export declare function getMarathonMetroConfig(
  projectRoot: string,
  overrides?: InputConfigT
): ConfigT;
