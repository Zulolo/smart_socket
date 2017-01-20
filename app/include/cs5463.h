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
	// read cmd: b00xxxxx0
	CS5463_CMD_RD_I = 0x0E,
	CS5463_CMD_RD_V = 0x10,
	CS5463_CMD_RD_P = 0x12,
	CS5463_CMD_RD_P_ACTIVE = 0x14,
	CS5463_CMD_RD_IRMS = 0x16,
	CS5463_CMD_RD_VRMS = 0x18,
	CS5463_CMD_RD_STATUS = 0x1E,
	CS5463_CMD_RD_MODE = 0x24,
	CS5463_CMD_RD_T = 0x26,
	CS5463_CMD_RD_Q_AVG = 0x28,
	CS5463_CMD_RD_Q = 0x2A,
	CS5463_CMD_RD_I_PEAK = 0x2C,
	CS5463_CMD_RD_V_PEAK = 0x2E,
	CS5463_CMD_RD_PF = 0x32,
	CS5463_CMD_RD_S = 0x36,
	CS5463_CMD_START_SINGLE_CNVS = 0xE0,
	CS5463_CMD_START_CNTN_CNVS = 0xE8,
	CS5463_CMD_SYNC_0 = 0xFE,
	CS5463_CMD_SYNC_1 = 0xFF
}CS5463_CMD_t;

void CS5463_Manager(void *pvParameters);
float CS5463_fGetCurrent(void);
float CS5463_fGetVoltage(void);
float CS5463_fGetPower(void);
float CS5463_fGetTemperature(void);

#endif /* APP_INCLUDE_CS5463_H_ */
