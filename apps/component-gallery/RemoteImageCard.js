// Remote images by URL. Each choice loads through Qt Network on the device:
// HTTPS uses Qt's OpenSSL TLS backend and the system CA store, HTTP is plain.
// The size line comes from Image.getSize (React Native's ImageLoader), so one
// card exercises both the <Image> view and the loader module.
import * as React from 'react';
import {
  ActivityIndicator,
  DevSettings,
  Image,
  Pressable,
  Text,
  TextInput,
  View,
} from 'react-native';

const SOURCES = [
  {
    id: 'https',
    label: 'HTTPS',
    uri: 'https://images.unsplash.com/photo-1506744038136-46273834b3fb?w=640&q=80',
  },
  { id: 'http', label: 'HTTP', uri: 'http://httpbin.org/image/jpeg' },
  { id: 'broken', label: 'Broken', uri: 'https://rnqnx-missing.invalid/photo.jpg' },
  { id: 'mac-http', label: 'Mac HTTP', uri: 'http://192.168.2.1:8090/mac-served.jpg' },
  { id: 'mac-https', label: 'Mac HTTPS', uri: 'https://192.168.2.1:8443/mac-served.jpg' },
];

const INK = '#e9f0f7';
const MUTED = '#9dafc2';
const ACCENT = '#79e8dc';
const ERROR = '#fca5a5';

function Choice({ label, selected, onPress }) {
  return (
    <Pressable
      onPress={onPress}
      accessibilityRole="button"
      accessibilityLabel={'Load ' + label + ' image'}
      style={{
        minHeight: 34,
        paddingHorizontal: 14,
        justifyContent: 'center',
        borderRadius: 8,
        marginRight: 6,
        backgroundColor: selected ? '#1b5b61' : '#0c1929',
      }}
    >
      <Text style={{ color: selected ? '#f2ffff' : MUTED, fontSize: 14 }}>{label}</Text>
    </Pressable>
  );
}

export default function RemoteImageCard() {
  const [uri, setUri] = React.useState(SOURCES[0].uri);
  const [draft, setDraft] = React.useState(SOURCES[0].uri);
  const [attempt, setAttempt] = React.useState(0);
  const [state, setState] = React.useState({ phase: 'loading' });
  const startedAt = React.useRef(Date.now());

  const load = React.useCallback((next) => {
    setUri(next);
    setDraft(next);
    setAttempt((value) => value + 1);
  }, []);

  // Keyboard path in development builds (Ctrl+D): step through the sources.
  React.useEffect(() => {
    DevSettings.addMenuItem('Image from a URL: next source', () =>
      setUri((current) => {
        const index = SOURCES.findIndex((source) => source.uri === current);
        const next = SOURCES[(index + 1) % SOURCES.length].uri;
        setDraft(next);
        setAttempt((value) => value + 1);
        return next;
      })
    );
  }, []);

  React.useEffect(() => {
    startedAt.current = Date.now();
    setState({ phase: 'loading' });
  }, [uri, attempt]);

  const onLoad = () => {
    const ms = Date.now() - startedAt.current;
    console.log('[RemoteImageCard] loaded ' + uri + ' in ' + ms + ' ms');
    setState({ phase: 'loaded', ms });
    Image.getSize(uri)
      .then(({ width, height }) => {
        console.log('[RemoteImageCard] Image.getSize ' + uri + ' = ' + width + 'x' + height);
        setState((current) =>
          current.phase === 'loaded' ? { ...current, width, height } : current
        );
      })
      .catch(() => {});
  };
  const onError = (event) => {
    const message = event?.nativeEvent?.error || 'The image could not be loaded.';
    console.log('[RemoteImageCard] failed ' + uri + ': ' + message);
    setState({ phase: 'error', message: String(message) });
    // The loader module reports Qt Network's reason (DNS, TLS, HTTP status).
    Image.getSize(uri).catch((error) => {
      const reason = error && error.message ? error.message : String(error);
      console.log('[RemoteImageCard] reason ' + uri + ': ' + reason);
      setState((current) =>
        current.phase === 'error' ? { ...current, message: reason } : current
      );
    });
  };

  const scheme = uri.split(':')[0].toUpperCase();
  let status;
  if (state.phase === 'loading') {
    status = (
      <View style={{ flexDirection: 'row', alignItems: 'center' }}>
        <ActivityIndicator size="small" color={ACCENT} />
        <Text style={{ color: MUTED, fontSize: 14, marginLeft: 10 }}>
          {'Loading over ' + scheme + '…'}
        </Text>
      </View>
    );
  } else if (state.phase === 'loaded') {
    const size = state.width ? state.width + '×' + state.height + ' px, ' : '';
    status = (
      <Text style={{ color: ACCENT, fontSize: 14 }} numberOfLines={1}>
        {'Loaded over ' + scheme + ' · ' + size + state.ms + ' ms'}
      </Text>
    );
  } else {
    status = (
      <Text style={{ color: ERROR, fontSize: 14 }} numberOfLines={2}>
        {'Failed over ' + scheme + ': ' + state.message}
      </Text>
    );
  }

  return (
    <View>
      <View style={{ flexDirection: 'row', flexWrap: 'wrap', marginBottom: 10 }}>
        {SOURCES.map((source) => (
          <Choice
            key={source.id}
            label={source.label}
            selected={uri === source.uri}
            onPress={() => load(source.uri)}
          />
        ))}
      </View>
      <View
        style={{
          height: 150,
          borderRadius: 10,
          overflow: 'hidden',
          backgroundColor: '#0c1929',
          marginBottom: 10,
          alignItems: 'center',
          justifyContent: 'center',
        }}
      >
        {state.phase === 'error' ? <Text style={{ color: ERROR, fontSize: 32 }}>×</Text> : null}
        <Image
          key={uri + '#' + attempt}
          source={{ uri }}
          resizeMode="cover"
          accessibilityLabel="Remote image"
          onLoad={onLoad}
          onError={onError}
          style={{
            position: 'absolute',
            left: 0,
            top: 0,
            right: 0,
            bottom: 0,
            opacity: state.phase === 'loaded' ? 1 : 0,
          }}
        />
      </View>
      <View style={{ minHeight: 40, justifyContent: 'center' }}>{status}</View>
      <View style={{ flexDirection: 'row', alignItems: 'center', marginTop: 8 }}>
        <TextInput
          value={draft}
          onChangeText={setDraft}
          onSubmitEditing={() => load(draft.trim())}
          autoCapitalize="none"
          autoCorrect={false}
          accessibilityLabel="Image URL"
          style={{
            flex: 1,
            height: 40,
            paddingHorizontal: 10,
            fontSize: 13,
            color: INK,
            backgroundColor: '#0c1929',
            borderRadius: 8,
            marginRight: 8,
          }}
        />
        <Choice label="Load URL" selected={false} onPress={() => load(draft.trim())} />
      </View>
    </View>
  );
}
