#!/bin/sh
# Device-side launcher for rnqnx_webgpu_test (copied next to the binary by
# run-on-pi.sh). Starts only this one test binary as its own Screen window,
# waits for it to report ready, takes a screenshot with the stock `screenshot`
# tool, then waits for the binary to exit. It never signals any process other
# than the one it started.
#
#   rnqnx-webgpu-launch.sh [test app arguments...]
#   HOLD=<seconds the window stays up after the last stage, default 10>
D=$(cd "$(dirname "$0")" && pwd)
export LD_LIBRARY_PATH="/data/pi400-graphics-compat/lib/dll:/data/pi400-graphics-compat/usr/lib:/data/pi400-graphics-compat/usr/lib/graphics/rpi4-drm:/usr/lib"
OUT="$D/run-$(date +%Y%m%dT%H%M%S)"
mkdir -p "$OUT"
echo "output directory: $OUT"

"$D/rnqnx_webgpu_test" --out-dir="$OUT" --hold-seconds="${HOLD:-10}" "$@" >"$OUT/app.log" 2>&1 &
pid=$!

waited=0
while [ ! -f "$OUT/ready" ] && kill -0 "$pid" 2>/dev/null && [ "$waited" -lt 120 ]; do
  sleep 1
  waited=$((waited + 1))
done

if [ -f "$OUT/ready" ]; then
  # The stock tool needs the Screen display context, which an ordinary user may
  # not create; fall back to passwordless sudo, which the target is set up for.
  if ! screenshot -file="$OUT/screen.bmp" >"$OUT/screenshot.log" 2>&1; then
    # shellcheck disable=SC2024 # the redirect target is a user-owned file
    sudo -n screenshot -file="$OUT/screen.bmp" >>"$OUT/screenshot.log" 2>&1 || echo "screenshot failed" >>"$OUT/screenshot.log"
  fi
else
  echo "app did not reach the ready state (waited ${waited}s)" >"$OUT/screenshot.log"
fi

# The app ends on its own after the hold period. Wait a bounded time for that;
# only the one process this script started is ever signalled.
n=0
while kill -0 "$pid" 2>/dev/null && [ "$n" -lt 90 ]; do sleep 1; n=$((n + 1)); done
if kill -0 "$pid" 2>/dev/null; then
  echo "app still running after the wait; sending SIGTERM to pid $pid" | tee -a "$OUT/app.log"
  kill -TERM "$pid"
fi
wait "$pid"
rc=$?
echo "exit status: $rc" | tee -a "$OUT/app.log"
grep -E '^RNQNX_WEBGPU (RESULT|STAGE end)' "$OUT/app.log"
exit "$rc"
