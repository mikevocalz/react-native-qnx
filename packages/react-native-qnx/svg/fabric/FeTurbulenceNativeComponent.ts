import { codegenNativeComponent } from 'react-native';
import type { CodegenTypes, ViewProps } from 'react-native';

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

type TurbulenceType = 'fractalNoise' | 'turbulence';
type StitchTiles = 'stitch' | 'noStitch';

export interface NativeProps extends ViewProps, FilterPrimitiveCommonProps {
  baseFrequencyX?: CodegenTypes.Double;
  baseFrequencyY?: CodegenTypes.Double;
  numOctaves?: CodegenTypes.WithDefault<CodegenTypes.Int32, 1>;
  seed?: CodegenTypes.Double;
  stitchTiles?: CodegenTypes.WithDefault<StitchTiles, 'noStitch'>;
  type?: CodegenTypes.WithDefault<TurbulenceType, 'turbulence'>;
}

export default codegenNativeComponent<NativeProps>('RNSVGFeTurbulence', {
  interfaceOnly: true,
});
