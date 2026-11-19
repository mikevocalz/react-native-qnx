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

export interface NativeProps extends ViewProps, FilterPrimitiveCommonProps {
  // A data: or file: URL, an absolute path, or `#id` for an element of the
  // same document. Asset numbers from require() are resolved to a URI in
  // JavaScript.
  href?: string;
  align?: string;
  // 0 meet, 1 slice, 2 none: same numbering as RNSVGImage.
  meetOrSlice?: CodegenTypes.WithDefault<CodegenTypes.Int32, 0>;
}

export default codegenNativeComponent<NativeProps>('RNSVGFeImage', {
  interfaceOnly: true,
});
