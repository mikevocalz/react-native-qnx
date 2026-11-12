#!/bin/sh
set -e
. /data/home/qnxuser/qt612-public-proofs/public-qt-env.sh
cd /data/home/qnxuser/rnqnx-hello-world
export RNQNX_NITRO_QT_FIXTURES=/data/home/qnxuser/rnqnx-async-test/fixtures
exec /data/home/qnxuser/rnqnx-async-test/rnqnx_nitro_qt_test
