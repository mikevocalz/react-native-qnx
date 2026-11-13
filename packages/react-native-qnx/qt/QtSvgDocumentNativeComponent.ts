import type { CodegenTypes, ViewProps } from 'react-native';
import { codegenNativeComponent } from 'react-native';

type DocumentLoadEvent = Readonly<{
  width: CodegenTypes.Double;
  height: CodegenTypes.Double;
  viewBoxX: CodegenTypes.Double;
  viewBoxY: CodegenTypes.Double;
  viewBoxWidth: CodegenTypes.Double;
  viewBoxHeight: CodegenTypes.Double;
}>;

type DocumentErrorEvent = Readonly<{ message: string }>;

interface NativeProps extends ViewProps {
  xml: string;
  resizeMode?: CodegenTypes.WithDefault<'contain' | 'cover' | 'stretch', 'contain'>;
  onDocumentLoad?: CodegenTypes.DirectEventHandler<DocumentLoadEvent>;
  onDocumentError?: CodegenTypes.DirectEventHandler<DocumentErrorEvent>;
}

export default codegenNativeComponent<NativeProps>('QnxSvgDocument');
