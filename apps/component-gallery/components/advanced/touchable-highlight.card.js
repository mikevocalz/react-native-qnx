import * as React from 'react';
import { Text, View } from 'react-native';
import { INK, MUTED } from '../../ui';
import { ProofShell, resolveComponent } from './proofCard';

function TouchableHighlightProof() {
  const [presses, setPresses] = React.useState(0);
  const resolved = React.useMemo(() => resolveComponent('TouchableHighlight', []), []);
  return (
    <ProofShell name="TouchableHighlight" resolved={resolved}>
      {(TouchableHighlight) => (
        <View style={{ gap: 10, alignItems: 'flex-start' }}>
          <TouchableHighlight onPress={() => setPresses((n) => n + 1)} underlayColor="#79e8dc">
            <View
              style={{
                paddingVertical: 12,
                paddingHorizontal: 22,
                borderRadius: 8,
                backgroundColor: '#1b5b61',
              }}
            >
              <Text style={{ color: INK, fontSize: 15 }}>TouchableHighlight</Text>
            </View>
          </TouchableHighlight>
          <Text style={{ color: MUTED, fontSize: 12 }}>presses: {presses}</Text>
        </View>
      )}
    </ProofShell>
  );
}

export default {
  id: 'touchable-highlight',
  title: 'TouchableHighlight',
  detail: 'Underlay colour while pressed',
  order: 75,
  component: 'TouchableHighlight',
  Component: TouchableHighlightProof,
};
