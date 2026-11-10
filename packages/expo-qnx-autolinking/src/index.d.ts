export interface QnxExpoModuleDescriptor {
  packageName: string;
  packageRoot: string;
  version: string;
  cmakeListsPath: string;
  target: string;
  modules: string[];
  registrationFunction?: string;
  binaryName: string;
}

export type ExpoNativePlatform = 'qnx' | 'marathon';
export type ExpoNativeModuleDescriptor = QnxExpoModuleDescriptor;

export function findQnxExpoModules(projectRoot?: string): Promise<QnxExpoModuleDescriptor[]>;
export function resolveQnxSearchResults(
  searchResults: Record<string, unknown>
): QnxExpoModuleDescriptor[];
export function generateQnxCMake(descriptors: QnxExpoModuleDescriptor[]): string;
export function findExpoModulesForPlatform(
  projectRoot?: string,
  platform?: ExpoNativePlatform
): Promise<ExpoNativeModuleDescriptor[]>;
export function resolveExpoSearchResults(
  searchResults: Record<string, unknown>,
  platform?: ExpoNativePlatform
): ExpoNativeModuleDescriptor[];
export function generateExpoPlatformCMake(
  descriptors: ExpoNativeModuleDescriptor[],
  platform?: ExpoNativePlatform
): string;
