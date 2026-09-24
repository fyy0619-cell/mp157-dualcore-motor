/* SPDX-License-Identifier: MIT */
/*
 * motor_pwm.c - TB6612 H-bridge drive.
 *
 * INTEGRATION: the htimX PWM handle and the AIN1/AIN2 GPIO pins are generated
 * by STM32CubeMX. Replace the TODO markers with your project's handles.
 * Direction truth table (TB6612): IN1=1,IN2=0 forward; IN1=0,IN2=1 reverse.
 */
#include "motor_pwm.h"
/* #include "main.h"  // CubeMX: htim, GPIO defines */

#define PWM_MAX_COMPARE 1000u   /* timer ARR; sets PWM resolution */

void motor_pwm_init(void)
{
	/* TODO(CubeMX): HAL_TIM_PWM_Start(&htimX, TIM_CHANNEL_y); */
	motor_pwm_stop();
}

static void set_dir_forward(void)
{
	/* TODO(CubeMX): HAL_GPIO_WritePin(AIN1_GPIO_Port, AIN1_Pin, GPIO_PIN_SET);
	 *               HAL_GPIO_WritePin(AIN2_GPIO_Port, AIN2_Pin, GPIO_PIN_RESET); */
}

static void set_dir_reverse(void)
{
	/* TODO(CubeMX): AIN1=RESET, AIN2=SET */
}

void motor_pwm_set(float duty)
{
	if (duty >= 0.0f)
		set_dir_forward();
	else {
		set_dir_reverse();
		duty = -duty;
	}
	if (duty > 1.0f)
		duty = 1.0f;

	uint32_t compare = (uint32_t)(duty * (float)PWM_MAX_COMPARE);
	(void)compare;
	/* TODO(CubeMX): __HAL_TIM_SET_COMPARE(&htimX, TIM_CHANNEL_y, compare); */
}

void motor_pwm_stop(void)
{
	/* TODO(CubeMX): AIN1=RESET, AIN2=RESET; set compare 0 */
}
