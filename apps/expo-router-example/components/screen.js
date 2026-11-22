import * as React from 'react';
import { ScrollView, StyleSheet, Text } from 'react-native';

export const styles = StyleSheet.create({
  page: { flex: 1, backgroundColor: '#f4f7fb' },
  content: { padding: 24, gap: 16 },
  heading: { fontSize: 26, color: '#14253d', fontWeight: '700' },
  text: { fontSize: 18, color: '#14253d' },
  input: {
    borderWidth: 1,
    borderColor: '#58718e',
    borderRadius: 8,
    padding: 12,
    color: '#14253d',
    backgroundColor: '#ffffff',
    fontSize: 18,
  },
  button: {
    minHeight: 48,
    padding: 12,
    borderRadius: 8,
    backgroundColor: '#d8e8fa',
    justifyContent: 'center',
  },
});

export function Screen({ title, children }) {
  return (
    <ScrollView
      style={styles.page}
      contentContainerStyle={styles.content}
      keyboardShouldPersistTaps="handled"
    >
      <Text accessibilityRole="header" style={styles.heading}>
        {title}
      </Text>
      {children}
    </ScrollView>
  );
}
