#!/bin/sh
set -e
. /data/home/qnxuser/qt612-public-proofs/public-qt-env.sh
cd /data/home/qnxuser/rnqnx-hello-world
exec ./rnqnx_playground_qnx --fullscreen --bundle "$PWD/hello-world-nitro.hbc" --screenshot /data/home/qnxuser/qt612-public-proofs/hello-world-public-qt.png
