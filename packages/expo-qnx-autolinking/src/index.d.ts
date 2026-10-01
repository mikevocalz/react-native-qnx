export interface QnxExpoModuleDescriptor {
  packageName: string;
  packageRoot: string;
  version: string;
  cmakeListsPath: string;
  target: string;
  modules: string[];
  binaryName: string;
}

export function findQnxExpoModules(projectRoot?: string): Promise<QnxExpoModuleDescriptor[]>;
export function resolveQnxSearchResults(
  searchResults: Record<string, unknown>
): QnxExpoModuleDescriptor[];
export function generateQnxCMake(descriptors: QnxExpoModuleDescriptor[]): string;
