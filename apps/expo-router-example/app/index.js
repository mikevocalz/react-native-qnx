import * as React from 'react';
import { Pressable, Text, TextInput } from 'react-native';
import { Link } from 'expo-router';
import { useExample } from '../components/example-store';
import { Screen, styles } from '../components/screen';

export default function Draft() {
  const note = useExample((state) => state.note);
  const visits = useExample((state) => state.visits);
  const setNote = useExample((state) => state.setNote);
  const increment = useExample((state) => state.increment);
  return (
    <Screen title="Your draft">
      <Text style={styles.text}>Edit a note, then switch tabs. Your draft stays in this app.</Text>
      <TextInput
        accessibilityLabel="Draft note"
        testID="draft-note"
        value={note}
        onChangeText={setNote}
        placeholder="Write a note"
        style={styles.input}
      />
      <Pressable
        accessibilityRole="button"
        accessibilityLabel="Add a visit"
        testID="add-visit"
        onPress={increment}
        style={styles.button}
      >
        <Text style={styles.text}>Visits: {visits}</Text>
      </Pressable>
      <Link href="/review" asChild>
        <Pressable accessibilityRole="link" style={styles.button}>
          <Text style={styles.text}>Review your draft</Text>
        </Pressable>
      </Link>
    </Screen>
  );
}
