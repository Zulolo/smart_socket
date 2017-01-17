/*
 * smart_socket_global.h
 *
 *  Created on: Dec 24, 2016
 *      Author: zulolo
 */

#ifndef APP_INCLUDE_SMART_SOCKET_GLOBAL_H_
#define APP_INCLUDE_SMART_SOCKET_GLOBAL_H_

#include "driver/gpio.h"

#define FLASH_SECTOR_SIZE				0x1000
#define FLASH_PROTECT_SECTORS			3			// If using flash protecting, writing one sector will actually cost 3 sectors
#define FLASH_USER_DATA_OFFSET			0x200000	// for 4096KB flash (option 6)
#define GET_USER_DATA_ADDR(tType)		(FLASH_USER_DATA_OFFSET + FLASH_SECTOR_SIZE * FLASH_PROTECT_SECTORS * (tType))
#define GET_USER_DATA_SECTORE(tType)	((FLASH_USER_DATA_OFFSET / FLASH_SECTOR_SIZE) + FLASH_PROTECT_SECTORS * (tType))

#define EVENT_HISTORY_MAX_RECORD_NUM	32

typedef enum{
	USER_DATA_EVENT_HISTORY = 0,	// event history
	USER_DATA_CONF_PARA				// configure parameter
}UserDataType_t;

typedef enum{
	SMART_SOCKET_EVENT_INVALID = 0,	// Invalide
	SMART_SOCKET_EVENT_OC,			// over current
	SMART_SOCKET_EVENT_OV,			// over voltage
	SMART_SOCKET_EVENT_UV,			// under voltage
	SMART_SOCKET_EVENT_OT,			// over temperature
	SMART_SOCKET_EVENT_REMOTE		// remote operate
}EventType_t;

typedef enum{
	REMOTE_OPERATE_SWITCH_OPEN = 0,
	REMOTE_OPERATE_SWITCH_CLOSE
}RemoteOperate_t;

typedef enum{
    LED_OFF = 0,
    LED_ON  = 1,
    LED_1HZ,
    LED_5HZ,
    LED_20HZ,
}UserLinkLedPattern_t;

typedef struct SmartSocketEvent{
    uint64 unTime;	// UTC seconds
    EventType_t tEventType;
    uint64_t data;
//	union {
//		double dCurrent;		/**< current information */
//		double dVoltage;		/**< voltage information */
//		RemoteOperate_t tRemoteOperate;
//	}data;				/**< event data... */
}SmartSocketEvent_t;

typedef struct SmartSocketEventList {
    uint32 unEventNum;
    uint32 unValidation;	//TODO: use CRC of tEvent array data
    SmartSocketEvent_t tEvent[EVENT_HISTORY_MAX_RECORD_NUM];
}SmartSocketEventList_t;

#endif /* APP_INCLUDE_SMART_SOCKET_GLOBAL_H_ */
