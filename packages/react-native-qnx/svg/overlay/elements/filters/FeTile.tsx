import React from 'react';
import type { NativeMethods } from 'react-native';
import FilterPrimitive from 'react-native-svg/src/elements/filters/FilterPrimitive';
import RNSVGFeTile from '../../../fabric/FeTileNativeComponent';
import { extractFilter, extractIn } from '../../lib/extract/extractFilter';

export interface FeTileProps {
  in?: string;
}

// Replaces react-native-svg/src/elements/filters/FeTile.tsx on QNX. The package
// declares this element but its render() warns and returns null. This renders the
// RNSVGFeTile Fabric component the host mounts.
export default class FeTile extends FilterPrimitive<FeTileProps> {
  static displayName = 'FeTile';

  static defaultProps = {
    ...this.defaultPrimitiveProps,
  };

  render() {
    return (
      <RNSVGFeTile
        ref={(ref) => this.refMethod(ref as (FeTile & NativeMethods) | null)}
        {...extractFilter(this.props)}
        {...extractIn(this.props)}
      />
    );
  }
}
