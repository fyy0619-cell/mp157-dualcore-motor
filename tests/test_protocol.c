/* SPDX-License-Identifier: MIT */
/* Host unit test: RPMsg text protocol parse and telemetry format. */
#include "rpmsg_comm.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

int main(void)
{
	motor_cmd_t c;

	assert(rpmsg_parse_line("SPD 1200", &c) && c.have_spd && c.spd == 1200.0f);
	assert(rpmsg_parse_line("RUN 1", &c) && c.have_run && c.run == 1.0f);
	assert(rpmsg_parse_line("PID 0.5 0.1 0.01", &c) && c.have_pid &&
	       c.kp == 0.5f && c.ki == 0.1f && c.kd == 0.01f);
	assert(!rpmsg_parse_line("GARBAGE", &c));

	char buf[64];
	int n = rpmsg_format_telemetry(buf, sizeof(buf), 1234,
				       100.5f, 100.0f, 0.42f, 2);
	printf("telemetry: %s", buf);
	assert(n > 0);
	assert(strncmp(buf, "T 1234 100.5 100.0 0.420 2", 26) == 0);

	printf("PROTOCOL OK\n");
	return 0;
}
