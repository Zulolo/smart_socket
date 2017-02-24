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

bool DAT_unGetCalib(void);
uint32 DAT_unGetAC_V_Offset(void);
uint32 DAT_unGetDC_V_Offset(void);
uint32 DAT_unGetAC_I_Offset(void);
uint32 DAT_unGetDC_I_Offset(void);
uint32 DAT_unGetV_Gain(void);
uint32 DAT_unGetI_Gain(void);
void DAT_unSetCalib(bool bCalib);
void DAT_unSetAC_V_Offset(uint32 unAC_V_Offset);
void DAT_unSetDC_V_Offset(uint32 unDC_V_Offset);
void DAT_unSetAC_I_Offset(uint32 unAC_I_Offset);
void DAT_unSetDC_I_Offset(uint32 unDC_I_Offset);
void DAT_unSetV_Gain(uint32 unV_Gain);
void DAT_unSetI_Gain(uint32 unI_Gain);

#endif /* APP_INCLUDE_USER_DATA_H_ */
