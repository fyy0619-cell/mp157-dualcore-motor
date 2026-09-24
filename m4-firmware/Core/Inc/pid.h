/* SPDX-License-Identifier: MIT */
/*
 * pid.h - positional PID controller with output clamping and anti-windup.
 * Hardware-independent and unit-testable (no HAL dependency).
 */
#ifndef PID_H_
#define PID_H_

typedef struct {
	float kp, ki, kd;   /* gains */
	float dt;           /* control period in seconds (e.g. 0.001 for 1 kHz) */
	float out_min;      /* output lower clamp (e.g. -1.0 duty) */
	float out_max;      /* output upper clamp (e.g. +1.0 duty) */
	float integ;        /* integral accumulator (state) */
	float prev_meas;    /* previous measurement, for derivative-on-measurement */
} pid_t;

/* Initialise controller with gains, control period and output limits. */
void pid_init(pid_t *p, float kp, float ki, float kd,
	      float dt, float out_min, float out_max);

/* Update gains at runtime (e.g. from the web UI). */
void pid_set_gains(pid_t *p, float kp, float ki, float kd);

/* Reset internal state (call on enable / setpoint jump / stall stop). */
void pid_reset(pid_t *p);

/* One control step: returns clamped control output for (setpoint - measurement). */
float pid_update(pid_t *p, float setpoint, float measurement);

#endif /* PID_H_ */
