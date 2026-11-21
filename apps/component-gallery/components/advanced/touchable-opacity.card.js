import * as React from 'react';
import { Text, View } from 'react-native';
import { INK, MUTED } from '../../ui';
import { ProofShell, resolveComponent } from './proofCard';

function TouchableOpacityProof() {
  const [presses, setPresses] = React.useState(0);
  const resolved = React.useMemo(() => resolveComponent('TouchableOpacity', []), []);
  return (
    <ProofShell name="TouchableOpacity" resolved={resolved}>
      {(TouchableOpacity) => (
        <View style={{ gap: 10, alignItems: 'flex-start' }}>
          <TouchableOpacity onPress={() => setPresses((n) => n + 1)}>
            <View
              style={{
                paddingVertical: 12,
                paddingHorizontal: 22,
                borderRadius: 8,
                backgroundColor: '#1b5b61',
              }}
            >
              <Text style={{ color: INK, fontSize: 15 }}>TouchableOpacity</Text>
            </View>
          </TouchableOpacity>
          <Text style={{ color: MUTED, fontSize: 12 }}>presses: {presses}</Text>
        </View>
      )}
    </ProofShell>
  );
}

export default {
  id: 'touchable-opacity',
  title: 'TouchableOpacity',
  detail: 'Opacity dims while pressed',
  order: 74,
  component: 'TouchableOpacity',
  Component: TouchableOpacityProof,
};
