#!/bin/sh
# usage: perf-loop2.sh <name> <dir> <binary> <bundle> <runs> [timing args...]
# One process per run, no warm-up discarded. A run that was refused because the previous process
# was still exiting (rc=3) is repeated, not counted. Output: /tmp/perf2-<name>/<name>-<n>.json
name=$1; dir=$2; bin=$3; bundle=$4; runs=$5; shift 5
K=$bin; L=${#K}; [ $L -gt 15 ] && K=$(echo "$K" | cut -c$((L-14))-$L)
mkdir -p /tmp/perf2-$name
i=1
while [ $i -le $runs ]; do
  n=0; while sudo -n pidin -F '%a %N' | grep -qF "$K"; do n=$((n+1)); [ $n -gt 30 ] && break; sleep 1; done
  f=/tmp/perf2-$name/$name-$i.json; rm -f $f
  /data/home/qnxuser/v1g2/v1g2-run.sh $dir $bin --bundle $bundle --timing-json $f "$@" > /tmp/perf2-$name/run-$i.log 2>&1
  rc=$?
  echo "run $i rc=$rc"
  [ $rc -eq 3 ] && { sleep 2; continue; }
  i=$((i+1)); sleep 2
done
