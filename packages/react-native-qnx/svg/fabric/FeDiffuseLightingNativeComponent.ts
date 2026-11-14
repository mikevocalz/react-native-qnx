import { codegenNativeComponent } from 'react-native';
import type { CodegenTypes, ColorValue, ViewProps } from 'react-native';

import type { UnsafeMixed } from './codegenUtils';

type NumberProp = string | number;

interface FilterPrimitiveCommonProps {
  x?: UnsafeMixed<NumberProp>;
  y?: UnsafeMixed<NumberProp>;
  width?: UnsafeMixed<NumberProp>;
  height?: UnsafeMixed<NumberProp>;
  result?: string;
  colorInterpolationFilters?: CodegenTypes.WithDefault<'auto' | 'sRGB' | 'linearRGB', 'auto'>;
}

type ColorStruct = Readonly<{
  type?: CodegenTypes.WithDefault<CodegenTypes.Int32, -1>;
  payload?: ColorValue;
  brushRef?: string;
}>;

type LightKind = 'none' | 'distant' | 'point' | 'spot';

// The feDistantLight, fePointLight or feSpotLight child. `none` means the
// element had no light source child, which SVG treats as an error.
type LightSource = Readonly<{
  kind?: CodegenTypes.WithDefault<LightKind, 'none'>;
  azimuth?: CodegenTypes.Double;
  elevation?: CodegenTypes.Double;
  x?: CodegenTypes.Double;
  y?: CodegenTypes.Double;
  z?: CodegenTypes.Double;
  pointsAtX?: CodegenTypes.Double;
  pointsAtY?: CodegenTypes.Double;
  pointsAtZ?: CodegenTypes.Double;
  specularExponent?: CodegenTypes.WithDefault<CodegenTypes.Double, 1>;
  limitingConeAngle?: CodegenTypes.Double;
  hasLimitingCone?: CodegenTypes.WithDefault<boolean, false>;
}>;

export interface NativeProps extends ViewProps, FilterPrimitiveCommonProps {
  in1?: string;
  surfaceScale?: CodegenTypes.WithDefault<CodegenTypes.Double, 1>;
  diffuseConstant?: CodegenTypes.WithDefault<CodegenTypes.Double, 1>;
  lightingColor?: UnsafeMixed<ColorValue | ColorStruct>;
  light?: LightSource;
}

export default codegenNativeComponent<NativeProps>('RNSVGFeDiffuseLighting', {
  interfaceOnly: true,
});
