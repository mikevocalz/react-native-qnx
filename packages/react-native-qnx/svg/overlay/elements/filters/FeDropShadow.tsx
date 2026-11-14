import React from 'react';
import type { NativeMethods } from 'react-native';
import FilterPrimitive from 'react-native-svg/src/elements/filters/FilterPrimitive';
import RNSVGFeDropShadow from '../../../fabric/FeDropShadowNativeComponent';
import { extractFilter, extractIn } from '../../lib/extract/extractFilter';
import { extractDropShadow } from '../../../extract/filterExtras';
import type { ColorValue } from 'react-native';
import type { NumberArray, NumberProp } from '../../../extract/numbers';

export interface FeDropShadowProps {
  in?: string;
  stdDeviation?: NumberArray;
  dx?: NumberProp;
  dy?: NumberProp;
  floodColor?: ColorValue;
  floodOpacity?: NumberProp;
}

// Replaces react-native-svg/src/elements/filters/FeDropShadow.tsx on QNX.
// The package expands this element into a blur, an offset, a flood, a
// composite and a merge, and gives dx and dy a default of 0 where SVG says 2.
// This renders the RNSVGFeDropShadow Fabric component instead, with SVG's defaults.
export default class FeDropShadow extends FilterPrimitive<FeDropShadowProps> {
  static displayName = 'FeDropShadow';

  static defaultProps = {
    ...this.defaultPrimitiveProps,
  };

  render() {
    return (
      <RNSVGFeDropShadow
        ref={(ref) => this.refMethod(ref as (FeDropShadow & NativeMethods) | null)}
        {...extractFilter(this.props)}
        {...extractIn(this.props)}
        {...extractDropShadow(this.props)}
      />
    );
  }
}
