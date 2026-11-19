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

type MorphologyOperator = 'erode' | 'dilate';

export interface NativeProps extends ViewProps, FilterPrimitiveCommonProps {
  in1?: string;
  // `operator` is a C++ keyword, so the generated member could not use it.
  operator1?: CodegenTypes.WithDefault<MorphologyOperator, 'erode'>;
  radiusX?: UnsafeMixed<NumberProp>;
  radiusY?: UnsafeMixed<NumberProp>;
}

export default codegenNativeComponent<NativeProps>('RNSVGFeMorphology', {
  interfaceOnly: true,
});
