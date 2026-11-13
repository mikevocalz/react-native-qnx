# Video and audio on QNX (Qt Multimedia)

Status: video and audio play on the Pi 400 through `QtVideo` in the gallery
(software decode, 3.5 mm jack output). The measured results are in
[the evidence note](../evidence/media-2026-11-13.md); what has not been done
is under [Open work](#open-work).

## Backend choice

Qt Multimedia 6.12.0 has two backends that can build for QNX: `qnx`
(mm-renderer) and `ffmpeg`.

| Backend             | Result   | Why                                                                                                                                                                                                                                                                                                                                                                                              |
| ------------------- | -------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| `qnx` (mm-renderer) | Not used | `QQnxMediaPlayer` attaches the video output to a native Screen window (`mmr_output_attach(..., "video")` in `src/plugins/multimedia/qnx/mediaplayer/qqnxmediaplayer.cpp:391`, `screen_create_window` in `common/qqnxwindowgrabber.cpp:89`). That window sits beside the Qt Quick scene graph, so it cannot clip, round, fade or draw under sibling React views, the window chrome or the drawer. |
| `ffmpeg`            | Chosen   | Decodes in software and hands `QVideoFrame`s to `VideoOutput`, which draws them as a scene-graph texture.                                                                                                                                                                                                                                                                                        |

The Pi image's own FFmpeg (`libavcodec.so.62.11.100`, FFmpeg 8.0) cannot be
used. Its configuration (read from `avutil_configuration` in
`libavutil.so.60`) has `--enable-gpl --enable-version3`, `--enable-libxvid`,
and `--disable-decoder='h264,hevc,vc1,prores_raw'`. It is a GPL build with no
H.264 decoder. The add-on therefore builds its own FFmpeg.

Audio uses the Multimedia library's QNX 8 `io-snd` backend
(`QT_FEATURE_qnx_io_snd`, `src/multimedia/qnx-iosnd/`), which needs no extra
plugin.

## What the board has (measured)

- `/dev/snd` does not exist. `io-snd` is installed (`/system/bin/io-snd`,
  `audio_io-snd_be-800_B247`) but not running, and as `qnxuser` it exits with
  "Driver init failed - Operation not permitted". It needs root.
- `/etc/system/config/sound/io_snd.conf` configures one controller,
  `bcm2711_pwm`: the on-board 3.5 mm headphone jack. The I2S section for the
  Raspiaudio Mic Ultra+ is commented out.
- The installed audio drivers are `sndrv-ctrl-bcm2711_pcm.so`,
  `sndrv-ctrl-bcm2711_pwm.so`, `sndrv-ctrl-usb.so` and `sndrv-mixer-wm8960.so`.
  There is no HDMI audio driver on this image, so sound over HDMI is not
  available here. Output would go to the 3.5 mm jack or a USB audio device.
- `aplay -l`: "no soundcards found".

## Add-on build

`scripts/qt-media/` builds, in the `qnx805-build` container, into
`/data/home/qnxuser/qt612-media/` (never `qt612-public-o2`):

1. `build-ffmpeg.sh`: FFmpeg 8.0 (`ffmpeg.org/releases/ffmpeg-8.0.tar.xz`,
   SHA-256 `b2751fcc...27b8e`). configure uses `--disable-gpl --disable-nonfree
--enable-version3 --enable-openssl --disable-autodetect` (LGPL 3.0 or later). Decoders: H.264, VP8, VP9, MPEG-4, MJPEG, AAC, MP3,
   Opus, Vorbis, FLAC, PCM. Protocols: file, http, https, tls, tcp, udp, pipe, cache. Assembly is off. It built and installed.
2. `build-qtmultimedia.sh`: `qtmultimedia-everywhere-src-6.12.0.tar.xz`
   (SHA-256 `3143f53b...a62d`, equal to the value published next to the archive
   on download.qt.io). It needs `libQt6Concurrent`, which the existing Qt core
   build tree had not built. `ninja Concurrent` in that tree adds the one
   library (15 steps, nothing else rebuilt); it is deployed in the add-on
   directory.

`run-media-app.sh` starts an app with `LD_LIBRARY_PATH` and `QT_PLUGIN_PATH`
pointing at the add-on first.

## Components

- `QtVideo` (`packages/react-native-qnx/qt`): Fabric component `QnxQtVideo`,
  behind `RNQNX_ENABLE_QT_VIDEO` (default OFF). `native/qt/video/` holds
  `QtVideoItem` (a `QQuickItem` owning `QnxVideo.qml`: `MediaPlayer`,
  `AudioOutput`, `VideoOutput` and a control bar drawn with plain Items).
  Props: `source`, `paused`, `muted`, `volume`, `rate`, `repeat`, `resizeMode`,
  `poster`, `controls`, `fullscreen`. Events: `onLoad`, `onProgress`, `onEnd`,
  `onError`, `onBuffer`, `onSeek`, `onFullscreen`. Ref: `seek`, `play`,
  `pause`. Generated Fabric code is under `vnext/generated/.../RnqnxQtVideo`
  (`pnpm qt:video:check`).
- `react-native-video` API subset (`packages/react-native-qnx/video`):
  `useVideoPlayer`, `useEvent`, `VideoPlayer`, `VideoView`. It is plain
  JavaScript over `QtVideo`, not a Nitro module. See below.

## react-native-video 7.0.0-beta.12 and react-native-audio-api

`react-native-video` 7.0.0-beta.12 does not run unchanged on the pinned Nitro
v1 runtime (`1.0.0-alpha.nightly.136`), from inspecting the published tarball:

- `src/core/utils/playerFactory.ts:13` calls
  `NitroModules.createHybridObject('VideoPlayerFactory')` at import time. The
  pinned `NitroModulesProxy` has no `createHybridObject`, so importing the
  package throws.
- The specs are Nitro v0 `interface ... extends HybridObject<{ios:'swift';
android:'kotlin'}>`; the shipped C++ was generated by nitrogen 0.35.0
  (`explicit HybridXSpec(): HybridObject(TAG)`, `loadHybridMethods()`), which
  does not match the pinned runtime's `HybridObject` class.
- The peer range `react-native-nitro-modules >=0.35.0` does not match a
  `1.0.0-alpha` version under npm semver rules.
- The native view is a separate legacy Fabric component, `RNCVideoView`, with a
  `nitroId` handshake.
- The native contract is 88 pure virtual methods across seven HybridObjects.

Two ways would make the real package run, and neither is done:

1. A Nitro v0 compatibility layer in the runtime: `createHybridObject` plus a
   v0-style `HybridObject` base (the `TAG` constructor, `loadHybridMethods`,
   `registerHybrids`) so the generated nitrogen 0.35 C++ compiles. It would
   mirror a second, older registration API on top of the pinned v1 one and
   would have to track nitrogen 0.35 output.
2. Hand-written v1 C++ for the seven HybridObjects registered under the same
   names (`VideoPlayerFactory`, `VideoPlayer`, ...), the `RNCVideoView` Fabric
   component with its `nitroId` handshake, and a small `createHybridObject`
   JS function over `getHybridClass` in the QNX Nitro runtime. The package's
   own JavaScript then runs as published. This is the smaller and less
   fragile route: it depends only on the 88-method surface, not on nitrogen's
   generator. It needs a package under `packages/` with the ADR-0019
   `react-native-qnx.nitro` metadata, built on `QtVideoItem`'s player.

Until one of them lands the gallery uses the JavaScript subset above, which
`packages/react-native-qnx/video` implements and `tests/js/qt-video.test.mjs`
checks. It is not aliased in any app's Metro config yet. `@videojs/react` is only
imported from `*.web.tsx` files and is not needed on QNX.

`react-native-audio-api` 1.0.0 also needs work: its C++ core
(`common/cpp/audioapi`) is portable, but it installs through an
`AudioAPIModule` TurboModule and has audio players only for iOS (AVAudioEngine)
and Android (Oboe). A QNX `AudioPlayer` on `QAudioSink` and a JSI installer are
missing. Until then audio is `QtVideo` with an audio file (the gallery's Audio
card).

## Pi 400 results: Qt Multimedia probe (2026-11-13)

`rnqnx_media_probe` (a QMediaPlayer + QAudioOutput + QVideoSink program, no
React Native) ran on the board with the add-on in `/data/home/qnxuser/qt612-media`
and the Qt core from `qt612-public-o2`. Files: `qt612-media/SHA256SUMS` was
written at stage time (`/home/qnxbuild/media/SHA256SUMS`).

- The ffmpeg plugin loads: `Using Qt multimedia with FFmpeg version 8.0 LGPL
version 2.1 or later`. It is the only backend (`Available backends
("ffmpeg")`).
- Software H.264 decode of the 480 x 270 clip ran in real time: 170 frames in
  7.0 s (24 fps), position tracking the wall clock, `hasAudio=1 hasVideo=1`.
  Hardware decoders: none listed.
- Audio: with no `io-snd`, `QMediaDevices::audioOutputs()` is empty
  ("snd_device_name_hint failed"). I started
  `sudo io-snd -c /etc/system/config/sound/io_snd.conf` by hand (a new process;
  nothing was restarted). `/dev/snd` then showed `controlC0`, `controlC1`,
  `pcmC0D0p`, `pcmPreferredp`, and the only output card is `bcm2711 pwm`: the
  3.5 mm headphone jack. There is no HDMI or USB audio device. Qt opened it:
  `Playback opened: "default" buffer_frames: 2112 period_frames: 704`, worker
  at SCHED_FIFO 20, native format float32.
- The 3 s WAV played to the end through that device: position 1021, 2043, 3000
  ms at 1 s wall-clock steps, then `EndOfMedia` and stopped. Whether sound is
  audible at the jack has to be confirmed by ear; nothing here measures it.
- A bad URL (`http://192.168.2.1:9/none.mp4`) gives `error=1 Could not open
file` and `InvalidMedia`.

Audio debugging after an owner report of "no audio" (same day):

- The clip's AAC track is present and was quiet (mean -21 dB, peak -15 dB, a
  440 Hz sine). It was regenerated 9 dB louder (peak -3.4 dB, mean -12 dB).
- The only playback card is `bcm2711 pwm` (card 0, `pcmC0D0p`): the 3.5 mm
  jack. Its mixer reads `PCM Mixer Playback Volume` 600/600 (0 dB) and
  `Playback Switch` on, so nothing is muted. `snd_info` also lists card 1,
  `USB`, which has a control node but no PCM: no USB audio device is attached
  (`usb -v` shows only the hub, a Logitech receiver and the keyboard).
- There is no HDMI audio path on this image: `/system/lib/dll` has no HDMI
  `sndrv` driver and `io_snd.conf` defines only `bcm2711_pwm` (plus a commented
  I2S card). Speakers must go in the 3.5 mm jack, or a USB audio device must be
  plugged in.
- The app now picks the output itself (`audioDevice` prop on `QtVideo`: id or
  part of a device name; empty prefers a USB device, then the first named card
  such as `plughw:CARD=bcm2711 pwm,DEV=0`). With the app running, Qt logged
  `Playback opened: "plughw:CARD=bcm2711 pwm,DEV=0"`, float32, 2112-frame
  buffer, with no underrun or decode warnings. The `aplay` command finished
  without an error on the same card. None of this proves sound at the jack.
  `onLoad` carries the chosen device name; the Audio card shows it.

io-snd keeps running until reboot or until it is stopped; it was started only
for this test.

## Audio at boot

`tools/qnx-audio/install-audio-boot.sh` adds a block to
`/system/etc/startup/full_startup.sh` that starts `io-snd` with
`/etc/system/config/sound/io_snd.conf` when `/dev/snd` does not exist. It
backs the file up to `full_startup.sh.before-audio`, is idempotent, checks the
patched file with `sh -n`, and `--undo` restores the backup. Run on a copy of
the board's real `full_startup.sh` it installed, refused a second install, and
restored the identical file. From the Mac:

```sh
K=~/qnx-pi400-8.0.5/pr102-ssh
ssh -i $K/id_ed25519 -o UserKnownHostsFile=$K/pr104-sdk-trusted-hosts \
  qnxuser@192.168.2.2 'sudo -n sh -s' < tools/qnx-audio/install-audio-boot.sh
```

The change takes effect at the next boot. It has not been installed on the
board.

## HDMI audio

No HDMI audio driver exists for this image.

- The QNX SDP 8.0.5 sound drivers for aarch64le (`target/qnx/aarch64le/lib/dll`)
  are `snd-restore`, `snd-splitter`, `snd-swmix` and `sndrv-ctrl-usb`. No
  `sndrv`, `deva` or `devc` driver mentions HDMI or vc4 (`find` over the SDP
  for `*hdmi*` finds only Screen graphics configs, an EDID blob and the DRM
  header `vc4_drm.h`).
- The board's own `/system/lib/dll` adds `sndrv-ctrl-bcm2711_pcm` (I2S),
  `sndrv-ctrl-bcm2711_pwm` (the 3.5 mm jack) and `sndrv-mixer-wm8960`. Nothing
  for HDMI.
- The Raspberry Pi 4 QNX 8.0 BSP repository
  (`gitlab.com/qnx/custom-target-image-builds/raspberry-pi-4-qnx-8.0`, `main`
  archive, 9.8 MB) contains no audio source: `grep -ril "io-snd\|hdmi"` hits
  only `README.md` (the display cable), the graphics configs and `mtouch.conf`;
  its `src/` holds a patch Makefile for the graphics trace tool.

An HDMI audio path would need a new `sndrv` driver for the BCM2711 HDMI audio
block, with no existing source to start from. Speakers go in the 3.5 mm jack,
or in a USB audio device (`sndrv-ctrl-usb` is installed and `QtVideo` prefers a
USB output).

## Streams over https

FFmpeg is built with the SDP's OpenSSL 3 (`--enable-openssl`). OpenSSL 3 is
Apache-2.0, which LGPL 2.1 cannot include, so the FFmpeg build is LGPL 3.0 or
later (`--enable-version3`, still `--disable-gpl --disable-nonfree`). The
packaging notice records this.

## Tests

- `tests/js/qt-video.test.mjs`: the `QtVideo` wrapper (source forms, event
  unwrapping, ref commands, argument errors) and the `react-native-video` API
  subset, against a recording stub of the native view. It failed when the
  `onEnd` wrapper was broken and passed when restored.
- `tests/native/qt-video/`: `QtVideoItem` on the offscreen platform with a 1 s
  generated VP8 clip (`clip.webm`, 9.7 KB). It checks source resolution, the
  load, seek, end and error signals, and that the QML player is destroyed with
  its item. The Linux host-renderer job installs the `qtmultimedia` module and
  builds with `RNQNX_ENABLE_QT_VIDEO=ON`. On a Qt without a working backend
  (the Homebrew Qt on the Mac has only AVFoundation) the two playback tests
  skip; set `RNQNX_QT_VIDEO_REQUIRE_PLAYBACK=1` to make a skip a failure. The
  skip path and the lifetime test were run on the Mac; the playback tests have
  not run anywhere.

## Open work

- The real `react-native-video` 7 beta and `react-native-audio-api` do not run;
  see the section above for why and for what each needs.
- Subtitles, audio-track selection, HLS and DASH playlists: the FFmpeg build
  has the HLS demuxer but no subtitle decoders or DASH demuxer, and `QtVideo`
  exposes no track props.
- On the board: control-bar taps, fullscreen toggle, the Audio card buttons and
  the Mac URL chip need pointer input that was not available during the run.
- Audibility at the jack is unconfirmed.
- Picture-in-picture has no counterpart here: `rnqnx-winmgr` has no
  picture-in-picture window type.
