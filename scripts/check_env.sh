#!/bin/sh
# SPDX-License-Identifier: MIT
# M0 pre-flight: verify the board supports the heterogeneous (remoteproc+RPMsg)
# path before building anything on top. Run on the STM32MP157 (A7/Linux).

echo "== remoteproc =="
if [ -d /sys/class/remoteproc/remoteproc0 ]; then
  echo "OK  /sys/class/remoteproc/remoteproc0 present"
  echo "    state=$(cat /sys/class/remoteproc/remoteproc0/state 2>/dev/null)"
else
  echo "MISSING remoteproc0 -> enable CONFIG_REMOTEPROC + STM32 remoteproc in kernel"
fi

echo "== rpmsg kernel bits =="
zcat /proc/config.gz 2>/dev/null | grep -E "REMOTEPROC|RPMSG" || \
  echo "  (config.gz absent; check CONFIG_RPMSG_TTY / CONFIG_RPMSG_VIRTIO)"

echo "== rpmsg tty devices =="
ls /dev/ttyRPMSG* 2>/dev/null || echo "  none yet (appears after M4 fw starts a channel)"

echo "== firmware dir =="
ls -l /lib/firmware 2>/dev/null | head -5

echo "== hint =="
echo "  next: build M4 echo fw in STM32CubeIDE, then: scripts/load_m4.sh start <fw.elf>"
