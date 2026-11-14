import React from 'react';
import type { NativeMethods } from 'react-native';
import FilterPrimitive from 'react-native-svg/src/elements/filters/FilterPrimitive';
import RNSVGFeDiffuseLighting from '../../../fabric/FeDiffuseLightingNativeComponent';
import { extractFilter, extractIn } from '../../lib/extract/extractFilter';
import { extractDiffuseLighting } from '../../../extract/filterExtras';
import type { ColorValue } from 'react-native';
import type { NumberArray, NumberProp } from '../../../extract/numbers';

export interface FeDiffuseLightingProps {
  in?: string;
  surfaceScale?: NumberProp;
  diffuseConstant?: NumberProp;
  lightingColor?: ColorValue;
  kernelUnitLength?: NumberArray;
  children?: React.ReactElement | Array<React.ReactElement>;
}

// Replaces react-native-svg/src/elements/filters/FeDiffuseLighting.tsx on QNX. The package
// declares this element but its render() warns and returns null. This renders the
// RNSVGFeDiffuseLighting Fabric component the host mounts.
export default class FeDiffuseLighting extends FilterPrimitive<FeDiffuseLightingProps> {
  static displayName = 'FeDiffuseLighting';

  static defaultProps = {
    ...this.defaultPrimitiveProps,
  };

  render() {
    return (
      <RNSVGFeDiffuseLighting
        ref={(ref) => this.refMethod(ref as (FeDiffuseLighting & NativeMethods) | null)}
        {...extractFilter(this.props)}
        {...extractIn(this.props)}
        {...extractDiffuseLighting(this.props)}
      />
    );
  }
}
