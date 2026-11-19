import React from 'react';
import type { NativeMethods } from 'react-native';
import Shape from 'react-native-svg/src/elements/Shape';
import RNSVGFilter from 'react-native-svg/src/fabric/FilterNativeComponent';
import type { NumberProp, Units } from 'react-native-svg/src/lib/extract/types';
import { extractColorInterpolation } from '../../../extract/filterExtras';

export interface FilterProps {
  children?: React.ReactNode;
  id?: string;
  x?: NumberProp;
  y?: NumberProp;
  width?: NumberProp;
  height?: NumberProp;
  filterUnits?: Units;
  primitiveUnits?: Units;
  colorInterpolationFilters?: 'auto' | 'sRGB' | 'linearRGB';
}

/**
 * color-interpolation-filters is an inherited property: a primitive without
 * one of its own uses its <filter>'s. The native element tree cannot look at
 * a parent while converting props, so the value is copied onto each primitive
 * child that has none.
 */
function inheritColorInterpolation(children: React.ReactNode, value: string | undefined) {
  if (value === undefined || value === 'auto') return children;
  return React.Children.map(children, (child) => {
    if (!React.isValidElement<{ colorInterpolationFilters?: string }>(child)) return child;
    const own = child.props.colorInterpolationFilters;
    return own === undefined || own === 'auto'
      ? React.cloneElement(child, { colorInterpolationFilters: value })
      : child;
  });
}

// Replaces react-native-svg/src/elements/filters/Filter.tsx on QNX. Same
// element, plus color-interpolation-filters, which its primitives inherit.
export default class Filter extends Shape<FilterProps> {
  static displayName = 'Filter';

  static defaultProps: React.ComponentProps<typeof Filter> = {
    x: '-10%',
    y: '-10%',
    width: '120%',
    height: '120%',
    filterUnits: 'objectBoundingBox',
    primitiveUnits: 'userSpaceOnUse',
  };

  render() {
    const { id, x, y, width, height, filterUnits, primitiveUnits } = this.props;
    const { colorInterpolationFilters } = extractColorInterpolation(
      this.props as Parameters<typeof extractColorInterpolation>[0]
    );

    const filterProps = {
      name: id,
      x,
      y,
      width,
      height,
      filterUnits,
      primitiveUnits,
    };
    return (
      <RNSVGFilter
        ref={(ref) => this.refMethod(ref as (Filter & NativeMethods) | null)}
        {...filterProps}
      >
        {inheritColorInterpolation(this.props.children, colorInterpolationFilters)}
      </RNSVGFilter>
    );
  }
}
