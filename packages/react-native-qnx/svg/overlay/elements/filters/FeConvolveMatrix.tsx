import React from 'react';
import type { NativeMethods } from 'react-native';
import FilterPrimitive from 'react-native-svg/src/elements/filters/FilterPrimitive';
import RNSVGFeConvolveMatrix from '../../../fabric/FeConvolveMatrixNativeComponent';
import { extractFilter, extractIn } from '../../lib/extract/extractFilter';
import { extractConvolveMatrix } from '../../../extract/filterExtras';
import type { NumberArray, NumberProp } from '../../../extract/numbers';

export interface FeConvolveMatrixProps {
  in?: string;
  order?: NumberArray;
  kernelMatrix?: NumberArray;
  divisor?: NumberProp;
  bias?: NumberProp;
  targetX?: NumberProp;
  targetY?: NumberProp;
  edgeMode?: 'duplicate' | 'wrap' | 'none';
  kernelUnitLength?: NumberArray;
  preserveAlpha?: boolean | 'true' | 'false';
}

// Replaces react-native-svg/src/elements/filters/FeConvolveMatrix.tsx on QNX. The package
// declares this element but its render() warns and returns null. This renders the
// RNSVGFeConvolveMatrix Fabric component the host mounts.
export default class FeConvolveMatrix extends FilterPrimitive<FeConvolveMatrixProps> {
  static displayName = 'FeConvolveMatrix';

  static defaultProps = {
    ...this.defaultPrimitiveProps,
  };

  render() {
    return (
      <RNSVGFeConvolveMatrix
        ref={(ref) => this.refMethod(ref as (FeConvolveMatrix & NativeMethods) | null)}
        {...extractFilter(this.props)}
        {...extractIn(this.props)}
        {...extractConvolveMatrix(this.props)}
      />
    );
  }
}
