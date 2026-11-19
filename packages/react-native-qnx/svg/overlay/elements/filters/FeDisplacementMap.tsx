import React from 'react';
import type { NativeMethods } from 'react-native';
import FilterPrimitive from 'react-native-svg/src/elements/filters/FilterPrimitive';
import RNSVGFeDisplacementMap from '../../../fabric/FeDisplacementMapNativeComponent';
import { extractFilter, extractIn } from '../../lib/extract/extractFilter';
import { extractDisplacementMap } from '../../../extract/filterExtras';
import type { NumberProp } from '../../../extract/numbers';

type ChannelSelector = 'R' | 'G' | 'B' | 'A';

export interface FeDisplacementMapProps {
  in?: string;
  in2?: string;
  scale?: NumberProp;
  xChannelSelector?: ChannelSelector;
  yChannelSelector?: ChannelSelector;
}

// Replaces react-native-svg/src/elements/filters/FeDisplacementMap.tsx on QNX. The package
// declares this element but its render() warns and returns null. This renders the
// RNSVGFeDisplacementMap Fabric component the host mounts.
export default class FeDisplacementMap extends FilterPrimitive<FeDisplacementMapProps> {
  static displayName = 'FeDisplacementMap';

  static defaultProps = {
    ...this.defaultPrimitiveProps,
  };

  render() {
    return (
      <RNSVGFeDisplacementMap
        ref={(ref) => this.refMethod(ref as (FeDisplacementMap & NativeMethods) | null)}
        {...extractFilter(this.props)}
        {...extractIn(this.props)}
        {...extractDisplacementMap(this.props)}
      />
    );
  }
}
