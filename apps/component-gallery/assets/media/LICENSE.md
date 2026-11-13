# Test media

`rnqnx-test-clip.mp4` (8 s, 480 x 270, H.264 Constrained Baseline + AAC-LC,
about 270 KB) and `rnqnx-test-tone.wav` (3 s, 22.05 kHz mono PCM, 523 Hz
sine) were generated for this repository on 2026-11-13 from FFmpeg's built-in
synthetic sources, so they contain no third-party footage or recordings:

```sh
ffmpeg -f lavfi -i "testsrc2=size=480x270:rate=24:duration=8" \
  -f lavfi -i "sine=frequency=440:sample_rate=44100:duration=8" \
  -c:v libx264 -profile:v baseline -pix_fmt yuv420p -crf 32 -g 24 \
  -af "volume=9dB" -c:a aac -b:a 64k -movflags +faststart -shortest rnqnx-test-clip.mp4
ffmpeg -f lavfi -i "sine=frequency=523:sample_rate=22050:duration=3" \
  -af "volume=-6dB" -ac 1 -c:a pcm_s16le rnqnx-test-tone.wav
```

They are released under CC0 1.0 (public domain dedication). The H.264 and AAC
bitstreams were produced with libx264 and FFmpeg's native AAC encoder; the
files carry no x264 or FFmpeg code.
