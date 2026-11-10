# QNX SDK build host on macOS (Colima + Docker)

This runbook records how the macOS build host for the Raspberry Pi 400 physical lane was set up, and how to bring it back after a stop. It was reconstructed on 2026-11-09 from the live host and the PR #102 supervised-run scripts. Anything not confirmed on the live host is marked **unconfirmed**.

The QNX SDP is licensed. The SDP, its license files and any SSH keys are never committed. Hostnames and addresses are also left out.

Related documents:

- [ADR-0010](../adr/0010-macos-qt-development-host.md): macOS as a development host. That ADR covers portable native work. This runbook covers the separate QNX cross-build host that runs inside a Linux container on the same Mac.
- [Raspberry Pi 400 bring-up](../getting-started/raspberry-pi-400.md)
- [QNX x86_64 VM on Windows](QNX_VM_WINDOWS.md): the older Windows workaround. It uses SDP 8.0.2 and a different target.
- `scripts/lib/qnx-target.sh`: the SSH defaults the physical lane uses.
- The final physical runner (`scripts/final-pi400-test.mjs`) and its procedure (`docs/validation/pi400-final-test.md`) are introduced by PR #102 and are not on `main` yet. The section on running the lane below follows that branch.

## What the host looks like

| Layer               | Value                                                                                             | Status                               |
| ------------------- | ------------------------------------------------------------------------------------------------- | ------------------------------------ |
| Mac                 | Apple silicon, macOS                                                                              | confirmed (Colima arch is `aarch64`) |
| Colima profile      | `default`, `vmType: vz`, `rosetta: true`, `mountType: virtiofs`, runtime `docker`, arch `aarch64` | confirmed                            |
| Colima size         | 2 CPUs, 4 GiB RAM, 100 GiB disk                                                                   | confirmed                            |
| Container           | `qnx805-build`, image `ubuntu:24.04`, command `sleep infinity`, restart policy `no`, runs as root | confirmed                            |
| Container arch      | `x86_64` (the QNX host tools are Linux x86_64, run through Rosetta)                               | confirmed                            |
| Mount               | one bind mount, macOS `/Users/mikevocalz/qnx-pi400-8.0.5` to `/work`, read-write                  | confirmed                            |
| Build user          | `qnxbuild` (uid 1001, home `/home/qnxbuild`) owns the Qt trees                                    | confirmed                            |
| Node / pnpm / CMake | Node 26.10.0, pnpm 12.8.1, CMake 3.31.9 (plus Ninja, Python 3, git)                               | confirmed                            |

The container has no extra environment variables beyond `PATH`. The `--platform` flag used when it was created is not recorded in the container config (**unconfirmed**; the architecture above is what the running container reports).

`/work` lives on the Mac's internal volume, not inside the Colima disk. The 100 GiB Colima disk holds the Docker image layers and container filesystem (6 GiB used). On 2026-11-09 `/work` had 5.5 GiB free at 99% use, which is close to the 3 GiB gate described below.

## Recreate the Colima profile

Only needed if the profile is gone. `scripts/qnx-sdk-container.sh` never does this.

    colima start --profile default \
      --vm-type vz --vz-rosetta --arch aarch64 \
      --cpu 2 --memory 4 --disk 100 \
      --mount-type virtiofs --runtime docker

The flags above are the profile values read from `~/.colima/default/colima.yaml`. The exact command used originally is **unconfirmed**. Raise `--cpu` and `--memory` if you have headroom: the supervised run used `CMAKE_BUILD_PARALLEL_LEVEL=1` to fit in 4 GiB.

Point the Docker CLI at the profile:

    export DOCKER_HOST=unix://$HOME/.colima/default/docker.sock

## Recreate the container

Only needed if `qnx805-build` is gone. The original `docker run` command is **unconfirmed**; this one reproduces the inspected configuration.

    docker run -d --name qnx805-build \
      --platform linux/amd64 \
      -v /Users/mikevocalz/qnx-pi400-8.0.5:/work -w /work \
      ubuntu:24.04 sleep infinity

Then install the toolchain packages inside it (build-essential-style tools, git, Ninja, Python 3, curl). The package list is **unconfirmed**. CMake 3.31.9 is in `/usr/local/bin`, so it was installed from the upstream release, not apt. Node 26.10.0 and pnpm 12.8.1 are the pins from `package.json` (`engines.node >= 26`, `packageManager pnpm@12.8.1`); install them so `node --version` prints `v26.10.0` and `pnpm --version` prints `12.8.1`. The supervised-run harness asserts both.

Create the `qnxbuild` user (uid 1001) and keep Qt and the SDP installer state under its home.

## Where the SDP comes from

The operator supplies a QNX Software Center account with an SDP 8.0 license. Nothing in this repository downloads or embeds it.

What is installed, confirmed from the live container:

- Install root: `/work/qnx800` (so on the Mac it is under the bind-mounted directory).
- `/work/qnx800/cti_package_set` contains `rpi4com.qnx.cti-pi400-805`.
- `/work/qnx800/cti_profile_id` holds the profile id (21 characters, `com.qnx.cti-pi400-805`).
- `/opt/qnx/qnxsoftwarecenter` holds the QNX Software Center command-line tool (`qnxsoftwarecenter_clt`). The QSC license and config live under `/home/qnxbuild/.qnx` (`license`, `qconfig`, `qnx_env.ini`). Do not copy or print those files.
- The package list the installer consumed is `/work/build/rpi4/qsc_install_packages.list` (395 lines, starting with `com.qnx.qnx800` 8.0.x and the Linux x86_64 host packages).

The restore log (`sdk-restore.log`) shows the two steps, run as `qnxbuild`:

    # 1. mirror the baseline repository (needs the operator's QNX login)
    /opt/qnx/qnxsoftwarecenter/qnxsoftwarecenter_clt \
      -url https://www.qnx.com/swcenter -mirrorBaseline qnx800 @options_file

    # 2. clean install of the listed packages under the CTI profile
    /opt/qnx/qnxsoftwarecenter/qnxsoftwarecenter_clt \
      -url https://www.qnx.com/swcenter -cleanInstall -setExperimentalEnabled=true \
      -setPolicy=conservative -destination /work/qnx800 \
      -importAndInstall /work/build/rpi4/qsc_install_packages.list \
      -profile com.qnx.cti-pi400-805 @options_file

`@options_file` carries the operator's credentials. Its contents are not recorded here. Where the package list itself came from (QNX's Pi 400 recipe versus a local export) is **unconfirmed**. The installer warned that one `net.devsddk` unit was replaced by a newer version, so a reinstall may differ from the installed set.

After install, check:

    docker exec qnx805-build bash -c \
      'source /work/qnx800/qnxsdp-env.sh && echo $QNX_HOST && echo $QNX_TARGET && which qcc'

Expected on this host: `QNX_HOST=/work/qnx800/host/linux/x86_64`, `QNX_TARGET=/work/qnx800/target/qnx`, `qcc` under `$QNX_HOST/usr/bin`.

## Qt 6.12 host and QNX builds

All under `/home/qnxbuild` in the container, built from `qt-everywhere-src-6.12.0` (source at `/work/qt-source/qt-everywhere-src-6.12.0`).

| Path                              | What it is                                                                          |
| --------------------------------- | ----------------------------------------------------------------------------------- |
| `/home/qnxbuild/qt612-host`       | Linux host Qt 6.12.0 build tree                                                     |
| `/home/qnxbuild/qt612-host-sdk`   | Installed host Qt, used as `QT_HOST_PATH` (has `bin/` with the host tools)          |
| `/home/qnxbuild/qt612-qnx`        | Qt 6.12.0 cross-built for QNX aarch64le, Release, Ninja, `qnx-aarch64le-qcc` mkspec |
| `/home/qnxbuild/qt612-qnx/qtbase` | Prefix passed as `CMAKE_PREFIX_PATH` (`lib/cmake/Qt6` is `Qt6_DIR`)                 |

The QNX Qt build used `CMAKE_TOOLCHAIN_FILE=/work/qt612-qnx-aarch64le.cmake`, a thin wrapper over Qt's own `qnx-toolchain-aarch64le.cmake` from the Qt 6.12.0 sources. It adds `-Vgcc_ntoaarch64le -D_QNX_SOURCE`, `CMAKE_SYSTEM_VERSION 800`, and the QNX `ranlib` and `nm` tools. The full `configure` command line is **unconfirmed** (the `config_*.opt` files in the build trees record it; read them before rebuilding).

Leftover trees (`qt612-qnx-before-moc-fix`, `*-initial-config`, `qt-pch-backups`) are earlier attempts. Do not rely on them.

## Environment file

`/work/pr102-sdk-env.sh` is the file the supervised scripts source. As a template (paths are the ones on this host; `pr102-candidate` is the PR checkout, change it per candidate):

    #!/usr/bin/env bash
    source /work/qnx800/qnxsdp-env.sh
    export RNQNX_HERMES_SOURCE_DIR=/work/<checkout>/third-party/hermes-260318099.0.5
    export RNQNX_HERMES_BUILD_DIR=/work/<checkout>/build/hermes-qnx-aarch64
    export RNQNX_HOST_HERMESC=/work/<checkout>/node_modules/.pnpm/hermes-compiler@260318099.0.5/node_modules/hermes-compiler/hermesc/linux64-bin/hermesc
    export CMAKE_PREFIX_PATH=/home/qnxbuild/qt612-qnx/qtbase
    export Qt6_DIR=/home/qnxbuild/qt612-qnx/qtbase/lib/cmake/Qt6
    export QT_HOST_PATH=/home/qnxbuild/qt612-host-sdk
    export CMAKE_BUILD_PARALLEL_LEVEL=2

The supervised run lowered `CMAKE_BUILD_PARALLEL_LEVEL` to 1 before executing. `qnxsdp-env.sh` exports `QNX_HOST` and `QNX_TARGET`, which the runner requires.

## Free-disk requirement

The supervised harness refuses to start unless `/work` has at least 3 GiB free:

    test "$(df -Pk /work | awk 'NR==2 {print $4}')" -ge 3145728

While the runner executes, it also polls `/work` every 5 seconds and sends `SIGTERM` if free space drops below 1 GiB. Both checks measure the Mac volume that backs the bind mount, not the Colima disk. Check from the Mac with `df -h /Users/mikevocalz` and free space there (old `*.log`, `*.qnx`, `.bundle.js` artifacts in the bind-mounted directory are the usual candidates) before a run.

## Recover when the Colima VM stops

When the VM stops, every container exits, and `qnx805-build` has restart policy `no`, so it stays down. Nothing inside is lost: `/work` is on the Mac, and `/home/qnxbuild` and the image layers are on the Colima disk, which persists.

    scripts/qnx-sdk-container.sh status   # report only
    scripts/qnx-sdk-container.sh start    # start Colima, then the container

`start` runs `colima start --profile default` if the profile is stopped, exports `DOCKER_HOST` for the profile, runs `docker start qnx805-build`, and then checks that `/work/qnx800/qnxsdp-env.sh` exists in the container. It prints one `PASS` or `FAIL` line per check and exits non-zero on any failure. It does not create or remove a profile or container: if either is missing it reports `FAIL` and you follow the recreate sections above. `COLIMA_PROFILE`, `QNX_SDK_CONTAINER` and `QNX_SDK_ENV_FILE` override the defaults.

Manual equivalent:

    colima start --profile default
    export DOCKER_HOST=unix://$HOME/.colima/default/docker.sock
    docker start qnx805-build

A file-system check was needed once after an unclean stop; the repair logs (`vm-filesystem-*.log`) show `e2fsck` passing on the Colima data disk afterwards. The exact repair procedure is **unconfirmed**; stop the VM with `colima stop` rather than killing it.

## Dedicated SSH key for the Pi 400

Use a key made for this lane, not your personal key. No key or address belongs in the repository, a log or a PR.

1. In the container, as the user that will run the lane, generate a new key in a private directory:

       umask 077
       mkdir -p /work/<private-dir>
       ssh-keygen -t ed25519 -N '' -C 'rnqnx-pi400-lane' -f /work/<private-dir>/id_ed25519

   `/work` is bind-mounted from the Mac, so keep the directory out of any git checkout and out of backups you share. A passphrase-protected key plus an agent is also fine; the runner uses `IdentitiesOnly=yes` with the file named in `QNX_SSH_KEY`.

2. Install the public key on the Pi 400 for the non-root `qnxuser` account only. Use the recovery console or an existing authorized session, append the `.pub` line to `/data/home/qnxuser/.ssh/authorized_keys`, and keep the directory `0700` and the file `0600`. Never install it for `root`; the runner rejects root and numeric usernames.
3. Record the target's host key out of band and add it to the container's `known_hosts`. The runner uses `StrictHostKeyChecking=yes`, so an unknown host fails closed.
4. Test with a batch-mode login that runs `id -u` and expects a non-zero value.
5. To retire the key, remove its line from `authorized_keys` on the target and delete the private file.

## Run the Pi 400 lane from the container

These steps follow the procedure in PR #102 (`docs/validation/pi400-final-test.md` there). Run them inside the container as `qnxbuild`, from a clean checkout of the candidate under `/work`.

    docker exec -it -u qnxbuild qnx805-build bash
    source /work/pr102-sdk-env.sh        # or your own copy of the template above
    cd /work/<checkout>
    node scripts/final-pi400-test.mjs plan

`plan` lists the stages as `not-run` and never runs SSH, installs, builds or changes the device. Then, with a consumer project already built to a matching Hermes `.hbc` and prepared with `project prepare-native`:

    export RNQNX_PI_BOARD=pi400
    export RNQNX_FINAL_TEST_CONFIRM=pi400
    export QNX_TARGET_USER=qnxuser
    export QNX_TARGET_IP=<verified-pi400-hostname>
    export QNX_SSH_KEY=/work/<private-dir>/id_ed25519
    export QNX_TARGET_DIR=/data/home/qnxuser/<unique-run-directory>
    unset QNX_SSH_OPTS
    node scripts/final-pi400-test.mjs execute --consumer-project /work/<consumer>

Variables the supervised run also set, for the Qt runtime already installed on the target:

| Variable                  | Used for                                                                          |
| ------------------------- | --------------------------------------------------------------------------------- |
| `RNQNX_PI_QT_FONTDIR`     | existing font directory on the target (the run used `/usr/share/fonts/dejavu`)    |
| `RNQNX_PI_LIBRARY_PATH`   | colon-separated target library directories, placed after `.` in `LD_LIBRARY_PATH` |
| `RNQNX_PI_QT_PLUGIN_PATH` | target Qt plugin directory (must contain `platforms/libqqnx.so`)                  |
| `RNQNX_PI_CONFIG`         | build configuration; the run used `Debug`                                         |
| `QNX_SSH_PORT`            | optional; default 22                                                              |

`QNX_SSH_OPTS` is refused by the runner. `QNX_HOST` and `QNX_TARGET` come from the SDP environment. The supervised run also set `CMAKE_BUILD_PARALLEL_LEVEL=1`, wrapped `execute` in `timeout 90m`, and ran the disk watchdog described above. `execute` touches physical hardware, so only run it when a person is supervising and a recovery path exists.

## Unconfirmed

- The original `colima start` and `docker run` commands, including the `--platform` value.
- The package list for the container's build tools, and how CMake 3.31.9 was installed.
- Where `qsc_install_packages.list` came from, and the contents of the QSC options file.
- The full Qt 6.12 `configure` commands for the host and QNX builds.
- The exact file-system repair steps used on the Colima disk.
- Whether `qnx805-build` should get a restart policy. It has `no` today.
