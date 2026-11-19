import React from 'react';
import type { NativeMethods } from 'react-native';
import FilterPrimitive from 'react-native-svg/src/elements/filters/FilterPrimitive';
import RNSVGFeComponentTransfer from '../../../fabric/FeComponentTransferNativeComponent';
import { extractFilter, extractIn } from '../../lib/extract/extractFilter';
import { extractComponentTransfer } from '../../../extract/filterExtras';

export interface FeComponentTransferProps {
  in?: string;
  children?: React.ReactElement | Array<React.ReactElement>;
}

// Replaces react-native-svg/src/elements/filters/FeComponentTransfer.tsx on QNX. The package
// declares this element but its render() warns and returns null. This renders the
// RNSVGFeComponentTransfer Fabric component the host mounts.
export default class FeComponentTransfer extends FilterPrimitive<FeComponentTransferProps> {
  static displayName = 'FeComponentTransfer';

  static defaultProps = {
    ...this.defaultPrimitiveProps,
  };

  render() {
    return (
      <RNSVGFeComponentTransfer
        ref={(ref) => this.refMethod(ref as (FeComponentTransfer & NativeMethods) | null)}
        {...extractFilter(this.props)}
        {...extractIn(this.props)}
        {...extractComponentTransfer(this.props)}
      />
    );
  }
}
