/*
 * led.c
 *
 *  Created on: Dec 23, 2016
 *      Author: zulolo
 */

#include "esp_common.h"
#include "gpio.h"

void led_blink(void *pvParameters)
{
	while(1)
	{
		// Delay and turn on
		vTaskDelay (300/portTICK_RATE_MS);
		GPIO_OUTPUT_SET (12, 1);

		// Delay and LED off
		vTaskDelay (300/portTICK_RATE_MS);
		GPIO_OUTPUT_SET (12, 0);
	}
}

