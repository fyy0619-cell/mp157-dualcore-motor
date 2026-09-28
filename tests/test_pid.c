/* SPDX-License-Identifier: MIT */
/* Host unit test: PID converges on a first-order plant, and clamps output. */
#include "pid.h"
#include <assert.h>
#include <stdio.h>
#include <math.h>

int main(void)
{
	/* Simulated first-order plant: v' = (-v + k*u)/tau */
	pid_t p;
	pid_init(&p, 0.02f, 0.5f, 0.0f, 0.001f, -1.0f, 1.0f);
	float v = 0.0f, setpoint = 100.0f, k = 200.0f, tau = 0.05f, dt = 0.001f;
	for (int i = 0; i < 5000; i++) {
		float u = pid_update(&p, setpoint, v);
		v += (-v + k * u) * dt / tau;
	}
	printf("final v=%.2f (target %.0f)\n", v, setpoint);
	assert(fabsf(v - setpoint) < 2.0f);          /* steady-state error < 2 */

	/* Output clamp: huge Kp must saturate to out_max. */
	pid_t q;
	pid_init(&q, 100.0f, 0.0f, 0.0f, 0.001f, -1.0f, 1.0f);
	assert(pid_update(&q, 1000.0f, 0.0f) <= 1.0f);
	assert(pid_update(&q, -1000.0f, 0.0f) >= -1.0f);

	printf("PID OK\n");
	return 0;
}
