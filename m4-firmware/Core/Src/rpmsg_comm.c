/* SPDX-License-Identifier: MIT */
/*
 * rpmsg_comm.c - protocol parse/format (unit-testable) + OpenAMP transport hooks.
 *
 * The parse/format functions are pure and testable on a host. rpmsg_comm_init()
 * and rpmsg_send_line() are wired to the CubeMX/OpenAMP MW (openamp.h, VIRT_UART
 * or rpmsg endpoint) in your project.
 */
#include "rpmsg_comm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int rpmsg_parse_line(const char *line, motor_cmd_t *cmd)
{
	memset(cmd, 0, sizeof(*cmd));

	if (!strncmp(line, "RUN ", 4)) {
		cmd->run = (float)atoi(line + 4);
		cmd->have_run = 1;
	} else if (!strncmp(line, "SPD ", 4)) {
		cmd->spd = (float)atof(line + 4);
		cmd->have_spd = 1;
	} else if (!strncmp(line, "PID ", 4)) {
		if (sscanf(line + 4, "%f %f %f", &cmd->kp, &cmd->ki, &cmd->kd) == 3)
			cmd->have_pid = 1;
	}
	return cmd->have_run || cmd->have_spd || cmd->have_pid;
}

int rpmsg_format_telemetry(char *buf, int buflen, uint32_t ms,
			   float rpm, float target, float duty, uint32_t flags)
{
	return snprintf(buf, buflen, "T %lu %.1f %.1f %.3f %lu\n",
			(unsigned long)ms, rpm, target, duty,
			(unsigned long)flags);
}

/* --- OpenAMP transport (project-specific) --- */
void rpmsg_comm_init(void)
{
	/* TODO(OpenAMP): MX_OPENAMP_Init(RPMSG_REMOTE, NULL);
	 * create endpoint / VIRT_UART and register RX callback that calls
	 * rpmsg_parse_line() and applies motor_cmd_t. */
}

void rpmsg_send_line(const char *line, int len)
{
	(void)line;
	(void)len;
	/* TODO(OpenAMP): VIRT_UART_Transmit(&huart, (uint8_t*)line, len);
	 * or OPENAMP_send(&ept, line, len); */
}
