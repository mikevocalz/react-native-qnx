import React from 'react';
import type { NativeMethods } from 'react-native';
import FilterPrimitive from 'react-native-svg/src/elements/filters/FilterPrimitive';
import RNSVGFeSpecularLighting from '../../../fabric/FeSpecularLightingNativeComponent';
import { extractFilter, extractIn } from '../../lib/extract/extractFilter';
import { extractSpecularLighting } from '../../../extract/filterExtras';
import type { ColorValue } from 'react-native';
import type { NumberArray, NumberProp } from '../../../extract/numbers';

export interface FeSpecularLightingProps {
  in?: string;
  surfaceScale?: NumberProp;
  specularConstant?: NumberProp;
  specularExponent?: NumberProp;
  lightingColor?: ColorValue;
  kernelUnitLength?: NumberArray;
  children?: React.ReactElement | Array<React.ReactElement>;
}

// Replaces react-native-svg/src/elements/filters/FeSpecularLighting.tsx on QNX. The package
// declares this element but its render() warns and returns null. This renders the
// RNSVGFeSpecularLighting Fabric component the host mounts.
export default class FeSpecularLighting extends FilterPrimitive<FeSpecularLightingProps> {
  static displayName = 'FeSpecularLighting';

  static defaultProps = {
    ...this.defaultPrimitiveProps,
  };

  render() {
    return (
      <RNSVGFeSpecularLighting
        ref={(ref) => this.refMethod(ref as (FeSpecularLighting & NativeMethods) | null)}
        {...extractFilter(this.props)}
        {...extractIn(this.props)}
        {...extractSpecularLighting(this.props)}
      />
    );
  }
}
