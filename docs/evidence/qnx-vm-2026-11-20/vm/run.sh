#!/bin/sh
# Boots the QEMU-virt aarch64 QNX 8.0.5 VM. ssh: -p 2222 root@127.0.0.1
D=$(cd "$(dirname "$0")" && pwd)
exec qemu-system-aarch64 -machine ${MACH:-virt,gic-version=3} ${ACCEL:--accel tcg -cpu cortex-a57} -smp 4 -m 3G \
 -kernel "$D/ifs.bin" \
 -drive file="$D/disk-qemu",format=raw,if=none,id=drv0 -device virtio-blk-device,drive=drv0 \
 -netdev user,id=net0,hostfwd=tcp:127.0.0.1:2222-:22 -device virtio-net-device,netdev=net0 \
 -object rng-random,filename=/dev/urandom,id=rng0 -device virtio-rng-device,rng=rng0 \
 -display none -serial "${SERIAL:-file:$D/serial.log}" -monitor unix:/private/tmp/qnxvm.mon,server,nowait "$@"
