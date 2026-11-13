# @rn-qnx/expo-constants-qnx

QNX implementation of the `ExponentConstants` native module that
[`expo-constants`](https://docs.expo.dev/versions/latest/sdk/constants/) 58
reads. Add both packages to an app:

```json
"dependencies": {
  "expo-constants": "58.0.16",
  "@rn-qnx/expo-constants-qnx": "workspace:*"
}
```

`project prepare-native` finds this package through its
`"react-native-qnx": {"expo": {...}}` declaration and records it in
`native-app.json` as an `expoModules` entry; `RnqnxConsumerApp.cmake` links it
and registers it before the bundle runs.

| Constant                     | QNX value                                                |
| ---------------------------- | -------------------------------------------------------- |
| `expoConfig`, `manifest`     | the app's resolved `expo` config, embedded at build time |
| `deviceName`                 | `uname` node name                                        |
| `platform.qnx`               | `systemName`, `systemVersion`, `machine` from `uname`    |
| `executionEnvironment`       | `bare`                                                   |
| `appOwnership`               | `null`                                                   |
| `sessionId`                  | a version 4 UUID per runtime                             |
| `statusBarHeight`            | `0` (no status bar)                                      |
| `systemFonts`                | `[]`                                                     |
| `getWebViewUserAgentAsync()` | resolves to `null` (no WebView on QNX)                   |
