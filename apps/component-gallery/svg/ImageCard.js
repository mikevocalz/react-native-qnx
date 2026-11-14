const React = require('react');
const Svg = require('react-native-svg').default;
const { Defs, ClipPath, Circle, Image, Rect, Text } = require('react-native-svg');
const { SvgCardFrame } = require('./SvgCardFrame');

// A 2x2 PNG (red, green, blue, white) as a data: URI.
const PIXELS =
  'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAIAAAACCAYAAABytg0kAAAAEklEQVR4nGP4z8DwHwyBNBgAAEnICff5q7YNAAAAAElFTkSuQmCC';
const REMOTE = 'https://images.unsplash.com/photo-1506744038136-46273834b3fb?w=640&q=80';
const BADGE =
  'data:image/svg+xml;utf8,' +
  encodeURIComponent(
    '<svg xmlns="http://www.w3.org/2000/svg" width="40" height="40"><circle cx="20" cy="20" r="18" fill="#eb5757"/></svg>'
  );

// Image from a data: URI, a bundled asset (require) and an SVG data URI, with
// preserveAspectRatio meet, slice and none, clipped and with opacity.
function ImageCard() {
  return (
    <SvgCardFrame
      title="Image"
      detail="Image href from data, bundled PNG, SVG document and HTTPS JPEG; preserveAspectRatio; clipPath; opacity"
    >
      <Svg width="100%" height={220} viewBox="0 0 560 220">
        <Defs>
          <ClipPath id="round">
            <Circle cx="75" cy="75" r="55" />
          </ClipPath>
        </Defs>
        <Rect x="10" y="10" width="120" height="60" fill="#142538" stroke="#e9f0f7" />
        <Image
          x="10"
          y="10"
          width="120"
          height="60"
          href={{ uri: PIXELS }}
          preserveAspectRatio="xMidYMid meet"
        />
        <Rect x="150" y="10" width="120" height="60" fill="#142538" stroke="#e9f0f7" />
        <Image
          x="150"
          y="10"
          width="120"
          height="60"
          href={{ uri: PIXELS }}
          preserveAspectRatio="xMidYMid slice"
        />
        <Image
          x="290"
          y="10"
          width="120"
          height="60"
          href={{ uri: PIXELS }}
          preserveAspectRatio="none"
        />
        <Image
          x="430"
          y="10"
          width="120"
          height="120"
          href={require('../assets/icon.png')}
          opacity="0.8"
        />
        <Image
          x="20"
          y="80"
          width="110"
          height="60"
          href={require('../assets/icon.png')}
          clipPath="url(#round)"
        />
        <Image x="290" y="85" width="50" height="50" href={{ uri: BADGE }} />
        <Text x="430" y="140" fontSize="11" fill="#9dafc2">
          HTTPS JPEG
        </Text>
        <Image
          x="430"
          y="150"
          width="120"
          height="60"
          href={{ uri: REMOTE }}
          preserveAspectRatio="xMidYMid slice"
        />
      </Svg>
    </SvgCardFrame>
  );
}

module.exports = { id: 'svg-image', title: 'Image', Component: ImageCard };
