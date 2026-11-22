source /t9/clones/distribution3-build/env.sh
export APP_SRC=/t9/clones/distribution3
export HERMES_QNX_DIR=/t9/clones/distribution3-build/deps/hermes-qnx-lean
export HERMES_QNX_SRC=/work/rn-source/third-party/hermes-260318099.0.5
export RN_DEPS_DIR=/work/pr102-consumer/build/qnx/native/_deps
source /t9/clones/distribution3/scripts/qt-public/layout.sh
s=$(date +%s); echo "app start $(date -u +%FT%TZ)"
/t9/clones/distribution3/scripts/qt-public/build-app-qnx.sh > /t9/clones/distribution3-build/logs-app/app-build.log 2>&1; rc=$?
echo "app exit=$rc wall=$(( $(date +%s) - s ))s $(date -u +%FT%TZ)"
