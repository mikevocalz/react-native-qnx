#!/bin/sh
# usage: deploy.sh playground|expo  (copies the built consumer app out of the container to the Pi)
export DOCKER_HOST=unix:///Users/mikevocalz/.colima/default/docker.sock
case $1 in playground) id=org.rnqnx.playground;; expo) id=org.rnqnx.routerexample;; esac
S=/Volumes/QNX/clones/v1g/stage
docker exec qt-v1g bash -c "cp /build/build/app-$1/consumer/qnx/$id/$id /t9/clones/v1g/stage/$1/$id"
scp -q -i $HOME/qnx-pi400-8.0.5/pr102-ssh/id_ed25519 -o IdentitiesOnly=yes -o LogLevel=ERROR $S/$1/$id qnxuser@192.168.2.2:/data/home/qnxuser/v1g2/$1/$id
/Volumes/QNX/clones/v1g/session/pi.sh "chmod +x /data/home/qnxuser/v1g2/$1/$id; sha256sum /data/home/qnxuser/v1g2/$1/$id"
