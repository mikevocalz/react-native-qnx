import extractBrush from 'react-native-svg/src/lib/extract/extractBrush';
import extractOpacity from 'react-native-svg/src/lib/extract/extractOpacity';
import { alignEnum, meetOrSliceTypes } from 'react-native-svg/src/lib/extract/extractViewBox';
import { resolveAssetUri } from 'react-native-svg/src/lib/resolveAssetUri';
import type { ReactNode } from 'react';
import { extractLightSource } from './lightSource';
import type { NumberArray, NumberProp } from './numbers';
import { oneOf, toNumber, toNumberList, toPair } from './numbers';
import { extractTransferFunctions } from './transferFunctions';

// Props that arrive on the element. Each extractor returns the native props
// the matching spec in ../fabric declares, shaped the way the spec types them.

const spaces = /\s+/;
const channels = ['R', 'G', 'B', 'A'] as const;
const edgeModes = ['duplicate', 'wrap', 'none'] as const;

type Brush = ReturnType<typeof extractBrush>;
type StyleBag = { [name: string]: unknown } | undefined;

function fromStyle(props: { style?: unknown }, name: string): unknown {
  const style = props.style as StyleBag;
  return style && typeof style === 'object' ? style[name] : undefined;
}

/**
 * A presentation property, from `style` when it sets it and from the attribute
 * otherwise: CSS (a style attribute, or a <style> rule SvgCss copied onto the
 * element) overrides the attribute, and a component's defaultProps look like
 * attributes. The package's own Fe elements ignore `style`; Inkscape writes
 * `style="flood-color:#000;flood-opacity:.5"`.
 */
export function presentation(
  props: { style?: unknown },
  name: string,
  attribute: unknown
): unknown {
  const styled = fromStyle(props, name);
  return styled === undefined ? attribute : styled;
}

/** A color prop as the `{ type, payload }` brush struct, or undefined when unset. */
export function extractColorBrush(value: unknown): Brush | undefined {
  if (value === undefined || value === null || value === '') return undefined;
  return extractBrush(value as string) ?? undefined;
}

export const extractMorphology = (props: { operator?: unknown; radius?: NumberArray }) => {
  const [radiusX, radiusY] = toPair(props.radius, 0);
  return {
    operator1: oneOf(props.operator, ['erode', 'dilate'] as const),
    radiusX,
    radiusY,
  };
};

export const extractTurbulence = (props: {
  baseFrequency?: NumberArray;
  numOctaves?: NumberProp;
  seed?: NumberProp;
  stitchTiles?: unknown;
  type?: unknown;
}) => {
  const [baseFrequencyX, baseFrequencyY] = toPair(props.baseFrequency, 0);
  return {
    baseFrequencyX,
    baseFrequencyY,
    numOctaves: Math.trunc(toNumber(props.numOctaves, 1)),
    seed: toNumber(props.seed, 0),
    stitchTiles: oneOf(props.stitchTiles, ['stitch', 'noStitch'] as const),
    type: oneOf(props.type, ['fractalNoise', 'turbulence'] as const),
  };
};

export const extractDisplacementMap = (props: {
  in2?: string | number;
  scale?: NumberProp;
  xChannelSelector?: unknown;
  yChannelSelector?: unknown;
}) => ({
  in2: props.in2 === undefined || props.in2 === null ? '' : String(props.in2),
  scale: toNumber(props.scale, 0),
  xChannelSelector: oneOf(props.xChannelSelector, channels),
  yChannelSelector: oneOf(props.yChannelSelector, channels),
});

export const extractComponentTransfer = (props: { children?: ReactNode }) =>
  extractTransferFunctions(props.children);

export const extractDropShadow = (props: {
  stdDeviation?: NumberArray;
  dx?: NumberProp;
  dy?: NumberProp;
  floodColor?: unknown;
  floodOpacity?: NumberProp;
  style?: unknown;
}) => {
  const [stdDeviationX, stdDeviationY] = toPair(props.stdDeviation, 2);
  const color = presentation(props, 'floodColor', props.floodColor);
  const opacity = presentation(props, 'floodOpacity', props.floodOpacity) as NumberProp | undefined;
  return {
    // SVG's feDropShadow defaults, unlike feOffset's zero.
    dx: props.dx ?? 2,
    dy: props.dy ?? 2,
    stdDeviationX,
    stdDeviationY,
    floodColor: extractColorBrush(color),
    floodOpacity: opacity === undefined ? undefined : extractOpacity(opacity),
  };
};

type HrefSource = string | number | { uri?: string } | undefined;

export const extractImage = (props: {
  href?: HrefSource;
  xlinkHref?: HrefSource;
  preserveAspectRatio?: string;
}) => {
  const source = props.href ?? props.xlinkHref;
  const href =
    typeof source === 'string'
      ? source
      : source === undefined || source === null
        ? undefined
        : resolveAssetUri(source)?.uri;
  const modes = props.preserveAspectRatio ? props.preserveAspectRatio.trim().split(spaces) : [];
  return {
    href,
    align: alignEnum[modes[0]] || 'xMidYMid',
    meetOrSlice: meetOrSliceTypes[modes[1]] || 0,
  };
};

export const extractConvolveMatrix = (props: {
  order?: NumberArray;
  kernelMatrix?: NumberArray;
  divisor?: NumberProp;
  bias?: NumberProp;
  targetX?: NumberProp;
  targetY?: NumberProp;
  edgeMode?: unknown;
  preserveAlpha?: boolean | 'true' | 'false';
}) => {
  const [orderX, orderY] = toPair(props.order, 3);
  return {
    orderX: Math.trunc(orderX),
    orderY: Math.trunc(orderY),
    kernelMatrix: toNumberList(props.kernelMatrix),
    divisor: toNumber(props.divisor, 0),
    bias: toNumber(props.bias, 0),
    targetX: Math.trunc(toNumber(props.targetX, -1)),
    targetY: Math.trunc(toNumber(props.targetY, -1)),
    edgeMode: oneOf(props.edgeMode, edgeModes),
    preserveAlpha: props.preserveAlpha === true || props.preserveAlpha === 'true',
  };
};

type LightingProps = {
  surfaceScale?: NumberProp;
  lightingColor?: unknown;
  children?: ReactNode;
  style?: unknown;
};

const extractLighting = (props: LightingProps) => ({
  surfaceScale: toNumber(props.surfaceScale, 1),
  lightingColor: extractColorBrush(presentation(props, 'lightingColor', props.lightingColor)),
  light: extractLightSource(props.children),
});

export const extractDiffuseLighting = (
  props: LightingProps & { diffuseConstant?: NumberProp }
) => ({
  ...extractLighting(props),
  diffuseConstant: toNumber(props.diffuseConstant, 1),
});

export const extractSpecularLighting = (
  props: LightingProps & { specularConstant?: NumberProp; specularExponent?: NumberProp }
) => ({
  ...extractLighting(props),
  specularConstant: toNumber(props.specularConstant, 1),
  specularExponent: toNumber(props.specularExponent, 1),
});

/** `color-interpolation-filters`, as a prop or in `style`. `auto` leaves it to the <filter>. */
export const extractColorInterpolation = (props: {
  colorInterpolationFilters?: unknown;
  style?: unknown;
}) => ({
  colorInterpolationFilters: oneOf(
    presentation(props, 'colorInterpolationFilters', props.colorInterpolationFilters),
    ['auto', 'sRGB', 'linearRGB'] as const
  ),
});
