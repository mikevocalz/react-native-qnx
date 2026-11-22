#!/bin/sh
# Create (first time) and mount the ext4 work image that lives on the T9.
# The T9's virtiofs mount refuses to create a file whose owner-read bit is off,
# which std::filesystem::copy does (syncqt fails); a loop-mounted ext4 file has
# normal Linux semantics while every byte still lands on the T9.
set -eu
img=/t9/clones/distribution3-build/work.ext4
[ -e "$img" ] || { truncate -s 120G "$img"; mkfs.ext4 -q -F -E lazy_itable_init=1,lazy_journal_init=1 "$img"; }
mkdir -p /build
mountpoint -q /build || mount -o loop,noatime "$img" /build
df -h /build | tail -1
