/* SPDX-License-Identifier: MIT */
/*
 * app_control.c - application glue: 1 kHz control tick, telemetry, protection.
 *
 * This is the heart of the M4 firmware. Drop it into a CubeMX project that
 * provides: a timer at 1 kHz raising an interrupt (control tick), an encoder
 * timer, a PWM timer, and the OpenAMP middleware. Call app_control_tick() from
 * the 1 kHz timer ISR and app_control_poll() from the main super-loop.
 */
#include "pid.h"
#include "encoder.h"
#include "motor_pwm.h"
#include "rpmsg_comm.h"
#include <stdint.h>

#define CONTROL_HZ        1000
#define CONTROL_DT        (1.0f / CONTROL_HZ)
#define TELEMETRY_DECIM   20            /* 1 kHz / 20 = 50 Hz telemetry */
#define ENCODER_CPR       1560          /* x4 * PPR * gear; set per your motor */
#define STALL_DUTY_TH     0.4f          /* commanding >40% ... */
#define STALL_RPM_TH      5.0f          /* ...but <5 rpm => stall */
#define STALL_TICKS       300           /* for 300 ms */

#define FLAG_STALL        0x1u
#define FLAG_RUNNING      0x2u

static pid_t     s_pid;
static encoder_t s_enc;
static volatile float s_target_rpm = 0.0f;
static volatile int   s_running = 0;
static uint32_t  s_ms = 0;
static uint32_t  s_decim = 0;
static uint32_t  s_stall_cnt = 0;
static volatile uint32_t s_flags = 0;
static volatile float s_rpm = 0.0f, s_duty = 0.0f;

/* Provided by main.c/CubeMX: read encoder timer counter. */
extern int32_t board_read_encoder_count(void);

void app_control_init(void)
{
	pid_init(&s_pid, 0.0008f, 0.0040f, 0.00002f,
		 CONTROL_DT, -1.0f, 1.0f);      /* starting gains; tune via UI */
	encoder_init(&s_enc, ENCODER_CPR, CONTROL_DT);
	motor_pwm_init();
	rpmsg_comm_init();
	motor_pwm_stop();
}

/* Apply a parsed command (called from the RPMsg RX path). */
void app_control_apply(const motor_cmd_t *c)
{
	if (c->have_pid)
		pid_set_gains(&s_pid, c->kp, c->ki, c->kd);
	if (c->have_spd)
		s_target_rpm = c->spd;
	if (c->have_run) {
		int run = (c->run != 0.0f);
		if (run && !s_running) {
			pid_reset(&s_pid);
			s_stall_cnt = 0;
			s_flags &= ~FLAG_STALL;
		}
		s_running = run;
	}
}

/* Call from the 1 kHz control-timer ISR. Deterministic, no blocking. */
void app_control_tick(void)
{
	s_ms++;
	float rpm = encoder_update(&s_enc, board_read_encoder_count());
	s_rpm = rpm;

	float duty = 0.0f;
	if (s_running && !(s_flags & FLAG_STALL)) {
		duty = pid_update(&s_pid, s_target_rpm, rpm);
		s_flags |= FLAG_RUNNING;
	} else {
		s_flags &= ~FLAG_RUNNING;
		pid_reset(&s_pid);
	}
	motor_pwm_set(duty);
	s_duty = duty;

	/* Stall protection: high command but no motion -> latch stop. */
	if (s_running) {
		float ad = duty < 0 ? -duty : duty;
		float ar = rpm < 0 ? -rpm : rpm;
		if (ad > STALL_DUTY_TH && ar < STALL_RPM_TH) {
			if (++s_stall_cnt >= STALL_TICKS) {
				s_flags |= FLAG_STALL;
				s_running = 0;
				motor_pwm_stop();
			}
		} else {
			s_stall_cnt = 0;
		}
	}

	s_decim++;
}

/* Call from the main super-loop: emit telemetry at 50 Hz, service OpenAMP. */
void app_control_poll(void)
{
	if (s_decim >= TELEMETRY_DECIM) {
		s_decim = 0;
		char line[64];
		int n = rpmsg_format_telemetry(line, sizeof(line), s_ms,
					       s_rpm, s_target_rpm, s_duty, s_flags);
		if (n > 0)
			rpmsg_send_line(line, n);
	}
	/* TODO(OpenAMP): OPENAMP_check_for_message() to pump RX -> app_control_apply() */
}
