export interface ExpoQnxPlatformModule {
  readonly platform: 'qnx';
  readonly architecture: string;
}

export function getQnxExpoPlatformModule(): ExpoQnxPlatformModule | null;
