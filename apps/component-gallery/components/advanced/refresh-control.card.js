import * as React from 'react';
import { ScrollView, Text, View } from 'react-native';
import { INK, MUTED } from '../../ui';
import { ProofShell, resolveComponent } from './proofCard';

function RefreshControlProof() {
  const [refreshing, setRefreshing] = React.useState(false);
  const [calls, setCalls] = React.useState(0);
  const resolved = React.useMemo(() => resolveComponent('RefreshControl', []), []);
  return (
    <ProofShell name="RefreshControl" resolved={resolved}>
      {(RefreshControl) => (
        <View>
          <View
            style={{
              height: 130,
              borderRadius: 10,
              overflow: 'hidden',
              backgroundColor: '#0c1929',
            }}
          >
            <ScrollView
              refreshControl={
                <RefreshControl
                  refreshing={refreshing}
                  onRefresh={() => {
                    setCalls((n) => n + 1);
                    setRefreshing(false);
                  }}
                />
              }
            >
              {Array.from({ length: 8 }, (_, index) => (
                <Text key={index} style={{ color: INK, fontSize: 14, padding: 8 }}>
                  Scroll row {index + 1}
                </Text>
              ))}
            </ScrollView>
          </View>
          <Text style={{ color: MUTED, fontSize: 12, marginTop: 6 }}>onRefresh calls: {calls}</Text>
        </View>
      )}
    </ProofShell>
  );
}

export default {
  id: 'refresh-control',
  title: 'RefreshControl',
  detail: 'Passed to ScrollView refreshControl',
  order: 79,
  component: 'RefreshControl',
  Component: RefreshControlProof,
};
