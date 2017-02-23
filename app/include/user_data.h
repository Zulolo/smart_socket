/*
 * user_data.h
 *
 *  Created on: Dec 25, 2016
 *      Author: zulolo
 */

#ifndef APP_INCLUDE_USER_DATA_H_
#define APP_INCLUDE_USER_DATA_H_

#include "smart_socket_global.h"

int32_t DAT_nAddEventHistory(uint64 unTime, EventType_t tEventType, uint64_t unData);	//void* pData, uint8_t unDataLen);
SmartSocketEvent_t DAT_nGetEventHistory(uint8_t unEventSelecter);	// unEventIndex =0 means latest, =1 means second last
bool DAT_bTrendRecordAdd(TrendContent_t tValueNeedToAdd);
int32_t DAT_nFlashDataClean(void);
TrendContent_t* DAT_pGetTrends(uint32_t unStartTime, uint32_t unEndTime, uint8_t* pTrendRecordNum);
float DAT_fGetIMax(void);
float DAT_fGetVMax(void);
float DAT_fGetCurrentThrhld(void);

#endif /* APP_INCLUDE_USER_DATA_H_ */
