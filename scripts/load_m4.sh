#!/bin/sh
# SPDX-License-Identifier: MIT
# Load / start / stop the M4 firmware from Linux (A7) via remoteproc.
#
#   ./load_m4.sh start motor_ctrl.elf   # copy fw to /lib/firmware then start
#   ./load_m4.sh stop
#   ./load_m4.sh status
#
# On ST OpenSTLinux the remoteproc node is usually /sys/class/remoteproc/remoteproc0.

RPROC=/sys/class/remoteproc/remoteproc0
FWDIR=/lib/firmware

usage() { echo "usage: $0 {start <fw.elf>|stop|status}"; exit 1; }
[ -d "$RPROC" ] || { echo "ERROR: $RPROC not found (remoteproc not enabled?)"; exit 1; }

case "$1" in
  start)
    [ -n "$2" ] || usage
    cp "$2" "$FWDIR"/ || exit 1
    echo "$(basename "$2")" > "$RPROC/firmware"
    echo start > "$RPROC/state"
    echo "started $(basename "$2"); state=$(cat $RPROC/state)"
    ls /dev/ttyRPMSG* 2>/dev/null && echo "RPMsg tty ready" || echo "no ttyRPMSG yet (check M4 fw creates a channel)"
    ;;
  stop)
    echo stop > "$RPROC/state"
    echo "stopped; state=$(cat $RPROC/state)"
    ;;
  status)
    echo "state=$(cat $RPROC/state 2>/dev/null)"
    echo "firmware=$(cat $RPROC/firmware 2>/dev/null)"
    ls /dev/ttyRPMSG* 2>/dev/null || echo "no ttyRPMSG"
    ;;
  *) usage ;;
esac
