#ifndef __USER_ESPSWITCH_H__
#define __USER_ESPSWITCH_H__

#include "driver/key.h"
#include "smart_socket_global.h"

//struct plug_saved_param {
//    uint8_t status;
//    uint8_t pad[3];
//};

void user_plug_init(void);
uint8 user_plug_get_status(void);
void user_plug_set_status(bool status);
//BOOL user_get_key_status(void);
void user_link_led_output(UserLinkLedPattern_t tPattern);
bool user_plug_relay_schedule_validation(uint8_t unIndex, uint32_t unCloseTime, uint32_t unOpenTime);
bool user_plug_relay_schedule_action(uint32_t unSystemTime);

#endif

