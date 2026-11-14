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

type ChannelSelector = 'R' | 'G' | 'B' | 'A';

export interface NativeProps extends ViewProps, FilterPrimitiveCommonProps {
  in1?: string;
  in2?: string;
  scale?: CodegenTypes.Double;
  xChannelSelector?: CodegenTypes.WithDefault<ChannelSelector, 'A'>;
  yChannelSelector?: CodegenTypes.WithDefault<ChannelSelector, 'A'>;
}

export default codegenNativeComponent<NativeProps>('RNSVGFeDisplacementMap', {
  interfaceOnly: true,
});
