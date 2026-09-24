/* SPDX-License-Identifier: MIT */
/*
 * encoder.h - quadrature encoder speed measurement.
 * Uses a STM32 timer in encoder mode (hardware x4 decoding).
 */
#ifndef ENCODER_H_
#define ENCODER_H_

#include <stdint.h>

typedef struct {
	uint32_t cpr;        /* encoder counts per output revolution (x4 * PPR * gear) */
	float dt;            /* sampling period in seconds (control tick) */
	int32_t last_count;  /* previous raw timer count */
	float rpm;           /* last computed speed (revolutions per minute) */
} encoder_t;

/* cpr: effective counts per output-shaft revolution; dt: control period (s). */
void encoder_init(encoder_t *e, uint32_t cpr, float dt);

/* Call once per control tick with the current raw timer count.
 * Returns speed in RPM (also stored in e->rpm). Handles 16-bit wraparound. */
float encoder_update(encoder_t *e, int32_t raw_count);

#endif /* ENCODER_H_ */
