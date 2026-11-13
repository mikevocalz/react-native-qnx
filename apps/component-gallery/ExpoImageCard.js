// expo-image on QNX: the same <Image> API as iOS and Android, drawn by the
// host's native ExpoImage view on the shared Qt image loader and decoded
// cache. Pick a source (bundled file or URL), a contentFit, and watch the
// placeholder cross-dissolve into the image.
import * as React from 'react';
import { Pressable, Text, View } from 'react-native';
import { Image } from 'expo-image';

const INK = '#e9f0f7';
const MUTED = '#9dafc2';
const ACCENT = '#79e8dc';
const SELECTED = '#1b5b61';

const SOURCES = [
  { id: 'local', label: 'Local file', source: require('./assets/icon.png') },
  {
    id: 'url',
    label: 'URL',
    source: { uri: 'https://images.unsplash.com/photo-1506744038136-46273834b3fb?w=640&q=80' },
  },
  { id: 'broken', label: 'Broken', source: { uri: 'https://rnqnx-missing.invalid/photo.jpg' } },
];
const FITS = ['cover', 'contain', 'fill', 'none'];

function Chip({ title, selected, onPress }) {
  return (
    <Pressable
      accessibilityRole="button"
      accessibilityLabel={title}
      onPress={onPress}
      style={{
        minHeight: 30,
        paddingHorizontal: 12,
        justifyContent: 'center',
        borderRadius: 8,
        marginRight: 6,
        marginBottom: 6,
        backgroundColor: selected ? SELECTED : 'rgba(255,255,255,0.06)',
      }}
    >
      <Text style={{ color: selected ? '#f2ffff' : MUTED, fontSize: 13 }}>{title}</Text>
    </Pressable>
  );
}

export default function ExpoImageCard() {
  const [sourceId, setSourceId] = React.useState('url');
  const [fit, setFit] = React.useState('cover');
  const [status, setStatus] = React.useState('Not loaded');
  const current = SOURCES.find((candidate) => candidate.id === sourceId) ?? SOURCES[0];
  return (
    <View>
      <View style={{ flexDirection: 'row', flexWrap: 'wrap' }}>
        {SOURCES.map((candidate) => (
          <Chip
            key={candidate.id}
            title={candidate.label}
            selected={candidate.id === sourceId}
            onPress={() => {
              setStatus('Loading');
              setSourceId(candidate.id);
            }}
          />
        ))}
      </View>
      <View style={{ flexDirection: 'row', flexWrap: 'wrap' }}>
        {FITS.map((candidate) => (
          <Chip
            key={candidate}
            title={candidate}
            selected={candidate === fit}
            onPress={() => setFit(candidate)}
          />
        ))}
      </View>
      <Image
        key={sourceId}
        source={current.source}
        contentFit={fit}
        transition={300}
        placeholder={{ uri: 'https://rnqnx-placeholder.invalid/p.png' }}
        accessibilityLabel={'expo-image ' + current.label}
        onLoadStart={() => setStatus('Loading')}
        onLoad={(event) =>
          setStatus(
            'Loaded ' +
              event.source?.width +
              'x' +
              event.source?.height +
              ' (' +
              event.cacheType +
              ')'
          )
        }
        onError={(event) => setStatus('Error: ' + event.error)}
        style={{ width: '100%', height: 150, borderRadius: 10, backgroundColor: '#0c1929' }}
      />
      <Text style={{ color: ACCENT, fontSize: 13, lineHeight: 20, marginTop: 8 }}>{status}</Text>
      <Text style={{ color: INK, fontSize: 12, lineHeight: 18 }}>
        contentFit {fit}, transition 300 ms
      </Text>
    </View>
  );
}
