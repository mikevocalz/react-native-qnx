import * as React from 'react';
import { Text } from 'react-native';
import { INK, MUTED } from '../../ui';
import { ProofShell, resolveComponent } from './proofCard';

function StatusBarProof() {
  const resolved = React.useMemo(() => resolveComponent('StatusBar', []), []);
  return (
    <ProofShell name="StatusBar" resolved={resolved}>
      {(StatusBar) => (
        <>
          <StatusBar barStyle="light-content" hidden={false} />
          <Text style={{ color: INK, fontSize: 14 }}>
            StatusBar element mounted with barStyle light-content.
          </Text>
          <Text style={{ color: MUTED, fontSize: 12 }}>
            The target has no status bar, so nothing is drawn.
          </Text>
        </>
      )}
    </ProofShell>
  );
}

export default {
  id: 'status-bar',
  title: 'StatusBar',
  detail: 'Static API is a no-op on QNX',
  order: 81,
  component: 'StatusBar',
  Component: StatusBarProof,
};
