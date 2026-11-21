set -e
source /work/qnx800/qnxsdp-env.sh >/dev/null 2>&1
cd /work/dist-root2/probe
mkdir -p out/v1 out/v2
Q="qcc -Vgcc_ntoaarch64le"
$Q -shared -fPIC -Wl,-soname,libfoo.so.1 -o out/v1/libfoo.so.1 foo1.c
$Q -shared -fPIC -Wl,-soname,libfoo.so.1 -o out/v2/libfoo.so.1 foo2.c
# main_abs: build-host style absolute DT_RPATH. main_origin: DT_RPATH with $ORIGIN.
$Q -o out/main_abs main.c -Lout/v1 -l:libfoo.so.1 -Wl,-rpath,/data/dist-probe/buildhost
$Q -o out/main_origin main.c -Lout/v1 -l:libfoo.so.1 '-Wl,-rpath,$ORIGIN/lib'
for f in main_abs main_origin; do echo "$f: $(ntoaarch64-readelf -d out/$f | grep -E 'RPATH|RUNPATH' | sed 's/^ *0x[0-9a-f]* //')"; done
