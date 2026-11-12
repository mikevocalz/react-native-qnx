#!/bin/sh
set -e
. /data/home/qnxuser/qt612-public-proofs/public-qt-env.sh
cd /data/home/qnxuser/rnqnx-hello-world
export RNQNX_PRESSABLE_FIXTURE=/data/home/qnxuser/rnqnx-async-test/pressable-probe.hbc
unset RNQNX_PRESSABLE_INTERCEPT_TEST RNQNX_PRESSABLE_DISABLE_REMOVED_TARGET_FIX || :
case "$1" in
  positive) ;;
  intercept) export RNQNX_PRESSABLE_INTERCEPT_TEST=1 ;;
  removed-target) export RNQNX_PRESSABLE_DISABLE_REMOVED_TARGET_FIX=1 ;;
  *) exit 2 ;;
esac
exec /data/home/qnxuser/rnqnx-async-test/pressable-probe.qnx
