#!/bin/sh
# Exec target of the .desktop entry: the shell launches it, the host connects back.
export WAYLAND_DEBUG= QT_QPA_PLATFORM=wayland QT_QUICK_BACKEND=software
exec $HOME/rnq-build/host/bin/rnqnx_marathon_host --app-id org.rnqnx.marathon.probe --app-name Probe --component-name RNQnxInputProbe >> $HOME/ms-evidence/run/host.log 2>> $HOME/ms-evidence/run/host.err
