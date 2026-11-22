import * as React from 'react';
import { Pressable, Text } from 'react-native';
import { Link } from 'expo-router';
import { useExample } from '../components/example-store';
import { Screen, styles } from '../components/screen';

export default function Review() {
  const note = useExample((state) => state.note);
  const visits = useExample((state) => state.visits);
  const reset = useExample((state) => state.reset);
  return (
    <Screen title="Review">
      <Text style={styles.text} testID="review-note">
        {note || 'Your draft is empty.'}
      </Text>
      <Text style={styles.text} testID="review-visits">
        Visits: {visits}
      </Text>
      <Pressable
        accessibilityRole="button"
        accessibilityLabel="Reset draft"
        testID="reset-draft"
        onPress={reset}
        style={styles.button}
      >
        <Text style={styles.text}>Reset draft</Text>
      </Pressable>
      <Link href="/" asChild>
        <Pressable accessibilityRole="link" style={styles.button}>
          <Text style={styles.text}>Return to draft</Text>
        </Pressable>
      </Link>
    </Screen>
  );
}
