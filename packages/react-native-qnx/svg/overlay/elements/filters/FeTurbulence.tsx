import React from 'react';
import type { NativeMethods } from 'react-native';
import FilterPrimitive from 'react-native-svg/src/elements/filters/FilterPrimitive';
import RNSVGFeTurbulence from '../../../fabric/FeTurbulenceNativeComponent';
import { extractFilter, extractIn } from '../../lib/extract/extractFilter';
import { extractTurbulence } from '../../../extract/filterExtras';
import type { NumberArray, NumberProp } from '../../../extract/numbers';

export interface FeTurbulenceProps {
  baseFrequency?: NumberArray;
  numOctaves?: NumberProp;
  seed?: NumberProp;
  stitchTiles?: 'stitch' | 'noStitch';
  type?: 'fractalNoise' | 'turbulence';
}

// Replaces react-native-svg/src/elements/filters/FeTurbulence.tsx on QNX. The package
// declares this element but its render() warns and returns null. This renders the
// RNSVGFeTurbulence Fabric component the host mounts.
export default class FeTurbulence extends FilterPrimitive<FeTurbulenceProps> {
  static displayName = 'FeTurbulence';

  static defaultProps = {
    ...this.defaultPrimitiveProps,
  };

  render() {
    return (
      <RNSVGFeTurbulence
        ref={(ref) => this.refMethod(ref as (FeTurbulence & NativeMethods) | null)}
        {...extractFilter(this.props)}
        {...extractTurbulence(this.props)}
      />
    );
  }
}
