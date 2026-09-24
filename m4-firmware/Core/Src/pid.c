/* SPDX-License-Identifier: MIT */
/*
 * pid.c - positional PID with derivative-on-measurement and anti-windup.
 *
 * Derivative is taken on the measurement (not the error) to avoid the
 * "derivative kick" when the setpoint changes. Anti-windup uses conditional
 * integration: the integral only accumulates when the output is not saturated
 * in the same direction as the error.
 */
#include "pid.h"

void pid_init(pid_t *p, float kp, float ki, float kd,
	      float dt, float out_min, float out_max)
{
	p->kp = kp;
	p->ki = ki;
	p->kd = kd;
	p->dt = dt;
	p->out_min = out_min;
	p->out_max = out_max;
	pid_reset(p);
}

void pid_set_gains(pid_t *p, float kp, float ki, float kd)
{
	p->kp = kp;
	p->ki = ki;
	p->kd = kd;
}

void pid_reset(pid_t *p)
{
	p->integ = 0.0f;
	p->prev_meas = 0.0f;
}

float pid_update(pid_t *p, float setpoint, float measurement)
{
	float error = setpoint - measurement;

	/* Proportional */
	float out = p->kp * error;

	/* Derivative on measurement (negated): -Kd * d(meas)/dt */
	float dmeas = (measurement - p->prev_meas) / p->dt;
	out -= p->kd * dmeas;
	p->prev_meas = measurement;

	/* Tentative integral term */
	float integ_next = p->integ + p->ki * error * p->dt;
	float out_with_i = out + integ_next;

	/* Anti-windup: only commit the integral if it does not push an
	 * already-saturated output further into saturation. */
	if (out_with_i > p->out_max) {
		if (error < 0.0f)
			p->integ = integ_next;   /* error pulls back -> allow */
		out_with_i = p->out_max;
	} else if (out_with_i < p->out_min) {
		if (error > 0.0f)
			p->integ = integ_next;
		out_with_i = p->out_min;
	} else {
		p->integ = integ_next;       /* not saturated -> accumulate */
	}

	return out_with_i;
}
