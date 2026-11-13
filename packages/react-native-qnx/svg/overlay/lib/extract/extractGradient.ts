// Replaces react-native-svg/src/lib/extract/extractGradient.ts on QNX.
//
// The package accepts `spreadMethod` on LinearGradient and RadialGradient in
// its types and never forwards it. The upstream function still builds the
// stops, units and transform; this adds the one prop it leaves out.
import upstreamExtractGradient from 'react-native-svg/src/lib/extract/extractGradient';

const spreadMethods: { [name: string]: number } = { pad: 0, reflect: 1, repeat: 2 };

type GradientProps = Parameters<typeof upstreamExtractGradient>[0] & {
  spreadMethod?: 'pad' | 'reflect' | 'repeat' | string;
};

export default function extractGradient(props: GradientProps, parent: unknown) {
  const extracted = upstreamExtractGradient(props, parent);
  if (extracted === null) return null;
  return { ...extracted, spreadMethod: spreadMethods[String(props.spreadMethod)] ?? 0 };
}
