import * as React from 'react';
import { Text, View } from 'react-native';
import { MUTED } from '../../ui';
import { ProofShell, resolveComponent } from './proofCard';

function ButtonProof() {
  const [presses, setPresses] = React.useState(0);
  const resolved = React.useMemo(() => resolveComponent('Button', []), []);
  return (
    <ProofShell name="Button" resolved={resolved}>
      {(Button) => (
        <View style={{ gap: 10, alignItems: 'flex-start' }}>
          <Button title="Press me" onPress={() => setPresses((n) => n + 1)} color="#1b8f96" />
          <Button title="Disabled" onPress={() => {}} disabled />
          <Text style={{ color: MUTED, fontSize: 12 }}>presses: {presses}</Text>
        </View>
      )}
    </ProofShell>
  );
}

export default {
  id: 'button',
  title: 'Button',
  detail: 'Enabled and disabled',
  order: 73,
  component: 'Button',
  Component: ButtonProof,
};
