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
xSemaphoreHandle xSmartSocketParameterSemaphore;
xSemaphoreHandle xSmartSocketCaliSemaphore;
SmartSocketEventList_t tSmartSocketEventList;
SmartSocketParameter_t tSmartSocketParameter;

float DAT_fGetCurrentThrhld(void)
{
	return tSmartSocketParameter.fCurrentThreshold;
}

float DAT_fGetIMax(void)
{
	return tSmartSocketParameter.tCS5463Calib.fI_Max;
}

float DAT_fGetVMax(void)
{
//	printf("SmartSocketParameter_t %d\n",sizeof(SmartSocketParameter_t));
//	printf("tSmartSocketParameter.tCS5463Calib.fV_Max %d\n", (int32_t)(tSmartSocketParameter.tCS5463Calib.fV_Max));
	return tSmartSocketParameter.tCS5463Calib.fV_Max;
}

int32_t DAT_nFlashDataClean(void)
{
	if(xSemaphoreTake(xSmartSocketEventListSemaphore, (portTickType)(10000/portTICK_RATE_MS)) == pdTRUE ){
		memset(&tSmartSocketEventList, 0, sizeof(tSmartSocketEventList));
		system_param_save_with_protect(GET_USER_DATA_SECTORE(USER_DATA_EVENT_HISTORY),
						&tSmartSocketEventList, sizeof(tSmartSocketEventList));
		xSemaphoreGive(xSmartSocketEventListSemaphore);
	}else{
		printf("Take event list semaphore failed.\n");
		return (-1);
    }

	if(xSemaphoreTake(xSmartSocketParameterSemaphore, (portTickType)(10000/portTICK_RATE_MS)) == pdTRUE){
		memset(&tSmartSocketParameter, 0, sizeof(tSmartSocketParameter));
		tSmartSocketParameter.unValidation = 0xA5A5A5A5;
		tSmartSocketParameter.tCS5463Calib.fI_Max = 9.8;
		tSmartSocketParameter.tCS5463Calib.fV_Max = 225;
		tSmartSocketParameter.tConfigure.bButtonRelayEnable = 1;
		tSmartSocketParameter.tConfigure.bSNTPEnable = 1;
//		tSmartSocketParameter.tConfigure.bRelayScheduleEnable = 0;
		tSmartSocketParameter.unFW_UpgradePort = 80;
		tSmartSocketParameter.fCurrentThreshold = 9.8;
		memcpy(tSmartSocketParameter.sSNTP_Server[0], "cn.pool.ntp.org", sizeof("cn.pool.ntp.org"));
		memcpy(tSmartSocketParameter.sSNTP_Server[1], "asia.pool.ntp.org", sizeof("asia.pool.ntp.org"));
		memcpy(tSmartSocketParameter.sSNTP_Server[2], "pool.ntp.org", sizeof("pool.ntp.org"));
		system_param_save_with_protect(GET_USER_DATA_SECTORE(USER_DATA_CONF_PARA),
							&tSmartSocketParameter, sizeof(tSmartSocketParameter));
		xSemaphoreGive(xSmartSocketParameterSemaphore);
	}else{
		printf("Take parameter semaphore failed.\n");
		return (-1);
	}

	return 0;
}

bool DAT_bTrendRecordAdd(TrendContent_t tValueNeedToAdd)
{
	uint32_t unRotatedIndex;
	uint32_t unRotatedSector;
	uint32_t unIndexInSector;
	uint32_t unSectorIndex;
	unRotatedIndex = tSmartSocketParameter.unTrendRecordNum % MAX_TREND_RECORD_NUM;
	unRotatedSector = unRotatedIndex / TREND_RECORD_NUM_PER_SECTOR;
	unIndexInSector = unRotatedIndex % TREND_RECORD_NUM_PER_SECTOR;
	unSectorIndex = GET_USER_DATA_SECTORE(USER_DATA_TREND) + unRotatedSector;
	// check if need to erase this sector
	if (0 == unIndexInSector){
		spi_flash_erase_sector(unSectorIndex);
	}

	// increase record number first in case data was actually written but during increasing exception happens
	// !!!! some bytes can be empty at end of each sector !!!!
	if(xSemaphoreTake(xSmartSocketParameterSemaphore, (portTickType)(10000/portTICK_RATE_MS)) == pdTRUE ){
		tSmartSocketParameter.unTrendRecordNum++;
		system_param_save_with_protect(GET_USER_DATA_SECTORE(USER_DATA_CONF_PARA),
						&tSmartSocketParameter, sizeof(tSmartSocketParameter));
		xSemaphoreGive(xSmartSocketParameterSemaphore);
		spi_flash_write(GET_USER_DATA_ADDR(USER_DATA_TREND) + unSectorIndex * FLASH_SECTOR_SIZE + unIndexInSector * sizeof(TrendContent_t),
				(uint32_t *)(&tValueNeedToAdd), sizeof(tValueNeedToAdd));
	}else{
		printf("Take parameter semaphore failed.\n");
		return false;
	}

	return true;
}

// return value -1 means error
// else is the record number of trend
int32_t DAT_nGetTrendLength(uint32_t* pEarliestRecordTime, uint32_t* pLastRecordTime)
{
	uint32_t unRotatedIndex;
	uint32_t unRotatedSector;
	TrendContent_t tValueNeedToAdd;

	if (0 == tSmartSocketParameter.unTrendRecordNum){
		return (-1);
	}else{
		// end time
		unRotatedIndex = (tSmartSocketParameter.unTrendRecordNum - 1) % MAX_TREND_RECORD_NUM;
		spi_flash_read(GET_USER_DATA_ADDR(USER_DATA_TREND) + unRotatedIndex * sizeof(TrendContent_t),
						(uint32_t *)(&tValueNeedToAdd), sizeof(tValueNeedToAdd));
		*pLastRecordTime = tValueNeedToAdd.unTime;

		// start time
		if (tSmartSocketParameter.unTrendRecordNum > MAX_TREND_RECORD_NUM){
			// Flash already full
			// So earliest is next (can be first) sector's first record
			// !!!! some bytes can be empty at end of each sector !!!!
			unRotatedSector = unRotatedIndex / TREND_RECORD_NUM_PER_SECTOR;
			spi_flash_read(GET_USER_DATA_ADDR(USER_DATA_TREND) +
					((unRotatedSector == (MAX_TREND_RECORD_SECTOR - 1))?(0):(unRotatedSector + 1)) * FLASH_SECTOR_SIZE,
									(uint32_t *)(&tValueNeedToAdd), sizeof(tValueNeedToAdd));
			*pEarliestRecordTime = tValueNeedToAdd.unTime;
			return (MAX_TREND_RECORD_NUM - TREND_RECORD_NUM_PER_SECTOR + tSmartSocketParameter.unTrendRecordNum % MAX_TREND_RECORD_NUM);
		}else{
			// Flash not full yet
			spi_flash_read(GET_USER_DATA_ADDR(USER_DATA_TREND), (uint32_t *)(&tValueNeedToAdd), sizeof(tValueNeedToAdd));
			*pEarliestRecordTime = tValueNeedToAdd.unTime;
			return 0;
		}
	}

}

int32_t getSpecifiedTrendRecordByIndex(uint32_t unTrendRecordIndex, TrendContent_t* pTrendRecord)
{
	uint32_t unRotatedIndex;
	uint32_t unRotatedSector;
	uint32_t unIndexInSector;
	uint32_t unSectorIndex;

	if (unTrendRecordIndex >= tSmartSocketParameter.unTrendRecordNum){
		return (-1);
	}

	if ((tSmartSocketParameter.unTrendRecordNum > MAX_TREND_RECORD_NUM) &&
			(unTrendRecordIndex < (tSmartSocketParameter.unTrendRecordNum - MAX_TREND_RECORD_NUM))){
		return (-1);
	}

	unRotatedIndex = unTrendRecordIndex % MAX_TREND_RECORD_NUM;
	unRotatedSector = unRotatedIndex / TREND_RECORD_NUM_PER_SECTOR;
	unIndexInSector = unRotatedIndex % TREND_RECORD_NUM_PER_SECTOR;
	unSectorIndex = GET_USER_DATA_SECTORE(USER_DATA_TREND) + unRotatedSector;
	spi_flash_read(GET_USER_DATA_ADDR(USER_DATA_TREND) + unSectorIndex * FLASH_SECTOR_SIZE + unIndexInSector * sizeof(TrendContent_t),
					(uint32_t *)(pTrendRecord), sizeof(TrendContent_t));
	return 0;
}

TrendContent_t* DAT_pGetTrends(uint32_t unStartTime, uint32_t unEndTime, uint8_t* pTrendRecordNum)
{
	uint32_t unRecordEarliestIndex;
	uint32_t unStartIndex, unEndIndex;
	int32_t nTotalRecordNum;
	uint32_t unRecordNumBetwenEndAndLast;
	uint8_t unSelectRecordNum;
	uint32_t unEarliestRecordTime, unLastRecordTime;
	TrendContent_t* pTrendValues;
	uint8_t unTrendIndex;

    if ((unEndTime <= unStartTime) || (0 == tSmartSocketParameter.unTrendRecordNum)){
    	*pTrendRecordNum = 0;
		return NULL;
    }else{
    	nTotalRecordNum = DAT_nGetTrendLength(&unEarliestRecordTime, &unLastRecordTime);
    	if (nTotalRecordNum < 0){
    		*pTrendRecordNum = 0;
    		return NULL;
    	}
    	unRecordEarliestIndex = tSmartSocketParameter.unTrendRecordNum - nTotalRecordNum;

    	// This check may be canceled
    	if ((unStartTime > unLastRecordTime) || (unEndTime < unEarliestRecordTime)){
        	*pTrendRecordNum = 0;
    		return NULL;
    	}

    	unEndTime = (unEndTime > unLastRecordTime)?(unLastRecordTime):(unEndTime);
    	unStartTime = (unStartTime < unEarliestRecordTime)?(unEarliestRecordTime):(unStartTime);

    	if (unLastRecordTime == unEndTime){
    		unEndIndex = tSmartSocketParameter.unTrendRecordNum - 1;
    	}else{
    		unRecordNumBetwenEndAndLast = (unLastRecordTime - unEndTime) / (TREND_RECORD_INTERVAL / 1000);
    		if (unRecordNumBetwenEndAndLast >= tSmartSocketParameter.unTrendRecordNum){
    			*pTrendRecordNum = 0;
    			return NULL;
    		}
    		unEndIndex = tSmartSocketParameter.unTrendRecordNum - 1 - unRecordNumBetwenEndAndLast;
    	}
    	unSelectRecordNum = (unEndTime - unStartTime) / (TREND_RECORD_INTERVAL / 1000) + 1;
    	unSelectRecordNum = (unSelectRecordNum > MAX_TREND_NUM_PER_QUERY)?(MAX_TREND_NUM_PER_QUERY):(unSelectRecordNum);
    	unStartIndex = unEndIndex + 1 - unSelectRecordNum;
    	unStartIndex = (unStartIndex < unRecordEarliestIndex)?(unRecordEarliestIndex):(unStartIndex);

    	// finally start and end index have both been gotten
    	*pTrendRecordNum = unEndIndex - unStartIndex + 1;
    	pTrendValues = malloc(*pTrendRecordNum * sizeof(TrendContent_t));
    	for(unTrendIndex = 0; unTrendIndex < *pTrendRecordNum; unTrendIndex++){
    		getSpecifiedTrendRecordByIndex(unStartIndex + unTrendIndex, pTrendValues + unTrendIndex);
    	}
    	return pTrendValues;
    }
}

int32_t DAT_nAddEventHistory(uint64 unTime, EventType_t tEventType, uint32_t unData)	//void* pData, uint8_t unDataLen)
{
    if(xSemaphoreTake(xSmartSocketEventListSemaphore, (portTickType)(10000/portTICK_RATE_MS)) == pdTRUE ){
    	tSmartSocketEventList.tEvent[tSmartSocketEventList.unEventNum % EVENT_HISTORY_MAX_RECORD_NUM].unTime = sntp_get_current_timestamp();
    	tSmartSocketEventList.tEvent[tSmartSocketEventList.unEventNum % EVENT_HISTORY_MAX_RECORD_NUM].tEventType = tEventType;
    	tSmartSocketEventList.tEvent[tSmartSocketEventList.unEventNum % EVENT_HISTORY_MAX_RECORD_NUM].data = unData;
    	//memcpy(&(tSmartSocketEventList.tEvent[tSmartSocketEventList.unEventNum % EVENT_HISTORY_MAX_RECORD_NUM].data), pData, unDataLen);
    	tSmartSocketEventList.unEventNum++;

		system_param_save_with_protect(GET_USER_DATA_SECTORE(USER_DATA_EVENT_HISTORY),
				&tSmartSocketEventList, sizeof(tSmartSocketEventList));

        xSemaphoreGive(xSmartSocketEventListSemaphore);
    }else{
		printf("Take event list semaphore failed.\n");
		return (-1);
    }
	return 0;
}

SmartSocketEvent_t DAT_nGetEventHistory(uint8_t unEventSelecter)	// unEventSelecter =0 means latest, =1 means second last
{
	SmartSocketEvent_t tEvent;
	uint8_t unEventIndex;

	tEvent.tEventType = SMART_SOCKET_EVENT_INVALID;

	if ((unEventIndex >= EVENT_HISTORY_MAX_RECORD_NUM) ||
			(tSmartSocketEventList.unEventNum < (unEventIndex + 1))){
		return tEvent;
	}
	unEventIndex = (tSmartSocketEventList.unEventNum - 1) % EVENT_HISTORY_MAX_RECORD_NUM;
    if(xSemaphoreTake(xSmartSocketEventListSemaphore, (portTickType)(10000/portTICK_RATE_MS)) == pdTRUE ){
    	tEvent = tSmartSocketEventList.tEvent[(unEventIndex >= unEventSelecter)?
    			(unEventIndex - unEventSelecter) : (EVENT_HISTORY_MAX_RECORD_NUM - (unEventSelecter - unEventIndex))];
        xSemaphoreGive(xSmartSocketEventListSemaphore);
        return tEvent;
    }else{
		printf("Take event list semaphore failed.\n");
		return tEvent;
    }
	return tEvent;
}
