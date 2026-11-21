import * as React from 'react';
import { Text, View } from 'react-native';
import { INK } from '../../ui';
import { ProofShell, resolveComponent } from './proofCard';

function ImageBackgroundProof() {
  const resolved = React.useMemo(() => resolveComponent('ImageBackground', []), []);
  return (
    <ProofShell name="ImageBackground" resolved={resolved}>
      {(ImageBackground) => (
        <ImageBackground
          source={require('../../assets/icon.png')}
          resizeMode="cover"
          style={{ height: 150, borderRadius: 10, overflow: 'hidden', justifyContent: 'flex-end' }}
        >
          <View style={{ padding: 10, backgroundColor: 'rgba(12,25,41,0.72)' }}>
            <Text style={{ color: INK, fontSize: 14 }}>Child drawn over the image</Text>
          </View>
        </ImageBackground>
      )}
    </ProofShell>
  );
}

export default {
  id: 'image-background',
  title: 'ImageBackground',
  detail: 'Image behind children, resizeMode cover',
  order: 77,
  component: 'ImageBackground',
  Component: ImageBackgroundProof,
};
