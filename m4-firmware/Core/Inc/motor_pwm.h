/* SPDX-License-Identifier: MIT */
/*
 * motor_pwm.h - H-bridge (TB6612) motor drive: signed duty -> PWM + direction.
 */
#ifndef MOTOR_PWM_H_
#define MOTOR_PWM_H_

#include <stdint.h>

/* Initialise PWM timer and direction GPIOs (wraps CubeMX handles). */
void motor_pwm_init(void);

/* Set drive from a signed duty in [-1.0, +1.0].
 * Sign selects TB6612 IN1/IN2 direction; magnitude sets PWM compare. */
void motor_pwm_set(float duty);

/* Coast/stop the motor (both inputs low). */
void motor_pwm_stop(void);

#endif /* MOTOR_PWM_H_ */
