/**
 * Device probe for the device-parity gate (ADR-0023, Pi 400). It exercises the
 * public-entry components that had no device disposition: VirtualizedList,
 * SectionList, VirtualizedSectionList, Button, TouchableOpacity,
 * TouchableHighlight, TouchableWithoutFeedback, ImageBackground,
 * KeyboardAvoidingView, RefreshControl, SafeAreaView and StatusBar.
 *
 * Every observation is one `RNQNX_COMPONENT_PROBE <ms> <component> <event>`
 * console line, so a receipt quotes the log and not a screenshot. Touch targets
 * log their window rectangle once (`layout`), and the driver clicks the centre
 * of that rectangle with rnqnx-inject. Lists are scrolled by the driver with
 * wheel ticks, and also by `scrollToOffset` so the list path is checked with
 * and without a pointer. The probe asserts nothing: a failed press is a missing
 * log line.
 */
import React from 'react';
import {
  AppRegistry,
  Button,
  FlatList,
  ImageBackground,
  KeyboardAvoidingView,
  Pressable,
  RefreshControl,
  SafeAreaView,
  ScrollView,
  SectionList,
  StatusBar,
  Text,
  TouchableHighlight,
  TouchableOpacity,
  TouchableWithoutFeedback,
  View,
  VirtualizedList,
  VirtualizedSectionList,
} from 'react-native';

const T0 = Date.now();
function log(component, event) {
  console.log(`RNQNX_COMPONENT_PROBE ${Date.now() - T0} ${component} ${event}`);
}

const PNG =
  'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR4nGMwTpv5HwAENAIyWy0K4AAAAABJRU5ErkJggg==';

/** Log the window rectangle of a view once it has laid out. */
function useRect(name) {
  const ref = React.useRef(null);
  const onLayout = React.useCallback(() => {
    requestAnimationFrame(() => {
      ref.current?.measureInWindow?.((x, y, w, h) => {
        log(
          name,
          `layout x=${Math.round(x)} y=${Math.round(y)} w=${Math.round(w)} h=${Math.round(h)}`
        );
      });
    });
  }, [name]);
  return { ref, onLayout };
}

function Tile({ name, children, style }) {
  const rect = useRect(name);
  return (
    <View ref={rect.ref} onLayout={rect.onLayout} collapsable={false} style={style}>
      {children}
    </View>
  );
}

const btn = {
  padding: 10,
  backgroundColor: '#2b6cb0',
  alignItems: 'center',
  margin: 4,
  width: 190,
};
const label = { color: '#fff', fontSize: 16 };

function TouchPage() {
  const [counts, setCounts] = React.useState({});
  const bump = (name) => setCounts((c) => ({ ...c, [name]: (c[name] || 0) + 1 }));
  const event = (component, name) => () => {
    log(component, name);
    if (name === 'onPress') bump(component);
  };
  const [refreshing, setRefreshing] = React.useState(false);
  React.useEffect(() => {
    StatusBar.setBarStyle?.('light-content');
    StatusBar.setHidden?.(false);
    StatusBar.setBackgroundColor?.('#000000');
    log('StatusBar', `static calls returned currentHeight=${String(StatusBar.currentHeight)}`);
  }, []);
  return (
    <ScrollView
      contentContainerStyle={{ padding: 8 }}
      refreshControl={
        <RefreshControl
          refreshing={refreshing}
          onRefresh={() => {
            log('RefreshControl', 'onRefresh');
            setRefreshing(true);
          }}
        />
      }
    >
      <View style={{ flexDirection: 'row', flexWrap: 'wrap' }}>
        <Tile name="Button" style={{ margin: 4 }}>
          <Button title={`Button ${counts.Button || 0}`} onPress={event('Button', 'onPress')} />
        </Tile>
        <Tile name="TouchableOpacity" style={btn}>
          <TouchableOpacity
            onPress={event('TouchableOpacity', 'onPress')}
            onPressIn={event('TouchableOpacity', 'onPressIn')}
            onPressOut={event('TouchableOpacity', 'onPressOut')}
            onLongPress={event('TouchableOpacity', 'onLongPress')}
            activeOpacity={0.4}
          >
            <Text style={label}>{`Opacity ${counts.TouchableOpacity || 0}`}</Text>
          </TouchableOpacity>
        </Tile>
        <Tile name="TouchableHighlight" style={btn}>
          <TouchableHighlight
            underlayColor="#e53e3e"
            onPress={event('TouchableHighlight', 'onPress')}
            onPressIn={event('TouchableHighlight', 'onPressIn')}
            onPressOut={event('TouchableHighlight', 'onPressOut')}
            onShowUnderlay={event('TouchableHighlight', 'onShowUnderlay')}
            onHideUnderlay={event('TouchableHighlight', 'onHideUnderlay')}
          >
            <Text style={label}>{`Highlight ${counts.TouchableHighlight || 0}`}</Text>
          </TouchableHighlight>
        </Tile>
        <Tile name="TouchableWithoutFeedback" style={btn}>
          <TouchableWithoutFeedback
            onPress={event('TouchableWithoutFeedback', 'onPress')}
            onPressIn={event('TouchableWithoutFeedback', 'onPressIn')}
            onPressOut={event('TouchableWithoutFeedback', 'onPressOut')}
          >
            <View>
              <Text style={label}>{`NoFeedback ${counts.TouchableWithoutFeedback || 0}`}</Text>
            </View>
          </TouchableWithoutFeedback>
        </Tile>
      </View>
      <Tile name="ImageBackground" style={{ width: 360, height: 90, margin: 4 }}>
        <ImageBackground
          source={{ uri: PNG }}
          resizeMode="stretch"
          style={{ width: 360, height: 90, backgroundColor: '#444' }}
          onLoad={() => log('ImageBackground', 'onLoad')}
          onError={() => log('ImageBackground', 'onError')}
          imageStyle={{ opacity: 0.9 }}
        >
          <Text style={label} onLayout={() => log('ImageBackground', 'child laid out over image')}>
            Child over ImageBackground
          </Text>
        </ImageBackground>
      </Tile>
      <KeyboardAvoidingView
        behavior="padding"
        keyboardVerticalOffset={12}
        enabled
        onLayout={() => log('KeyboardAvoidingView', 'onLayout children rendered')}
        style={{ margin: 4, padding: 6, backgroundColor: '#1a365d' }}
      >
        <Text style={label}>KeyboardAvoidingView child</Text>
      </KeyboardAvoidingView>
      <SafeAreaView
        onLayout={(e) => {
          const l = e.nativeEvent.layout;
          log('SafeAreaView', `onLayout w=${Math.round(l.width)} h=${Math.round(l.height)}`);
        }}
        style={{ margin: 4, padding: 6, backgroundColor: '#22543d' }}
      >
        <Text style={label}>SafeAreaView child</Text>
      </SafeAreaView>
      <Text style={label}>
        RefreshControl: wheel up at the top; the log shows onRefresh if it fires.
      </Text>
    </ScrollView>
  );
}

const ITEMS = Array.from({ length: 300 }, (_, i) => ({ id: `r${i}`, title: `Row ${i}` }));
const SECTIONS = Array.from({ length: 12 }, (_, s) => ({
  title: `Section ${s}`,
  data: Array.from({ length: 25 }, (_, i) => ({ id: `s${s}-${i}`, title: `Item ${s}.${i}` })),
}));

const rowStyle = {
  height: 44,
  justifyContent: 'center',
  paddingHorizontal: 12,
  borderBottomWidth: 1,
  borderColor: '#2d3748',
};
const headerStyle = {
  height: 36,
  justifyContent: 'center',
  paddingHorizontal: 12,
  backgroundColor: '#2c5282',
};

function viewable(name) {
  return ({ viewableItems }) => {
    const keys = viewableItems.map((v) => v.key);
    log(name, `viewable first=${keys[0]} last=${keys[keys.length - 1]} count=${keys.length}`);
  };
}

function VirtualizedListPage() {
  const ref = React.useRef(null);
  const onViewableItemsChanged = React.useRef(viewable('VirtualizedList')).current;
  const [rendered] = React.useState({ max: 0 });
  React.useEffect(() => {
    const t = setTimeout(() => {
      log('VirtualizedList', 'scrollToIndex 150 start');
      ref.current?.scrollToIndex?.({ index: 150, animated: false });
    }, 1500);
    return () => clearTimeout(t);
  }, []);
  return (
    <VirtualizedList
      ref={ref}
      data={ITEMS}
      keyExtractor={(item) => item.id}
      getItemCount={(data) => data.length}
      getItem={(data, index) => data[index]}
      getItemLayout={(_, index) => ({ length: 44, offset: 44 * index, index })}
      initialNumToRender={12}
      windowSize={5}
      maxToRenderPerBatch={8}
      onViewableItemsChanged={onViewableItemsChanged}
      onScroll={(e) =>
        log('VirtualizedList', `onScroll y=${Math.round(e.nativeEvent.contentOffset.y)}`)
      }
      scrollEventThrottle={100}
      onEndReachedThreshold={0.5}
      onEndReached={() => log('VirtualizedList', 'onEndReached')}
      renderItem={({ item, index }) => {
        if (index > rendered.max) {
          rendered.max = index;
          if (index % 25 === 0) log('VirtualizedList', `rendered up to ${index}`);
        }
        return (
          <View style={rowStyle}>
            <Text style={label}>{item.title}</Text>
          </View>
        );
      }}
    />
  );
}

function SectionListPage() {
  const ref = React.useRef(null);
  const onViewableItemsChanged = React.useRef(viewable('SectionList')).current;
  React.useEffect(() => {
    const t = setTimeout(() => {
      log('SectionList', 'scrollToLocation section 6 start');
      ref.current?.scrollToLocation?.({ sectionIndex: 6, itemIndex: 0, animated: false });
    }, 1500);
    return () => clearTimeout(t);
  }, []);
  return (
    <SectionList
      ref={ref}
      sections={SECTIONS}
      keyExtractor={(item) => item.id}
      stickySectionHeadersEnabled
      onViewableItemsChanged={onViewableItemsChanged}
      onScroll={(e) =>
        log('SectionList', `onScroll y=${Math.round(e.nativeEvent.contentOffset.y)}`)
      }
      scrollEventThrottle={100}
      getItemLayout={undefined}
      onScrollToIndexFailed={() =>
        log('SectionList', 'scrollToLocation failed (needs getItemLayout)')
      }
      renderSectionHeader={({ section }) => (
        <View style={headerStyle}>
          <Text style={label}>{section.title}</Text>
        </View>
      )}
      renderItem={({ item }) => (
        <View style={rowStyle}>
          <Text style={label}>{item.title}</Text>
        </View>
      )}
    />
  );
}

function VirtualizedSectionListPage() {
  const onViewableItemsChanged = React.useRef(viewable('VirtualizedSectionList')).current;
  return (
    <VirtualizedSectionList
      sections={SECTIONS}
      getItemCount={(data) => data.length}
      getItem={(data, index) => data[index]}
      keyExtractor={(item) => item.id}
      onViewableItemsChanged={onViewableItemsChanged}
      onScroll={(e) =>
        log('VirtualizedSectionList', `onScroll y=${Math.round(e.nativeEvent.contentOffset.y)}`)
      }
      scrollEventThrottle={100}
      renderSectionHeader={({ section }) => (
        <View style={headerStyle}>
          <Text style={label}>{section.title}</Text>
        </View>
      )}
      renderItem={({ item }) => (
        <View style={rowStyle}>
          <Text style={label}>{item.title}</Text>
        </View>
      )}
    />
  );
}

const PAGES = [
  ['touch', 'Touch', TouchPage],
  ['vlist', 'VList', VirtualizedListPage],
  ['section', 'Section', SectionListPage],
  ['vsection', 'VSection', VirtualizedSectionListPage],
];

function Probe() {
  const [page, setPage] = React.useState('touch');
  const Page = PAGES.find((p) => p[0] === page)[2];
  React.useEffect(() => {
    log('probe', `page=${page}`);
  }, [page]);
  return (
    <View style={{ flex: 1, backgroundColor: '#102033' }}>
      <View style={{ flexDirection: 'row' }}>
        {PAGES.map(([id, title]) => (
          <Tile key={id} name={`nav-${id}`} style={{ flex: 1 }}>
            <Pressable
              onPress={() => setPage(id)}
              style={{
                padding: 12,
                backgroundColor: page === id ? '#2b6cb0' : '#1a365d',
                alignItems: 'center',
              }}
            >
              <Text style={label}>{title}</Text>
            </Pressable>
          </Tile>
        ))}
      </View>
      <View style={{ flex: 1 }}>
        <Page key={page} />
      </View>
    </View>
  );
}

AppRegistry.registerComponent('RNQnxPlayground', () => Probe);
