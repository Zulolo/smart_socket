/*
 * current.c
 *
 *  Created on: Dec 21, 2016
 *      Author: Zulolo
 */
#include "esp_common.h"
#include "driver/gpio.h"
#include "driver/spi_register.h"
#include "driver/spi_interface.h"

void CS5463_Manager(void *pvParameters)
{
	while(1){
		vTaskDelay(1000/portTICK_RATE_MS);
	}
}

double CS5463_dGetCurrent(void)
{
	printf("user_cs5463_get_current_d\n");
	return 2.34;
}
