#!/bin/sh
# SIGTERM only. pidin truncates names to their last 18 characters, so match on the last 15.
# Never touches screen, drm-rpi4 or the window manager.
key() { n=$1; l=${#n}; [ $l -gt 15 ] && n=$(echo "$n" | cut -c$((l-14))-$l); echo "$n"; }
for name in "$@"; do
  k=$(key $name)
  for p in $(sudo -n pidin -F '%a %N' | grep -F "$k" | while read a n; do echo $a; done); do kill $p; done
done
n=0
while :; do
  left=0
  for name in "$@"; do k=$(key $name); sudo -n pidin -F '%a %N' | grep -qF "$k" && left=1; done
  [ $left = 0 ] && { echo stopped; exit 0; }
  n=$((n+1)); [ $n -gt 10 ] && { echo "still running"; exit 1; }; sleep 1
done
