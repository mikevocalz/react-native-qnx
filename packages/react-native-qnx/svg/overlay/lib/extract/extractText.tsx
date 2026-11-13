// Replaces react-native-svg/src/lib/extract/extractText.tsx on QNX.
//
// Text accepts `textLength` and `lengthAdjust` in its types, its native
// component declares both, and extractText drops them, so they never reach a
// host. The upstream function still builds the font, the position lists and
// the children; this adds the two props it leaves out.
import upstreamExtractText, {
  extractFont,
  setTSpan,
} from 'react-native-svg/src/lib/extract/extractText';

export { extractFont, setTSpan };
export type { TextChild, TextProps } from 'react-native-svg/src/lib/extract/extractText';

type ExtractTextArguments = Parameters<typeof upstreamExtractText>;
type TextLengthProps = ExtractTextArguments[0] & {
  textLength?: number | string;
  lengthAdjust?: 'spacing' | 'spacingAndGlyphs' | string;
};

export default function extractText(props: TextLengthProps, container: ExtractTextArguments[1]) {
  const extracted = upstreamExtractText(props, container);
  const { textLength, lengthAdjust } = props;
  return {
    ...extracted,
    ...(textLength != null ? { textLength } : null),
    // The native prop is a free string; only the two SVG keywords mean anything.
    ...(lengthAdjust === 'spacing' || lengthAdjust === 'spacingAndGlyphs' ? { lengthAdjust } : null),
  };
}
