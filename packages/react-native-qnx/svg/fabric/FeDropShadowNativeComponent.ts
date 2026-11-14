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

// SVG defaults: dx = dy = 2, stdDeviation = 2, flood-color black, opacity 1.
export interface NativeProps extends ViewProps, FilterPrimitiveCommonProps {
  in1?: string;
  dx?: UnsafeMixed<NumberProp>;
  dy?: UnsafeMixed<NumberProp>;
  stdDeviationX?: CodegenTypes.WithDefault<CodegenTypes.Double, 2>;
  stdDeviationY?: CodegenTypes.WithDefault<CodegenTypes.Double, 2>;
  floodColor?: UnsafeMixed<ColorValue | ColorStruct>;
  floodOpacity?: CodegenTypes.WithDefault<CodegenTypes.Float, 1.0>;
}

export default codegenNativeComponent<NativeProps>('RNSVGFeDropShadow', {
  interfaceOnly: true,
});
