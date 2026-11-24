#!/bin/sh
# usage: drag-loop.sh <runs>  -- drawer-drag with pointer events from Screen (rnqnx-inject5), observed by the probe.
runs=$1; name=drawer-drag-physical
K=g.rnqnx.playground
mkdir -p /tmp/perf2-$name
I="sudo -n /data/home/qnxuser/bin/rnqnx-inject5"
i=1
while [ $i -le $runs ]; do
  n=0; while sudo -n pidin -F '%a %N' | grep -qF "$K"; do n=$((n+1)); [ $n -gt 30 ] && break; sleep 1; done
  f=/tmp/perf2-$name/$name-$i.json; rm -f $f
  ( sleep 3.2; $I drag 195 403 620 403 100 ) > /tmp/perf2-$name/inject-$i.log 2>&1 &
  /data/home/qnxuser/v1g2/v1g2-run.sh playground org.rnqnx.playground --bundle /data/home/qnxuser/v1g2/bundles/drawer-drag.hbc --timing-json $f --timing-observe-ms 7500 --timing-settle-ms 1000 > /tmp/perf2-$name/run-$i.log 2>&1
  rc=$?
  echo "run $i rc=$rc"
  wait
  [ $rc -eq 3 ] && { sleep 2; continue; }
  i=$((i+1)); sleep 2
done
echo ALLDONE > /tmp/perf2-drag-done
