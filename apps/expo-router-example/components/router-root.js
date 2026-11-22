import * as React from 'react';
import { ExpoRoot } from 'expo-router';
import { QnxRouterProvider } from '@rn-qnx/expo-router-qnx';
import { ctx } from '../app-ctx';
import { ExampleProvider } from './example-store';

export default function App() {
  return (
    <QnxRouterProvider>
      <ExpoRoot context={ctx} wrapper={ExampleProvider} />
    </QnxRouterProvider>
  );
}
