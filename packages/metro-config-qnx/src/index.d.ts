import type { ConfigT, InputConfigT } from 'metro-config';

export declare const QNX_PLATFORM: 'qnx';

export declare function createQnxResolveRequest(
  overridesRoot: string
): (context: unknown, moduleName: string, platform: string | null) => unknown;

export declare function getQnxMetroConfig(projectRoot: string, overrides?: InputConfigT): ConfigT;
