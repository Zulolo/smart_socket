/*
 * user_data.h
 *
 *  Created on: Dec 25, 2016
 *      Author: zulolo
 */

#ifndef APP_INCLUDE_USER_DATA_H_
#define APP_INCLUDE_USER_DATA_H_

#include "smart_socket_global.h"

int32_t URDT_nAddEventHistory(uint64 unTime, EventType_t tEventType, void* pData, uint8_t unDataLen);

#endif /* APP_INCLUDE_USER_DATA_H_ */
