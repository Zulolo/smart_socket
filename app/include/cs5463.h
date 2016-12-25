/*
 * current.h
 *
 *  Created on: Dec 21, 2016
 *      Author: Zulolo
 */

#ifndef APP_INCLUDE_CS5463_H_
#define APP_INCLUDE_CS5463_H_

#include "driver/gpio.h"

//#define CS5463_MOSI_IO_PIN		GPIO_Pin_13
//#define CS5463_MOSI_PIN_NUM		13

typedef enum{
	CS5463_CMD_RD_CONFIG = 0x00,
	CS5463_CMD_START_SINGLE_CNVS = 0xE0,
	CS5463_CMD_START_CNTN_CNVS = 0xE8,
	CS5463_CMD_SYNC_0 = 0xFE,
	CS5463_CMD_SYNC_1 = 0xFF
}CS5463_CMD_t;

void CS5463_Manager(void *pvParameters);
double CS5463_dGetCurrent(void);

#endif /* APP_INCLUDE_CS5463_H_ */
