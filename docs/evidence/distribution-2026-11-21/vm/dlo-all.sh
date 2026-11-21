#!/bin/sh
# QNX VM. dlopen(RTLD_NOW) every ELF file of the staged runtime, one process per file
# so a failure cannot hide another. LD_LIBRARY_PATH holds only libeventfd.so.1 (missing
# from the VM image) and the QNX Screen libraries; Qt is found through DT_RUNPATH.
cd /data/qt612-public || exit 1
vm=/data/vm
export LD_LIBRARY_PATH=/data/dist-sys:$vm/screen/lib:$vm/screen/lib/dll:$vm/screen/gfx:/system/lib:/proc/boot
n=0; bad=0
for f in $(find . -type f -name '*.so*' | sort); do
  n=$((n+1))
  out=$(/data/dist-dlo "$f" 2>&1 | head -1)
  case "$out" in ok*) echo "$out";; *) bad=$((bad+1)); echo "$out";; esac
done
echo "files=$n failed=$bad"
