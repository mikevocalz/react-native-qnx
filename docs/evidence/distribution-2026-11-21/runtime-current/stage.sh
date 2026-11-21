set -euo pipefail
source /work/qnx800/qnxsdp-env.sh >/dev/null 2>&1
export QT_PUBLIC_ROOT=/work/qt-6.12.0-public QT_PUBLIC_BUILD=/home/qnxbuild/qt-6.12.0-public
export IMAGEFORMATS_BUILD=/home/qnxbuild/dist-build2/imageformats-qnx IMAGEFORMATS_SRC=/work/dist-root2/src
export QT_EVAL_MANIFEST=/nonexistent
cd /work/dist-root2/scripts
python3 -I stage-runtime.py /work/dist-root2/runtime-current /work/dist-root2/audit-base.json /work/dist-root2/audit-imageformats.json /work/dist-root2/audit-svg.json > /work/dist-root2/logs/stage-runtime.log 2>&1; echo stage rc=$?
tail -30 /work/dist-root2/logs/stage-runtime.log
