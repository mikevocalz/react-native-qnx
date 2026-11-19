const React = require('react');
const { View, Text, Pressable } = require('react-native');
const { Image } = require('expo-image');
const { useStore } = require('zustand');
const { createStore } = require('zustand/vanilla');

const LOCAL = require('../assets/icon.png');
const GIF = require('../assets/anim.gif');
const REMOTE = 'https://images.unsplash.com/photo-1506744038136-46273834b3fb?w=640&q=80';
const INK = '#e9f0f7';

function Action({ title, onPress }) {
  return (
    <Pressable
      onPress={onPress}
      accessibilityLabel={title}
      style={{ backgroundColor: '#1b5b61', padding: 9, marginRight: 6, marginBottom: 6 }}
    >
      <Text style={{ color: INK, fontSize: 13 }}>{title}</Text>
    </Pressable>
  );
}

function ImageProofCard() {
  const store = React.useMemo(
    () =>
      createStore(() => ({
        size: 20,
        animated: false,
        loads: 0,
        status: 'Loading',
        lines: [],
        refRevision: 0,
      })),
    []
  );
  const size = useStore(store, (state) => state.size);
  const animated = useStore(store, (state) => state.animated);
  const loads = useStore(store, (state) => state.loads);
  const status = useStore(store, (state) => state.status);
  const lines = useStore(store, (state) => state.lines);
  useStore(store, (state) => state.refRevision);
  // View handles and ImageRef resources stay outside serialized application state.
  const view = React.useRef(null);
  const loaded = React.useRef(null);
  const generation = React.useRef(0);
  React.useEffect(
    () => () => {
      generation.current += 1;
      loaded.current?.release();
      loaded.current = null;
    },
    []
  );
  const append = React.useCallback(
    (line) => {
      store.setState((state) => ({ lines: [...state.lines, line] }));
      globalThis.__rnqnxLog?.('IMAGE-PROOF ' + line);
    },
    [store]
  );
  const onLoad = React.useCallback(
    (event) => {
      store.setState((state) => ({
        loads: state.loads + 1,
        status: `${event.source?.width}x${event.source?.height}, cache ${event.cacheType}`,
      }));
      globalThis.__rnqnxLog?.(
        'IMAGE-PROOF load ' +
          JSON.stringify({
            count: store.getState().loads,
            view: store.getState().size,
            sourceWidth: event.source?.width,
            sourceHeight: event.source?.height,
            cache: event.cacheType,
          })
      );
    },
    [store]
  );
  const runApis = React.useCallback(async () => {
    const token = ++generation.current;
    const report = (line) => {
      if (token === generation.current) append(line);
    };
    store.setState({ lines: [] });
    try {
      const memory = await Image.clearMemoryCache();
      if (token !== generation.current) return;
      report('clearMemoryCache: ' + memory);
      if (!memory) throw new Error('Memory cache clear failed');
      const disk = await Image.clearDiskCache();
      if (token !== generation.current) return;
      report('clearDiskCache: ' + disk);
      if (!disk) throw new Error('Disk cache clear failed');
      const prefetched = await Image.prefetch(REMOTE, { cachePolicy: 'disk' });
      if (token !== generation.current) return;
      report('prefetch disk: ' + prefetched);
      if (!prefetched) throw new Error('Prefetch failed');
      const cached = await Image.getCachePathAsync(REMOTE);
      if (token !== generation.current) return;
      report('cache path present: ' + Boolean(cached));
      if (!cached) throw new Error('Prefetched image is absent from disk');
      const next = await Image.loadAsync(LOCAL);
      if (token !== generation.current) {
        next.release();
        return;
      }
      loaded.current?.release();
      loaded.current = next;
      store.setState((state) => ({ refRevision: state.refRevision + 1 }));
      report(`loadAsync: ${next.width}x${next.height}`);
      if (!(next.width > 0 && next.height > 0)) throw new Error('Invalid ImageRef dimensions');
      const hash = await Image.generateBlurhashAsync(next, [4, 3]);
      if (token !== generation.current) return;
      report('blurhash length: ' + (hash?.length ?? 0));
      if (hash?.length !== 28) throw new Error('Invalid 4x3 blurhash');
      report('API-PROOF-COMPLETE');
    } catch (error) {
      report('API error: ' + String(error));
    }
  }, [store, append]);
  const playback = React.useCallback(
    async (start) => {
      try {
        if (start) await view.current?.startAnimating();
        else await view.current?.stopAnimating();
        append(start ? 'startAnimating dispatched' : 'stopAnimating dispatched');
      } catch (error) {
        append('Playback error: ' + String(error));
      }
    },
    [append]
  );
  return (
    <View>
      <View style={{ flexDirection: 'row', flexWrap: 'wrap' }}>
        <Action title="Grow to 240" onPress={() => store.setState({ size: 240 })} />
        <Action
          title="Use still / GIF"
          onPress={() => store.setState((state) => ({ animated: !state.animated, loads: 0 }))}
        />
        <Action title="Start animation" onPress={() => playback(true)} />
        <Action title="Stop animation" onPress={() => playback(false)} />
        <Action title="Run cache / ref APIs" onPress={runApis} />
      </View>
      <Image
        ref={view}
        key={animated ? 'gif' : 'still'}
        source={animated ? GIF : LOCAL}
        autoplay={false}
        allowDownscaling
        cachePolicy="none"
        contentFit="cover"
        priority="high"
        onLoad={onLoad}
        onError={(event) => store.setState({ status: 'Error: ' + event.error })}
        style={{ width: animated ? 160 : size, height: animated ? 160 : size }}
      />
      <Text style={{ color: INK, fontSize: 13 }}>
        View {size}; loads {loads}; {status}
      </Text>
      {loaded.current ? <Image source={loaded.current} style={{ width: 80, height: 80 }} /> : null}
      {lines.map((line, index) => (
        <Text key={index} style={{ color: '#79e8dc', fontSize: 12 }}>
          {line}
        </Text>
      ))}
    </View>
  );
}

module.exports = { ImageProofCard };
