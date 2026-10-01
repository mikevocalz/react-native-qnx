export interface QnxPlatformInfo {
  readonly platform: 'qnx';
  readonly architecture: string;
  add(left: number, right: number): number;
}

export function createQnxPlatformInfo(): QnxPlatformInfo;
