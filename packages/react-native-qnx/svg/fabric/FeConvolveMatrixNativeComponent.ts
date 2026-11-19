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

type ConvolveEdgeMode = 'duplicate' | 'wrap' | 'none';

export interface NativeProps extends ViewProps, FilterPrimitiveCommonProps {
  in1?: string;
  orderX?: CodegenTypes.WithDefault<CodegenTypes.Int32, 3>;
  orderY?: CodegenTypes.WithDefault<CodegenTypes.Int32, 3>;
  kernelMatrix?: ReadonlyArray<CodegenTypes.Double>;
  // 0: the sum of the kernel, or 1 when that sum is 0.
  divisor?: CodegenTypes.Double;
  bias?: CodegenTypes.Double;
  // -1: floor(order / 2).
  targetX?: CodegenTypes.WithDefault<CodegenTypes.Int32, -1>;
  targetY?: CodegenTypes.WithDefault<CodegenTypes.Int32, -1>;
  edgeMode?: CodegenTypes.WithDefault<ConvolveEdgeMode, 'duplicate'>;
  preserveAlpha?: CodegenTypes.WithDefault<boolean, false>;
}

export default codegenNativeComponent<NativeProps>('RNSVGFeConvolveMatrix', {
  interfaceOnly: true,
});
