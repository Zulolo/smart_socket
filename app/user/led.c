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
	GPIO_ConfigTypeDef tIO_OutConf;
	tIO_OutConf.GPIO_IntrType = GPIO_PIN_INTR_DISABLE;
	tIO_OutConf.GPIO_Mode = GPIO_Mode_Output;
	tIO_OutConf.GPIO_Pin = LED_1_IO_PIN ;
	tIO_OutConf.GPIO_Pullup = GPIO_PullUp_DIS;
	gpio_config(&tIO_OutConf);

	while(1)
	{
		// Delay and turn on
		vTaskDelay(500/portTICK_RATE_MS);
		GPIO_OUTPUT_SET(LED_1_PIN_NUM, 1);

		// Delay and LED off
		vTaskDelay(500/portTICK_RATE_MS);
		GPIO_OUTPUT_SET(LED_1_PIN_NUM, 0);
	}
}

