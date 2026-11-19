import React from 'react';
import type { NativeMethods } from 'react-native';
import FilterPrimitive from 'react-native-svg/src/elements/filters/FilterPrimitive';
import RNSVGFeImage from '../../../fabric/FeImageNativeComponent';
import { extractFilter, extractIn } from '../../lib/extract/extractFilter';
import { extractImage } from '../../../extract/filterExtras';

export interface FeImageProps {
  href?: string | number | { uri?: string };
  xlinkHref?: string | number | { uri?: string };
  preserveAspectRatio?: string;
  crossOrigin?: 'anonymous' | 'use-credentials' | '';
}

// Replaces react-native-svg/src/elements/filters/FeImage.tsx on QNX. The package
// declares this element but its render() warns and returns null. This renders the
// RNSVGFeImage Fabric component the host mounts.
export default class FeImage extends FilterPrimitive<FeImageProps> {
  static displayName = 'FeImage';

  static defaultProps = {
    ...this.defaultPrimitiveProps,
  };

  render() {
    return (
      <RNSVGFeImage
        ref={(ref) => this.refMethod(ref as (FeImage & NativeMethods) | null)}
        {...extractFilter(this.props)}
        {...extractImage(this.props)}
      />
    );
  }
}
