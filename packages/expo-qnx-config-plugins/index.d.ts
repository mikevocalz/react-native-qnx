declare function withQnxProject<T extends withQnxProject.ExpoConfigLike>(
  config: T,
  options?: withQnxProject.QnxProjectOptions
): withQnxProject.WithQnxProjectResult<T>;

declare namespace withQnxProject {
  interface QnxProjectOptions {
    targetArch?: 'aarch64' | 'x86_64';
    autolinkingJson?: string;
    autolinkingCMake?: string;
  }

  interface ResolvedQnxProjectOptions {
    targetArch: 'aarch64' | 'x86_64';
    autolinkingJson: string;
    autolinkingCMake: string;
  }

  interface ExpoConfigLike {
    extra?: Record<string, any>;
    [key: string]: any;
  }

  type WithQnxProjectResult<T extends ExpoConfigLike> = Omit<T, 'extra'> & {
    extra: NonNullable<T['extra']> & {
      rnQnx: ResolvedQnxProjectOptions;
    };
  };
}

export = withQnxProject;
