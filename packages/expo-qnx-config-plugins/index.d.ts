declare function withQnxProject<T extends withQnxProject.ExpoConfigLike>(
  config: T,
  options?: withQnxProject.QnxProjectOptions
): withQnxProject.WithQnxProjectResult<T>;

declare namespace withQnxProject {
  type WindowControls = 'macos' | 'windows';
  type TitleBarStyle = 'default' | 'hidden' | 'none';
  type UnavailableControls = 'hide' | 'disable';

  interface QnxWindowOptions {
    /** Title bar button layout. Default: 'macos'. */
    controls?: WindowControls;
    /** Default: 'default'. */
    titleBarStyle?: TitleBarStyle;
    /** Initial window size in logical pixels, 1-16384 per edge. */
    initialSize?: { width: number; height: number };
    /**
     * A control the platform cannot run: left out ('hide', the default) or
     * drawn grey and inert ('disable').
     */
    unavailableControls?: UnavailableControls;
  }

  interface ResolvedQnxWindow {
    controls: WindowControls;
    titleBarStyle: TitleBarStyle;
    initialSize?: { width: number; height: number };
    unavailableControls?: UnavailableControls;
  }

  interface QnxProjectOptions {
    targetArch?: 'aarch64' | 'x86_64';
    autolinkingJson?: string;
    autolinkingCMake?: string;
    /** Reverse-DNS application ID written to extra.rnqnx.appId. */
    appId?: string;
    /** Registered root component name written to extra.rnqnx.componentName. */
    componentName?: string;
    /** Launcher tile label; expo.name is used when absent. */
    shortName?: string;
    /** Icon source overriding expo.icon. */
    icon?: string;
    window?: QnxWindowOptions;
  }

  interface ResolvedQnxProjectOptions {
    targetArch: 'aarch64' | 'x86_64';
    autolinkingJson: string;
    autolinkingCMake: string;
  }

  interface QnxNativeConfig {
    appId?: string;
    componentName?: string;
    shortName?: string;
    icon?: string;
    window?: ResolvedQnxWindow;
  }

  interface ExpoConfigLike {
    extra?: Record<string, any>;
    [key: string]: any;
  }

  type WithQnxProjectResult<T extends ExpoConfigLike> = Omit<T, 'extra'> & {
    extra: NonNullable<T['extra']> & {
      rnQnx: ResolvedQnxProjectOptions;
      rnqnx?: QnxNativeConfig;
    };
  };

  function resolveQnxWindow(value: unknown): ResolvedQnxWindow;
  const WINDOW_CONTROLS: readonly WindowControls[];
  const TITLE_BAR_STYLES: readonly TitleBarStyle[];
}

export = withQnxProject;
