/*
 * led.c
 *
 *  Created on: Dec 23, 2016
 *      Author: zulolo
 */

#include "esp_common.h"
#include "driver/gpio.h"
#include "smart_socket_global.h"

void LED_Manager(void *pvParameters)
{
	GPIO_ConfigTypeDef io_out_conf;
	io_out_conf.GPIO_IntrType = GPIO_PIN_INTR_DISABLE;
	io_out_conf.GPIO_Mode = GPIO_Mode_Output;
	io_out_conf.GPIO_Pin = LED_1_IO_PIN ;
	io_out_conf.GPIO_Pullup = GPIO_PullUp_DIS;
	gpio_config(&io_out_conf);

	while(1)
	{
		// Delay and turn on
		vTaskDelay(300/portTICK_RATE_MS);
		GPIO_OUTPUT_SET(LED_1_PIN_NUM, 1);

		// Delay and LED off
		vTaskDelay(300/portTICK_RATE_MS);
		GPIO_OUTPUT_SET(LED_1_PIN_NUM, 0);
	}
}

