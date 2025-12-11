/*
 * control.c
 *
 *  Created on: Nov 14, 2025
 *      Author: ivanp
 */

#include "pwm.h"
#include "uart.h"


static TIM_HandleTypeDef *s_htim_pwm = NULL;
static uint32_t pwm_ch = 0;
static UART_HandleTypeDef *s_huart = NULL;

static volatile uint32_t pwm_duty = 0;

void Control_Init(TIM_HandleTypeDef *htim_pwm, uint32_t pwm_channel, UART_HandleTypeDef *huart){
    s_htim_pwm = htim_pwm;
    pwm_ch = pwm_channel;
    s_huart = huart;

    pwm_set_duty_percent(s_htim_pwm, pwm_ch, pwm_duty);
}


void process_line(char *line)
{
    if (strncmp(line, "pwm", 3) == 0) {
        line += 3;
        while (*line == ' ' || *line == '\t') line++;
        int val = (*line) ? atoi(line) : -1;
        if (val < 0)   val = 0;
        if (val > 100) val = 100;

        pwm_duty = (uint32_t) val;
        pwm_set_duty_percent(s_htim_pwm, pwm_ch, pwm_duty);

        char ack[32];
        int n = snprintf(ack, sizeof(ack), "OK pwm=%d%%\r\n", val);
        HAL_UART_Transmit(s_huart, (uint8_t*)ack, n, HAL_MAX_DELAY);

        return;
    }

    const char *err = "ERR: use 'pwm <0..100>'\r\n";
    HAL_UART_Transmit(s_huart, (uint8_t*)err, strlen(err), HAL_MAX_DELAY);
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == GPIO_PIN_13)
    {
        // --- debounce softverski ---
        static uint32_t last_press_ms = 0;
        uint32_t now = HAL_GetTick();

        // Ako je tipka stisnuta prebrzo zaredom (<200ms), ignoriraj
        if ((now - last_press_ms) < 200) {
            return;
        }
        last_press_ms = now;

        // --- promijeni duty ciklus ---
        pwm_duty += 10;
        if (pwm_duty > 100) {
            pwm_duty = 0;
        }

        // --- ažuriraj PWM output odmah ---
        pwm_set_duty_percent(s_htim_pwm, pwm_ch, pwm_duty);

        // --- (opcionalno) pošalji status na UART ---
        char buf[40];
        int n = snprintf(buf, sizeof(buf), "Duty changed by button = %lu%%\r\n", pwm_duty);
        HAL_UART_Transmit(s_huart, (uint8_t*)buf, n, HAL_MAX_DELAY);
    }
}

