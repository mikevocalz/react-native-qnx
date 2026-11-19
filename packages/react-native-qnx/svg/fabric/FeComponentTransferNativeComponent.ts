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

// One feFuncR, feFuncG, feFuncB or feFuncA child. The JavaScript side reads
// the children, so no native node exists for them.
//
// `type` is a plain string (identity, table, discrete, linear, gamma). As an
// enum, codegen emits one enum definition per struct that uses this shape, and
// the four copies redefine the same C++ type.
type TransferFunction = Readonly<{
  type?: string;
  tableValues?: ReadonlyArray<CodegenTypes.Double>;
  slope?: CodegenTypes.WithDefault<CodegenTypes.Double, 1>;
  intercept?: CodegenTypes.WithDefault<CodegenTypes.Double, 0>;
  amplitude?: CodegenTypes.WithDefault<CodegenTypes.Double, 1>;
  exponent?: CodegenTypes.WithDefault<CodegenTypes.Double, 1>;
  offset?: CodegenTypes.WithDefault<CodegenTypes.Double, 0>;
}>;

export interface NativeProps extends ViewProps, FilterPrimitiveCommonProps {
  in1?: string;
  funcR?: TransferFunction;
  funcG?: TransferFunction;
  funcB?: TransferFunction;
  funcA?: TransferFunction;
}

export default codegenNativeComponent<NativeProps>('RNSVGFeComponentTransfer', {
  interfaceOnly: true,
});
