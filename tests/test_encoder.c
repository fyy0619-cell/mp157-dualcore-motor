/* SPDX-License-Identifier: MIT */
/* Host unit test: encoder count -> rpm conversion and 16-bit wraparound. */
#include "encoder.h"
#include <assert.h>
#include <stdio.h>
#include <math.h>

int main(void)
{
	encoder_t e;
	encoder_init(&e, 1000, 0.001f);            /* 1000 cpr, 1 ms tick */
	encoder_update(&e, 0);
	float rpm = encoder_update(&e, 100);       /* +100 counts in 1 ms */
	/* 100/1000 rev per ms => 0.1 * 60000 = 6000 rpm */
	printf("rpm=%.1f (expect 6000)\n", rpm);
	assert(fabsf(rpm - 6000.0f) < 1.0f);

	/* 16-bit wraparound: 65500 -> 20 is +56 counts, not -65480. */
	encoder_init(&e, 1000, 0.001f);
	encoder_update(&e, 65500);
	rpm = encoder_update(&e, 20);
	printf("wrap rpm=%.1f (expect > 0)\n", rpm);
	assert(rpm > 0.0f);

	printf("ENCODER OK\n");
	return 0;
}
