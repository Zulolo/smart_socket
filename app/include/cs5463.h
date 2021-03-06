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
	CS5463_CMD_RD_DC_I_OFFSET = (0x01 << 1),
	CS5463_CMD_RD_I_GAIN = (0x02 << 1),
	CS5463_CMD_RD_DC_V_OFFSET = (0x03 << 1),
	CS5463_CMD_RD_V_GAIN = (0x04 << 1),
	CS5463_CMD_RD_CYCLE_COUNT = (0x05 << 1),
	CS5463_CMD_RD_PULSE_RATE = (0x06 << 1),
	CS5463_CMD_RD_I = (0x07 << 1),
	CS5463_CMD_RD_V = (0x08 << 1),
	CS5463_CMD_RD_P = (9 << 1),
	CS5463_CMD_RD_P_ACTIVE = (10 << 1),
	CS5463_CMD_RD_I_RMS = (11 << 1),
	CS5463_CMD_RD_V_RMS = (12 << 1),
	CS5463_CMD_RD_STATUS = (15 << 1),
	CS5463_CMD_RD_AC_I_OFFSET = (16 << 1),
	CS5463_CMD_RD_AC_V_OFFSET = (17 << 1),
	CS5463_CMD_RD_MODE = (18 << 1),
	CS5463_CMD_RD_T = (19 << 1),
	CS5463_CMD_RD_Q_AVG = (20 << 1),
	CS5463_CMD_RD_Q = (21 << 1),
	CS5463_CMD_RD_I_PEAK = (22 << 1),
	CS5463_CMD_RD_V_PEAK = (23 << 1),
	CS5463_CMD_RD_PF = (25 << 1),
	CS5463_CMD_RD_S = (27 << 1),
	CS5463_CMD_WR_CONFIG = 0x40 | CS5463_CMD_RD_CONFIG,
	CS5463_CMD_WR_DC_I_OFFSET = 0x40 | CS5463_CMD_RD_DC_I_OFFSET,
	CS5463_CMD_WR_I_GAIN = 0x40 | CS5463_CMD_RD_I_GAIN,
	CS5463_CMD_WR_DC_V_OFFSET = 0x40 | CS5463_CMD_RD_DC_V_OFFSET,
	CS5463_CMD_WR_V_GAIN = 0x40 | CS5463_CMD_RD_V_GAIN,
	CS5463_CMD_WR_CYCLE_COUNT = 0x40 | CS5463_CMD_RD_CYCLE_COUNT,
	CS5463_CMD_WR_PULSE_RATE = 0x40 | CS5463_CMD_RD_PULSE_RATE,
	CS5463_CMD_WR_STATUS = 0x40 | CS5463_CMD_RD_STATUS,
	CS5463_CMD_WR_AC_I_OFFSET = 0x40 | CS5463_CMD_RD_AC_I_OFFSET,
	CS5463_CMD_WR_AC_V_OFFSET = 0x40 | CS5463_CMD_RD_AC_V_OFFSET,
	CS5463_CMD_WR_MODE = 0x40 | CS5463_CMD_RD_MODE,
	CS5463_CMD_WR_MASK = 0x74,
	CS5463_CMD_WR_CTRL = 0x78,
	CS5463_CMD_SW_RESET = 0x80,
	CS5463_CMD_POWER_UP_HALT = 0xA0,
	CS5463_CMD_START_DC_I_OFFSET_CALIB = 0xC9,
	CS5463_CMD_START_AC_I_OFFSET_CALIB = 0xCD,
	CS5463_CMD_START_AC_I_GAIN_CALIB = 0xCE,
	CS5463_CMD_START_DC_V_OFFSET_CALIB = 0xD1,
	CS5463_CMD_START_AC_V_OFFSET_CALIB = 0xD5,
	CS5463_CMD_START_AC_V_GAIN_CALIB = 0xD6,
	CS5463_CMD_START_SINGLE_CNVS = 0xE0,
	CS5463_CMD_START_CNTN_CNVS = 0xE8,
	CS5463_CMD_SYNC_0 = 0xFE,
	CS5463_CMD_SYNC_1 = 0xFF
}CS5463_CMD_t;

void CS5463_Manager(void *pvParameters);
uint32_t CS5463_unGetStatus(void);
float CS5463_fGetI_RMS(void);
float CS5463_fGetV_RMS(void);
float CS5463_fGetActivePower(void);
float CS5463_fGetTemperature(void);
void CS5463IF_ProtectEnable(bool bEnable);
void CS5463IF_TrendEnable(bool bEnable);

#endif /* APP_INCLUDE_CS5463_H_ */
