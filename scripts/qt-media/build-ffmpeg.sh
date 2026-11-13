#!/usr/bin/env bash
# FFmpeg 8.0 for QNX aarch64le: LGPL 2.1+, shared, decoders and demuxers only.
# --disable-gpl --disable-version3 --disable-nonfree, no external libraries
# (--disable-autodetect), so the result links nothing but the QNX C library.
# Assembly is off: qcc is not a compiler FFmpeg's configure recognizes.
set -euo pipefail
source "$(dirname "$0")/layout.sh"
source "$QNX_SDP/qnxsdp-env.sh" >/dev/null
mkdir -p "$MEDIA_ROOT"; cd "$MEDIA_ROOT"
if [ ! -f ffmpeg-$FFMPEG_VERSION.tar.xz ]; then
  curl -sSL -o ffmpeg-$FFMPEG_VERSION.tar.xz https://ffmpeg.org/releases/ffmpeg-$FFMPEG_VERSION.tar.xz
fi
echo "$FFMPEG_SHA256  ffmpeg-$FFMPEG_VERSION.tar.xz" | sha256sum -c -
[ -d ffmpeg-$FFMPEG_VERSION ] || tar xf ffmpeg-$FFMPEG_VERSION.tar.xz
mkdir -p ff-build; cd ff-build
../ffmpeg-$FFMPEG_VERSION/configure --prefix="$MEDIA_PREFIX" \
  --enable-cross-compile --target-os=qnx --arch=aarch64 \
  --cc="qcc -Vgcc_ntoaarch64le -D_QNX_SOURCE" --cxx="qcc -Vgcc_ntoaarch64le -D_QNX_SOURCE" \
  --ar=ntoaarch64-ar --nm=ntoaarch64-nm --ranlib=ntoaarch64-ranlib --strip=ntoaarch64-strip \
  --enable-shared --disable-static --disable-programs --disable-doc --disable-debug \
  --disable-everything --disable-autodetect --disable-asm --enable-pic \
  --disable-gpl --disable-nonfree --disable-version3 \
  --enable-avcodec --enable-avformat --enable-avutil --enable-swscale --enable-swresample --enable-network \
  --enable-protocol=file,http,tcp,pipe,cache \
  --enable-demuxer=mov,matroska,ogg,wav,mp3,flac,aac,avi,mpegts,hls \
  --enable-decoder=h264,vp8,vp9,mpeg4,mjpeg,aac,aac_latm,mp3,mp3float,opus,vorbis,flac,pcm_s16le,pcm_s16be,pcm_u8,pcm_f32le \
  --enable-parser=h264,vp8,vp9,aac,aac_latm,mpegaudio,opus,vorbis,flac,mpeg4video,mjpeg \
  --enable-bsf=h264_mp4toannexb,aac_adtstoasc,extract_extradata \
  --enable-filter=null,anull,aresample,scale,format \
  --enable-encoder=pcm_s16le,mjpeg
make -j"${JOBS:-6}"
make DESTDIR="$MEDIA_ROOT/ff-stage" install
echo "FFmpeg staged in $MEDIA_ROOT/ff-stage$MEDIA_PREFIX"
