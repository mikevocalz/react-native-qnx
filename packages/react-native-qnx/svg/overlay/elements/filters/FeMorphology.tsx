import React from 'react';
import type { NativeMethods } from 'react-native';
import FilterPrimitive from 'react-native-svg/src/elements/filters/FilterPrimitive';
import RNSVGFeMorphology from '../../../fabric/FeMorphologyNativeComponent';
import { extractFilter, extractIn } from '../../lib/extract/extractFilter';
import { extractMorphology } from '../../../extract/filterExtras';
import type { NumberArray } from '../../../extract/numbers';

export interface FeMorphologyProps {
  in?: string;
  operator?: 'erode' | 'dilate';
  radius?: NumberArray;
}

// Replaces react-native-svg/src/elements/filters/FeMorphology.tsx on QNX. The package
// declares this element but its render() warns and returns null. This renders the
// RNSVGFeMorphology Fabric component the host mounts.
export default class FeMorphology extends FilterPrimitive<FeMorphologyProps> {
  static displayName = 'FeMorphology';

  static defaultProps = {
    ...this.defaultPrimitiveProps,
  };

  render() {
    return (
      <RNSVGFeMorphology
        ref={(ref) => this.refMethod(ref as (FeMorphology & NativeMethods) | null)}
        {...extractFilter(this.props)}
        {...extractIn(this.props)}
        {...extractMorphology(this.props)}
      />
    );
  }
}
