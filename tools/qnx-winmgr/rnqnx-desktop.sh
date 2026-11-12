#!/bin/sh
# QNX 8.0 Quick Start desktop with rnqnx-winmgr in place of fullscreen-winmgr.
# Same steps and order as the stock /usr/etc/startup/full_startup.sh (window
# manager, first-start checks, demolauncher); only the window manager binary
# differs. When rnqnx-winmgr is missing or exits within two seconds the stock
# fullscreen-winmgr starts instead, and when it stops later the stock desktop
# comes back, so the board always has a desktop.
wm=/data/rnqnx-winmgr/rnqnx-winmgr

if ! waitfor /dev/screen/ 15; then
    echo '---> waitfor /dev/screen (rnqnx-desktop.sh) FAILED!'
fi

if [ -d /dev/screen ]; then
    echo '---> Starting Window Manager...'
    started=
    if [ -x "$wm" ]; then
        echo "Using rnqnx-winmgr..."
        "$wm" -s demolauncher,st,gtk </dev/null >>/tmp/rnqnx-winmgr.log 2>&1 &
        wmpid=$!
        sleep 2
        if kill -0 "$wmpid" 2>/dev/null; then
            started=1
            # If it stops later (a crash, or `slay rnqnx-winmgr`), bring the
            # stock desktop back: fullscreen-winmgr, then a fresh launcher it
            # can see being created.
            (
                while kill -0 "$wmpid" 2>/dev/null; do sleep 5; done
                echo 'rnqnx-winmgr stopped; starting fullscreen-winmgr' >>/tmp/rnqnx-winmgr.log
                fullscreen-winmgr -s demolauncher,st,gtk &
                sleep 2
                slay demolauncher
                demolauncher -a /system/etc/desktop_files &
            ) </dev/null >/dev/null 2>&1 &
        else
            echo 'rnqnx-winmgr exited; falling back to fullscreen-winmgr'
        fi
    fi
    if [ -z "$started" ]; then
        echo "Using Full Screen Window Manager..."
        fullscreen-winmgr -s demolauncher,st,gtk &
    fi
else
    echo '---> /dev/screen check failed! NOT STARTING a window manager!!! ...'
fi

if [ ! -f /usr/etc/startup/first_startup_checks.lock ]; then
  echo '---> calling first_startup_checks.sh'
  export TERM=xterm-256color
  st -e /usr/etc/startup/first_startup_checks.sh
fi

if [ -d /dev/screen ]; then
    echo '---> Starting Demolauncher...'
    demolauncher -a /system/etc/desktop_files &
else
    echo '---> /dev/screen check failed! NOT STARTING Demolauncher!!! ...'
fi
