#!/bin/sh
# SPDX-License-Identifier: MIT
# Host unit tests for the M4 pure-logic modules (no HAL needed).
set -e
INC="m4-firmware/Core/Inc"
SRC="m4-firmware/Core/Src"

cc -Wall -I"$INC" tests/test_pid.c      "$SRC/pid.c"       -lm -o /tmp/t_pid   && /tmp/t_pid
cc -Wall -I"$INC" tests/test_encoder.c  "$SRC/encoder.c"   -lm -o /tmp/t_enc   && /tmp/t_enc
cc -Wall -I"$INC" tests/test_protocol.c "$SRC/rpmsg_comm.c"     -o /tmp/t_proto && /tmp/t_proto

echo "ALL UNIT TESTS PASSED"
