'use strict';

const React = require('react');
const { useStore } = require('zustand');
const { createStore } = require('zustand/vanilla');

// Per-instance state, not a global width/menu shared by unrelated app windows.
function useLocalStore(initialState) {
  const ref = React.useRef(null);
  if (ref.current === null) ref.current = createStore(() => initialState);
  return [useStore(ref.current), ref.current];
}

function useElementSize() {
  const [size, store] = useLocalStore({ width: 0, height: 0 });
  const onLayout = React.useCallback(
    (event) => {
      const { width, height } = event.nativeEvent.layout;
      if (!Number.isFinite(width) || !Number.isFinite(height) || width < 0 || height < 0) return;
      const previous = store.getState();
      if (previous.width !== width || previous.height !== height) store.setState({ width, height });
    },
    [store]
  );
  return { ...size, onLayout };
}

module.exports = { useLocalStore, useElementSize };
