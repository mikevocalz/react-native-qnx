// Shared shell for the single-component cards (`*.card.js` files that carry a
// React Native component name in `component`). A card resolves the component
// the way an app would, from the `react-native` public entry, and falls back to
// the deep import path only when the entry has no getter for it. The card shows
// which route worked and the exact error from each route that failed, so a
// missing export is visible on screen rather than hidden.
import * as React from 'react';
import { Text, View } from 'react-native';
import { INK, MUTED } from '../../ui';

const ERROR = '#ff9d8f';

export function resolveComponent(name, deepLoaders) {
  const failures = [];
  try {
    const entry = require('react-native')[name];
    if (entry) return { Comp: entry, via: `import { ${name} } from 'react-native'`, failures };
    failures.push(`react-native entry returned ${String(entry)} for ${name}`);
  } catch (error) {
    failures.push(`react-native entry: ${error && error.message ? error.message : String(error)}`);
  }
  for (const [label, load] of deepLoaders) {
    try {
      const loaded = load();
      const Comp = loaded && loaded.default ? loaded.default : loaded;
      if (Comp) return { Comp, via: label, failures };
    } catch (error) {
      failures.push(`${label}: ${error && error.message ? error.message : String(error)}`);
    }
  }
  return { Comp: null, via: null, failures };
}

export function ProofShell({ name, resolved, children }) {
  return (
    <View testID={`proof-${name}`}>
      <Text style={{ color: MUTED, fontSize: 13, marginBottom: 6 }}>
        {resolved.via ? `Resolved: ${resolved.via}` : `${name} did not resolve`}
      </Text>
      {resolved.failures.map((line) => (
        <Text key={line} style={{ color: ERROR, fontSize: 12, marginBottom: 6 }}>
          {line}
        </Text>
      ))}
      {resolved.Comp ? (
        children(resolved.Comp)
      ) : (
        <Text style={{ color: INK, fontSize: 14 }}>Not rendered: no implementation reachable.</Text>
      )}
    </View>
  );
}
