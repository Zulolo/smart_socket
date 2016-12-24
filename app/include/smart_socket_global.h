/*
 * smart_socket_global.h
 *
 *  Created on: Dec 24, 2016
 *      Author: zulolo
 */

#ifndef APP_INCLUDE_SMART_SOCKET_GLOBAL_H_
#define APP_INCLUDE_SMART_SOCKET_GLOBAL_H_

#include "driver/gpio.h"

#define LED_1_IO_PIN  		GPIO_Pin_4
#define LED_1_PIN_NUM  		4
#define RELAY_IO_PIN  		GPIO_Pin_2
#define RELAY_PIN_NUM		2
#define RELAY_CLOSE()		GPIO_OUTPUT_SET(RELAY_PIN_NUM, 0)
#define RELAY_OPEN()		GPIO_OUTPUT_SET(RELAY_PIN_NUM, 1)
#define RELAY_GET_STATE()	((GPIO_INPUT_GET(RELAY_PIN_NUM) == 1)?(0):(1))
#define RELAY_CLOSE_VALUE	1
#define RELAY_OPEN_VALUE	0

#endif /* APP_INCLUDE_SMART_SOCKET_GLOBAL_H_ */
