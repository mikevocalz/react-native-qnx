import * as React from 'react';
import { Text, TextInput, View } from 'react-native';
import { INK, MUTED } from '../../ui';
import { ProofShell, resolveComponent } from './proofCard';

function KeyboardAvoidingViewProof() {
  const resolved = React.useMemo(() => resolveComponent('KeyboardAvoidingView', []), []);
  return (
    <ProofShell name="KeyboardAvoidingView" resolved={resolved}>
      {(KeyboardAvoidingView) => (
        <KeyboardAvoidingView behavior="padding" keyboardVerticalOffset={12} style={{ gap: 8 }}>
          <Text style={{ color: INK, fontSize: 14 }}>
            Inside KeyboardAvoidingView (behavior padding)
          </Text>
          <TextInput
            defaultValue="Field"
            style={{
              color: INK,
              borderWidth: 1,
              borderColor: '#2a4560',
              borderRadius: 8,
              padding: 8,
            }}
          />
          <Text style={{ color: MUTED, fontSize: 12 }}>
            No Keyboard module is wired on QNX, so no avoidance happens.
          </Text>
        </KeyboardAvoidingView>
      )}
    </ProofShell>
  );
}

export default {
  id: 'keyboard-avoiding-view',
  title: 'KeyboardAvoidingView',
  detail: 'Renders children as a View',
  order: 78,
  component: 'KeyboardAvoidingView',
  Component: KeyboardAvoidingViewProof,
};
