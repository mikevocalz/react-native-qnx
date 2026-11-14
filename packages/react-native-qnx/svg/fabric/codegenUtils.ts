// react-native-codegen does not follow imports. Naming this type UnsafeMixed
// makes it emit folly::dynamic, while TypeScript sees the plain type. Same
// trick as react-native-svg's own src/fabric/codegenUtils.ts.
export type UnsafeMixed<T> = T;
