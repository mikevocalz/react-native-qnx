// Replaces react-native-svg/src/xmlTags.ts on QNX.
//
// SvgXml, SvgUri, SvgCss and friends build elements from this table. The
// package has no entries for feFuncR, feFuncG, feFuncB or feFuncA, so those
// tags became null and a <feComponentTransfer> from an SVG file lost its
// transfer functions.
import { tags as upstreamTags } from 'react-native-svg/src/xmlTags';
import {
  FeFuncA,
  FeFuncB,
  FeFuncG,
  FeFuncR,
} from 'react-native-svg/src/elements/filters/FeComponentTransferFunction';

export const tags = {
  ...upstreamTags,
  feFuncA: FeFuncA,
  feFuncB: FeFuncB,
  feFuncG: FeFuncG,
  feFuncR: FeFuncR,
} as const;
