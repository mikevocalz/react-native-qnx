// Replaces react-native-svg/src/lib/extract/extractFilter.ts on QNX.
//
// Everything the package exports stays as it is, with two corrections to the
// functions the package's own primitives call.
//
// - `extractFilter`, which seven primitives spread into their native props,
//   also forwards color-interpolation-filters, which the package drops. Codegen
//   adds that prop to the primitives' Fabric specs.
// - feFlood reads flood-color and flood-opacity from `style` as well as from
//   attributes. The package's FeFlood has defaultProps for both, so a style
//   value could never reach it.
// - Names (`result`, `in`, `in2`, merge nodes) are always strings. SvgXml turns
//   every numeric-looking attribute into a number, so `result="1"` arrived as
//   the number 1, which a string prop cannot take. (`in="0"` on feComposite and
//   feMergeNode is still read as unset: the package's own functions drop a
//   falsy name before these wrappers see it.)
import upstreamExtractFeFlood, {
  extractFeBlend as upstreamExtractFeBlend,
  extractFeComposite as upstreamExtractFeComposite,
  extractFeMerge as upstreamExtractFeMerge,
  extractFilter as upstreamExtractFilter,
} from 'react-native-svg/src/lib/extract/extractFilter';
import { extractColorInterpolation, presentation } from '../../../extract/filterExtras';

export * from 'react-native-svg/src/lib/extract/extractFilter';

const name = (value: unknown) => (value === undefined || value === null ? '' : String(value));

export const extractFilter = (
  props: Parameters<typeof upstreamExtractFilter>[0] & {
    colorInterpolationFilters?: unknown;
    style?: unknown;
  }
) => {
  const extracted = upstreamExtractFilter(props);
  return {
    ...extracted,
    ...(extracted.result === undefined ? {} : { result: name(extracted.result) }),
    ...extractColorInterpolation(props),
  };
};

export const extractIn = (props: { in?: string | number }) =>
  props.in === undefined || props.in === null || props.in === '' ? {} : { in1: name(props.in) };

export const extractFeBlend = (props: Parameters<typeof upstreamExtractFeBlend>[0]) => {
  const extracted = upstreamExtractFeBlend(props);
  return extracted.in2 === undefined ? extracted : { ...extracted, in2: name(extracted.in2) };
};

export const extractFeComposite = (props: Parameters<typeof upstreamExtractFeComposite>[0]) => {
  const extracted = upstreamExtractFeComposite(props);
  return { ...extracted, in1: name(extracted.in1), in2: name(extracted.in2) };
};

export const extractFeMerge = (
  props: Parameters<typeof upstreamExtractFeMerge>[0],
  parent: unknown
) => {
  const extracted = upstreamExtractFeMerge(props, parent);
  return { ...extracted, nodes: (extracted.nodes ?? []).map(name) };
};

export default function extractFeFlood(
  props: Parameters<typeof upstreamExtractFeFlood>[0] & { style?: unknown }
) {
  return upstreamExtractFeFlood({
    ...props,
    floodColor: presentation(props, 'floodColor', props.floodColor) as typeof props.floodColor,
    floodOpacity: presentation(
      props,
      'floodOpacity',
      props.floodOpacity
    ) as typeof props.floodOpacity,
  });
}
