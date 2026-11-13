import * as React from 'react';
import { Text } from 'react-native';
import { Screen, Heading, MUTED } from '../ui';

export default function MediaRoute() {
  return (
    <Screen>
      <Heading title="Media" detail="Images from bundled files and URLs." />
      <Text style={{ color: MUTED }}>
        The expo-image cards land here with the expo-image pull request.
      </Text>
    </Screen>
  );
}
