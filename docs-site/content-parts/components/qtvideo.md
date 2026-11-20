## Props

`QtVideo` is a QNX-only component, not part of React Native. It is exported from `packages/react-native-qnx/qt` (the package has no `exports` map, so the subpath is a plain file path). The wrapper renders the Fabric component `QnxQtVideo`. Types are in `packages/react-native-qnx/qt/index.d.ts`; native props in `packages/react-native-qnx/qt/QtVideoNativeComponent.ts`; they are applied by `vnext/src/fabric/QtVideoBridge.cpp` to `native/qt/video/QtVideoItem.*`, which owns `QnxVideo.qml`.

| Prop                 | Type                          | Status    | Source-backed note                                                                                                  |
| -------------------- | ----------------------------- | --------- | ------------------------------------------------------------------------------------------------------------------- |
| `source`             | string or `{ uri }`           | supported | URL, `file://`, absolute path, or path relative to the executable. Anything else throws a TypeError in the wrapper. |
| `paused`             | boolean                       | supported | Default false.                                                                                                      |
| `muted`              | boolean                       | supported | Bound to the QML `AudioOutput`.                                                                                     |
| `volume`             | number                        | supported | Documented range 0 to 1. Default 1.                                                                                 |
| `rate`               | number                        | supported | Documented range 0.25 to 4. Default 1. Clamping was not found in code.                                              |
| `repeat`             | boolean                       | supported | Infinite loops when true.                                                                                           |
| `resizeMode`         | `contain`, `cover`, `stretch` | supported | Other values throw. Default `contain`.                                                                              |
| `poster`             | string                        | supported | Shown until the first frame arrives.                                                                                |
| `audioDevice`        | string                        | supported | Device id or part of its description. Empty prefers a USB device, then the named card or default.                   |
| `controls`           | boolean                       | supported | Draws a control bar in QML: play/pause, seek bar, time, volume, fullscreen button.                                  |
| `fullscreen`         | boolean                       | partial   | Sets a QML property that highlights the button. No code was found that resizes the window; see Known gaps.          |
| View props (`style`) | ViewProps                     | supported | `children` is omitted from the type.                                                                                |

## Events

Handlers receive the payload directly, not a native event wrapper (`qt/index.js`).

| Event          | Payload                                                      | Status                   |
| -------------- | ------------------------------------------------------------ | ------------------------ |
| `onLoad`       | `duration, width, height, audioDevice`                       | supported                |
| `onProgress`   | `currentTime, playableDuration` (every 250 ms while playing) | supported                |
| `onEnd`        | none                                                         | supported                |
| `onError`      | `message, code`                                              | supported                |
| `onBuffer`     | `isBuffering`                                                | supported                |
| `onSeek`       | `currentTime, seekTime`                                      | supported                |
| `onFullscreen` | `fullscreen`                                                 | supported, not exercised |

The ref handle has `seek(seconds)`, `play()` and `pause()`.

## Platform behavior on QNX

Playback uses Qt Multimedia with the FFmpeg backend, software decode, and frames are drawn in the Qt Quick scene graph. The component exists only if the host is built with `RNQNX_ENABLE_QT_VIDEO=ON` (default OFF in `cmake/RnqnxQtNative.cmake`) and the Qt Multimedia add-on. Without it, rendering throws "QtVideo is unavailable" because the view manager `QnxQtVideo` is missing. On the Pi 400 on 2026-11-13, a 480x270 H.264/AAC clip played, with seek, pause, play, end and a bad-URL error reaching JavaScript. An https clip played after FFmpeg was rebuilt with OpenSSL 3. Source: `docs/evidence/media-2026-11-13.md`.

## Unsupported or partial props

Subtitles, audio-track selection, HLS and DASH playlists, and picture-in-picture have no props or code (`docs/getting-started/media.md`). Hardware decode is not used.

## Minimal example

```tsx
import React, { useRef } from 'react';
import { Button, View } from 'react-native';
import { QtVideo } from '@rn-qnx/react-native-qnx/qt';

export default function Player() {
  const ref = useRef<{ seek(s: number): void; play(): void; pause(): void }>(null);
  return (
    <View style={{ flex: 1 }}>
      <QtVideo
        ref={ref}
        style={{ width: 480, height: 270 }}
        source="http://192.168.2.1:8765/rnqnx-test-clip.mp4"
        controls
        resizeMode="contain"
        onLoad={(e) => console.log('loaded', e.duration, e.width, e.height)}
        onError={(e) => console.log('error', e.code, e.message)}
      />
      <Button title="Seek 5s" onPress={() => ref.current?.seek(5)} />
    </View>
  );
}
```

Build the host with `-DRNQNX_ENABLE_QT_VIDEO=ON`. See [Qt tiers](/docs/reference/qt-tiers).

## Known gaps

- Control-bar taps and the fullscreen toggle were not exercised on the board.
- The 2026-11-19 Pi 400 builds had QtVideo off, so they say nothing about it.
- Host column is unverified: `tests/native/qt-video` playback tests had not run anywhere as of the media doc, and skip on Qt builds without a working backend.
- A JavaScript subset of the `react-native-video` 7 API sits in `packages/react-native-qnx/video` (`useVideoPlayer`, `VideoView`). It has not run on a device and is not aliased in any app's Metro config. The upstream beta cannot load on the pinned Nitro v1.
