#!/bin/sh
# usage: shot.sh <name>  -> docs/evidence/pi400-v1-gates-2026-11-24/frames/<name>.png (board-side screenshot)
E=/Volumes/QNX/clones/v1g/rnq/docs/evidence/pi400-v1-gates-2026-11-24/frames
K="-i $HOME/qnx-pi400-8.0.5/pr102-ssh/id_ed25519 -o IdentitiesOnly=yes"
ssh $K qnxuser@192.168.2.2 "sudo -n screenshot -file=/tmp/shot-$1.bmp" 2>/dev/null
scp -q $K qnxuser@192.168.2.2:/tmp/shot-$1.bmp /private/tmp/claude-501/shot-$1.bmp 2>/dev/null
sips -s format png /private/tmp/claude-501/shot-$1.bmp --out $E/$1.png >/dev/null 2>&1
ssh $K qnxuser@192.168.2.2 "rm -f /tmp/shot-$1.bmp" 2>/dev/null
echo $E/$1.png
