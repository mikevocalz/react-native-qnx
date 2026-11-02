export interface ExpoQnxPlatformModule {
  readonly platform: 'qnx';
  readonly architecture: string;
}

export function getQnxExpoPlatformModule(): ExpoQnxPlatformModule | null;

/**
 * Require QNX's native Expo JSI platform module.
 *
 * Throws when the QNX native host has not installed it. This is a synchronous
 * runtime lookup, NOT an Expo Modules API 2.0 Swift/Kotlin binding.
 */
export function requireQnxExpoPlatformModule(): ExpoQnxPlatformModule;
