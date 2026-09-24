/* SPDX-License-Identifier: MIT */
/*
 * encoder.c - quadrature speed measurement from a timer count.
 *
 * Hardware-independent: the caller reads the timer counter (CubeMX-generated,
 * e.g. __HAL_TIM_GET_COUNTER(&htimX)) and passes it in. This keeps the speed
 * maths unit-testable without HAL.
 */
#include "encoder.h"

void encoder_init(encoder_t *e, uint32_t cpr, float dt)
{
	e->cpr = cpr ? cpr : 1;
	e->dt = dt;
	e->last_count = 0;
	e->rpm = 0.0f;
}

float encoder_update(encoder_t *e, int32_t raw_count)
{
	/* Signed delta with 16-bit timer wraparound handling. */
	int32_t delta = raw_count - e->last_count;
	if (delta > 32767)
		delta -= 65536;
	else if (delta < -32768)
		delta += 65536;
	e->last_count = raw_count;

	/* revolutions this tick = delta / cpr ; rpm = rev/tick * (60 / dt) */
	e->rpm = ((float)delta / (float)e->cpr) * (60.0f / e->dt);
	return e->rpm;
}
