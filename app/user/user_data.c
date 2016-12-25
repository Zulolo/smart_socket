/*
 * user_data.c
 *
 *  Created on: Dec 25, 2016
 *      Author: zulolo
 */

#include "esp_common.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "smart_socket_global.h"

xSemaphoreHandle xSmartSocketEventListSemaphore;
SmartSocketEventList_t tSmartSocketEventList;

int32_t URDT_nAddEventHistory(uint64 unTime, EventType_t tEventType, void* pData, uint8_t unDataLen)
{
    if(xSemaphoreTake(xSmartSocketEventListSemaphore, (portTickType)10) == pdTRUE ){
    	tSmartSocketEventList.tEvent[tSmartSocketEventList.unEventIndex].unTime = unTime;
    	tSmartSocketEventList.tEvent[tSmartSocketEventList.unEventIndex].tEventType = tEventType;
    	memcpy(&(tSmartSocketEventList.tEvent[tSmartSocketEventList.unEventIndex].data), pData, unDataLen);
    	tSmartSocketEventList.unEventIndex++;

		system_param_save_with_protect(GET_USER_DATA_SECTORE(USER_DATA_EVENT_HISTORY),
				&tSmartSocketEventList, sizeof(tSmartSocketEventList));

        xSemaphoreGive(xSmartSocketEventListSemaphore);
    }else{
		printf("Take event list semaphore failed.\n");
		return (-1);
    }
	return 0;
}
