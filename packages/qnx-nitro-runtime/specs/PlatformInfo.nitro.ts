import { HybridObject } from 'nitro-core';

/** Portable synchronous Nitro v1 contract for QNX and MarathonOS. */
export declare class PlatformInfo extends HybridObject {
  readonly platform: string;
  readonly architecture: string;
  add(left: number, right: number): number;
}
