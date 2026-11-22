#!/bin/bash
# Timed JS round trip: edit app text -> bundle-qnx -> hermesc -> push to VM -> run on VM -> pull PNG.
# Native runner is the already-deployed /data/vm/app/org.rnqnx.showcase (not rebuilt).
set -u
export PATH=/Volumes/QNX/cache/dp-pnpm/node_modules/.bin:$PATH
R=/Volumes/QNX/clones/developer-path; S=/Volumes/QNX/qnxvm; E=/Volumes/QNX/clones/dp-evidence/vm
HC=$(ls $R/node_modules/.pnpm/hermes-compiler@*/node_modules/hermes-compiler/hermesc/osx-bin/hermesc)
cd $R
for n in 1 2 3; do
  sed -i '' "s/dashboard on React Native QNX.*'/dashboard on React Native QNX round $n'/" apps/dashboard/index.js
  t0=$(date +%s.%N 2>/dev/null || python3 -c 'import time;print(time.time())')
  now() { python3 -c 'import time;print(time.time())'; }
  t0=$(now)
  pnpm qnx bundle-qnx --app dashboard --out build/dashboard.bundle.js >$E/rt$n-bundle.log 2>&1; t1=$(now)
  $HC -O -emit-binary -out build/dashboard.hbc build/dashboard.bundle.js >$E/rt$n-hermesc.log 2>&1; t2=$(now)
  cat build/dashboard.hbc | $S/sshq 'cat > /data/dp/bundles/proof-dashboard.hbc'; t3=$(now)
  $S/sshq 'sh /data/vm/capture.sh /data/vm/app /data/dp/bundles /data/dp/out dashboard' >$E/rt$n-run.txt 2>&1; t4=$(now)
  $S/scpq root@127.0.0.1:/data/dp/out/dashboard.png $E/rt$n.png; t5=$(now)
  python3 - "$t0" "$t1" "$t2" "$t3" "$t4" "$t5" "$n" <<'P' | tee -a $E/roundtrip.txt
import sys
t=[float(x) for x in sys.argv[1:7]]
print("round %s: bundle %.1fs hermesc %.1fs push %.1fs run %.1fs pull %.1fs total %.1fs"%((sys.argv[7],)+tuple(t[i+1]-t[i] for i in range(5))+(t[5]-t[0],)))
P
  shasum -a 256 $E/rt$n.png build/dashboard.hbc >> $E/roundtrip.txt
done
