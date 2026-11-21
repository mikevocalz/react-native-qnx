import * as React from 'react';
import { Text, View } from 'react-native';
import { INK } from '../../ui';
import { ProofShell, resolveComponent } from './proofCard';

function SafeAreaViewProof() {
  const resolved = React.useMemo(
    () =>
      resolveComponent('SafeAreaView', [
        [
          "require('react-native/Libraries/Components/SafeAreaView/SafeAreaView')",
          () => require('react-native/Libraries/Components/SafeAreaView/SafeAreaView'),
        ],
      ]),
    []
  );
  return (
    <ProofShell name="SafeAreaView" resolved={resolved}>
      {(SafeAreaView) => (
        <View style={{ borderWidth: 1, borderColor: '#2a4560', borderRadius: 8 }}>
          <SafeAreaView style={{ backgroundColor: '#1b5b61', borderRadius: 8 }}>
            <Text style={{ color: INK, fontSize: 14, padding: 12 }}>
              Inside SafeAreaView (zero insets expected)
            </Text>
          </SafeAreaView>
        </View>
      )}
    </ProofShell>
  );
}

export default {
  id: 'safe-area-view',
  title: 'SafeAreaView',
  detail: 'Mounts as a View with zero insets',
  order: 80,
  component: 'SafeAreaView',
  Component: SafeAreaViewProof,
};
