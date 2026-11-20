# @rn-qnx/react-native-marathon

JavaScript overrides for React Native on the `marathon` Metro platform. The
package is experimental. A native Marathon host is required to run a bundle.

Overrides in `Libraries/`:

| File                                                      | Behavior                                                                                                                      |
| --------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------- |
| `Utilities/Platform.js`                                   | `Platform.OS` is `marathon`. Constants come from `globalThis.__RNQNX_PLATFORM_CONSTANTS`; reading them before install throws. |
| `Components/StatusBar/StatusBar.js`                       | Records requests, applies nothing. The shell owns system bars. `StatusBar.getRequestedState()` returns the merged request.    |
| `Components/KeyboardAvoidingView/KeyboardAvoidingView.js` | Renders a `View` with all view props. The shell owns the keyboard and the host sends no keyboard frames.                      |
| `Linking/Linking.js`                                      | No launch URL; `openURL` and `openSettings` reject with a Marathon error until a host module exists.                          |
| `Utilities/BackHandler.js`                                | Listeners run newest first when the host calls `_emitBackPress()`. `exitApp` has no effect.                                   |

Any other React Native file resolves to the QNX override of the same path when
one exists. This is source reuse; no QNX binary is shared.

Shell reference: https://github.com/patrickjquinn/Marathon-Shell
