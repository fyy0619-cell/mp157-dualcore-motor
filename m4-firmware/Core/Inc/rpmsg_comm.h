/* SPDX-License-Identifier: MIT */
/*
 * rpmsg_comm.h - text protocol over the OpenAMP/RPMsg channel to the A7.
 *
 * Downlink (A7 -> M4):  "RUN 1|0" | "SPD <rpm>" | "PID <kp> <ki> <kd>"
 * Uplink   (M4 -> A7):  "T <ms> <rpm> <target> <duty> <flags>"
 */
#ifndef RPMSG_COMM_H_
#define RPMSG_COMM_H_

#include <stdint.h>

/* Command parsed out of a downlink line. */
typedef struct {
	int      have_run;   float run;      /* 1/0 enable */
	int      have_spd;   float spd;      /* target rpm */
	int      have_pid;   float kp, ki, kd;
} motor_cmd_t;

/* Parse one received text line into cmd. Returns 1 if any field was set. */
int rpmsg_parse_line(const char *line, motor_cmd_t *cmd);

/* Format a telemetry line into buf (returns length). */
int rpmsg_format_telemetry(char *buf, int buflen, uint32_t ms,
			   float rpm, float target, float duty, uint32_t flags);

/* Transport hooks wired to OpenAMP in main.c: */
void rpmsg_comm_init(void);                 /* set up endpoint */
void rpmsg_send_line(const char *line, int len);

#endif /* RPMSG_COMM_H_ */
