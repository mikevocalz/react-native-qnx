import * as React from 'react';
import { Text, View } from 'react-native';
import { INK, MUTED } from '../../ui';
import { ProofShell, resolveComponent } from './proofCard';

function TouchableWithoutFeedbackProof() {
  const [presses, setPresses] = React.useState(0);
  const resolved = React.useMemo(() => resolveComponent('TouchableWithoutFeedback', []), []);
  return (
    <ProofShell name="TouchableWithoutFeedback" resolved={resolved}>
      {(TouchableWithoutFeedback) => (
        <View style={{ gap: 10, alignItems: 'flex-start' }}>
          <TouchableWithoutFeedback onPress={() => setPresses((n) => n + 1)}>
            <View
              style={{
                paddingVertical: 12,
                paddingHorizontal: 22,
                borderRadius: 8,
                backgroundColor: '#1b5b61',
              }}
            >
              <Text style={{ color: INK, fontSize: 15 }}>TouchableWithoutFeedback</Text>
            </View>
          </TouchableWithoutFeedback>
          <Text style={{ color: MUTED, fontSize: 12 }}>presses: {presses}</Text>
        </View>
      )}
    </ProofShell>
  );
}

export default {
  id: 'touchable-without-feedback',
  title: 'TouchableWithoutFeedback',
  detail: 'Child gets presses, no visual feedback',
  order: 76,
  component: 'TouchableWithoutFeedback',
  Component: TouchableWithoutFeedbackProof,
};
