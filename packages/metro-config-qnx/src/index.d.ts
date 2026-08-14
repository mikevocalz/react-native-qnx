import type { InputConfigT } from 'metro-config';

export declare const QNX_PLATFORM: 'qnx';

export declare function getQnxMetroConfig(
  projectRoot: string,
  overrides?: InputConfigT
): InputConfigT;
