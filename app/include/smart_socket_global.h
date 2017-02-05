/*
 * smart_socket_global.h
 *
 *  Created on: Dec 24, 2016
 *      Author: zulolo
 */

#ifndef APP_INCLUDE_SMART_SOCKET_GLOBAL_H_
#define APP_INCLUDE_SMART_SOCKET_GLOBAL_H_

#include "driver/gpio.h"
#include "lwip/apps/sntp_time.h"

#define FLASH_SECTOR_SIZE				SPI_FLASH_SEC_SIZE
#define FLASH_PROTECT_SECTORS			3			// If using flash protecting, writing one sector will actually cost 3 sectors
#define FLASH_USER_DATA_OFFSET			0x200000	// for 4096KB flash (option 6)
#define GET_USER_DATA_ADDR(tType)		(FLASH_USER_DATA_OFFSET + FLASH_SECTOR_SIZE * FLASH_PROTECT_SECTORS * (tType))
#define GET_USER_DATA_SECTORE(tType)	((FLASH_USER_DATA_OFFSET / FLASH_SECTOR_SIZE) + FLASH_PROTECT_SECTORS * (tType))

#define EVENT_HISTORY_MAX_RECORD_NUM	32
#define MAX_SIGNED_INT_16_VALUE			0x8000
#define MAX_TREND_RECORD_SECTOR			256
#define MAX_TREND_RECORD_SIZE			(FLASH_SECTOR_SIZE * MAX_TREND_RECORD_SECTOR)
#define TREND_RECORD_NUM_PER_SECTOR		((FLASH_SECTOR_SIZE)/sizeof(TrendContent_t))
#define MAX_TREND_RECORD_NUM			((TREND_RECORD_NUM_PER_SECTOR)*(MAX_TREND_RECORD_SECTOR))
#define TREND_RECORD_INTERVAL			5000	// ms
#define MAX_TREND_NUM_PER_QUERY			10
#define MAX_TREND_QUERY_LEN				((TREND_RECORD_INTERVAL / 1000) * MAX_TREND_NUM_PER_QUERY)

#define MAX_CURRENT_THRESHOLD			10
#define MIN_CURRENT_THRESHOLD			1

#define MAX_SNTP_SERVER_ADDR_LEN		64
#define MAX_UPDATE_SERVER_HOST_LEN		64
#define MAX_SNTP_SERVER_NUM				3
#define MAX_UPDATE_SERVER_ADDR_LEN		MAX_SNTP_SERVER_ADDR_LEN
#define MAX_TCP_PORT					0xFFFF
#define UPGRADE_TOKEN_LENGTH			32
#define UPGRADE_URL_LENGTH				128

#define RELAY_SCHEDULE_NUM				4
#define RELAY_SCHEDULE_MAX_SEC_DAY		(SECSPERDAY)
#define RELAY_SCHEDULE_TIME_ZONE		(8)
#define RELAY_SCHEDULE_MIN_CLOSE_TIME	5
#define IS_RELAY_SCHEDULE_EMPTY(index)	(tSmartSocketParameter.tRelaySchedule[(index)].unRelayCloseTime == \
											tSmartSocketParameter.tRelaySchedule[(index)].unRelayOpenTime)

typedef enum{
	USER_DATA_EVENT_HISTORY = 0,	// event history
	USER_DATA_CONF_PARA,			// configure parameter
	USER_DATA_TREND					// trend
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
    uint32 unTime;	// UTC seconds
    EventType_t tEventType;
    uint32_t data;
}SmartSocketEvent_t;

typedef struct SmartSocketEventList {
    uint32 unEventNum;
    SmartSocketEvent_t tEvent[EVENT_HISTORY_MAX_RECORD_NUM];
}SmartSocketEventList_t;

typedef struct RelaySchedule{
	uint32 unRelayCloseTime;
	uint32 unRelayOpenTime;
}RelaySchedule_t;

typedef struct SmartSocketParameter{
	struct
	{
		uint32 bSNTPEnable : 1;
		uint32 bButtonRelayEnable : 1;
		uint32 bTrendEnable : 1;
		uint32 bReSmartConfig : 1;
		uint32 bRelayScheduleEnable : 1;
		uint32 bCurrentFailed : 1;
		uint32 bIPGotten : 1;
		uint32 bJustLongPressed : 1;
		uint32 bFWUpgradeStart : 1;
		uint32 bFWUpgradeReset : 1;
		uint32 unused : 22;
	}tConfigure;
	char cSNTP_Server[MAX_SNTP_SERVER_NUM][MAX_SNTP_SERVER_ADDR_LEN];
	char cFW_UpgradeServer[MAX_UPDATE_SERVER_ADDR_LEN];
	char cFW_UpgradeHost[MAX_UPDATE_SERVER_HOST_LEN];
	uint16 unFW_UpgradePort;
	char cFW_UpgradeToken[UPGRADE_TOKEN_LENGTH + 1];
	char cFW_UpgradeUrl[UPGRADE_URL_LENGTH + 1];
	RelaySchedule_t tRelaySchedule[RELAY_SCHEDULE_NUM];
	float fCurrentThreshold;
	uint32 unTrendRecordNum;
	uint32 unValidation;	//TODO: use CRC of tEvent array data
}SmartSocketParameter_t;

typedef struct TrendContent{
	uint32_t unTime;
	float fTemperature;
	float fCurrent;
	float fVoltage;
	float fPower;
}TrendContent_t;

#endif /* APP_INCLUDE_SMART_SOCKET_GLOBAL_H_ */
