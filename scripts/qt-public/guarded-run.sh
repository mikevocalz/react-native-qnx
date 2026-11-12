#!/usr/bin/env bash
# Run a build script while watching host free space through /work (virtiofs
# reports the Mac volume). The VM data disk also lives on that volume, so a
# full host disk corrupts the VM filesystem (seen 2026-11-06). Stop at < MIN_KB.
set -uo pipefail
script=$1; log=$2; MIN_KB=${MIN_KB:-1572864}
"$script" > "$log" 2>&1 &
pid=$!
while kill -0 $pid 2>/dev/null; do
  avail=$(df --output=avail -k /work | tail -1 | tr -d ' ')
  if [ "$avail" -lt "$MIN_KB" ]; then
    echo "GUARD: host free ${avail}KiB < ${MIN_KB}KiB; stopping build" >> "$log"
    pkill -TERM -P $pid; pkill -TERM ninja; kill -TERM $pid
    wait $pid; echo "EXIT=guard-stopped" >> "$log"; exit 99
  fi
  sleep 15
done
wait $pid; rc=$?
echo "EXIT=$rc $(date -u +%FT%TZ)" >> "$log"; exit $rc
