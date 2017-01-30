/******************************************************************************
 * Copyright (C) 2014 -2016  Espressif System
 *
 * FileName: user_plug.c
 *
 * Description: plug demo's function realization
 *
 * Modification history:
 * 2015/7/1, v1.0 create this file.
*******************************************************************************/
#include "esp_common.h"
#include "user_config.h"
#if PLUG_DEVICE
#include "user_plug.h"
#include "user_data.h"
#include "smart_socket_global.h"

/* NOTICE---this is for 4096KB spi flash with 1024+3072.
 * you can change to other sector if you use other size spi flash. */
#define PRIV_PARAM_START_SEC        252	//0x7C

#define PRIV_PARAM_SAVE     0

#define PLUG_USER_KEY_NUM	1
#define RELAY_CLOSE_VALUE	1
#define RELAY_OPEN_VALUE	0

#define NEW_BOARD
#ifdef NEW_BOARD
	#define PLUG_LINK_LED_IO_MUX	PERIPHS_IO_MUX_GPIO2_U
	#define PLUG_LINK_LED_IO_FUNC	FUNC_GPIO2
	#define PLUG_LINK_LED_IO_PIN  	GPIO_Pin_2
	#define PLUG_LINK_LED_PIN_NUM	2

	#define PLUG_USR_KEY_IO_MUX		PERIPHS_IO_MUX_MTDO_U
	#define PLUG_USR_KEY_IO_FUNC	FUNC_GPIO15
	#define PLUG_USR_KEY_IO_PIN  	GPIO_Pin_15
	#define PLUG_USR_KEY_PIN_NUM	15

	#define RELAY_LED_IO_MUX		PERIPHS_IO_MUX_GPIO0_U
	#define RELAY_LED_IO_FUNC		FUNC_GPIO0
	#define RELAY_LED_IO_PIN  		GPIO_Pin_0
	#define RELAY_LED_PIN_NUM		0

	#define RELAY_IO_MUX			PERIPHS_IO_MUX_GPIO4_U
	#define RELAY_IO_FUNC			FUNC_GPIO4
	#define RELAY_IO_PIN  			GPIO_Pin_4
	#define RELAY_PIN_NUM			4

	#define RELAY_LED_ON()			GPIO_OUTPUT_SET(RELAY_LED_PIN_NUM, 0)
	#define RELAY_LED_OFF()			GPIO_OUTPUT_SET(RELAY_LED_PIN_NUM, 1)

	#define RELAY_CLOSE()			GPIO_OUTPUT_SET(RELAY_PIN_NUM, 1)
	#define RELAY_OPEN()			GPIO_OUTPUT_SET(RELAY_PIN_NUM, 0)

	#define RELAY_GET_STATE()		((GPIO_INPUT_GET(RELAY_PIN_NUM) == 1)?(RELAY_CLOSE_VALUE):(RELAY_OPEN_VALUE))
#else
	#define PLUG_LINK_LED_IO_MUX	PERIPHS_IO_MUX_GPIO4_U
	#define PLUG_LINK_LED_IO_FUNC	FUNC_GPIO4
	#define PLUG_LINK_LED_IO_PIN  	GPIO_Pin_4
	#define PLUG_LINK_LED_PIN_NUM	4

	#define PLUG_USR_KEY_IO_MUX		PERIPHS_IO_MUX_MTDO_U
	#define PLUG_USR_KEY_IO_FUNC	FUNC_GPIO15
	#define PLUG_USR_KEY_IO_PIN  	GPIO_Pin_15
	#define PLUG_USR_KEY_PIN_NUM	15

	#define RELAY_LED_IO_MUX		PERIPHS_IO_MUX_GPIO0_U
	#define RELAY_LED_IO_FUNC		FUNC_GPIO0
	#define RELAY_LED_IO_PIN  		GPIO_Pin_0
	#define RELAY_LED_PIN_NUM		0

	#define RELAY_IO_MUX			PERIPHS_IO_MUX_GPIO2_U
	#define RELAY_IO_FUNC			FUNC_GPIO2
	#define RELAY_IO_PIN  			GPIO_Pin_2
	#define RELAY_PIN_NUM			2

	#define RELAY_LED_ON()			GPIO_OUTPUT_SET(RELAY_LED_PIN_NUM, 0)
	#define RELAY_LED_OFF()			GPIO_OUTPUT_SET(RELAY_LED_PIN_NUM, 1)

	#define RELAY_CLOSE()			GPIO_OUTPUT_SET(RELAY_PIN_NUM, 0)
	#define RELAY_OPEN()			GPIO_OUTPUT_SET(RELAY_PIN_NUM, 1)

	#define RELAY_GET_STATE()		((GPIO_INPUT_GET(RELAY_PIN_NUM) == 1)?(0):(1))
#endif

#define PLUG_LINK_LED_ON		GPIO_OUTPUT_SET(PLUG_LINK_LED_PIN_NUM, 0)
#define PLUG_LINK_LED_OFF		GPIO_OUTPUT_SET(PLUG_LINK_LED_PIN_NUM, 1)

//LOCAL struct plug_saved_param plug_param;
LOCAL struct keys_param keys;
LOCAL struct single_key_param *user_key[PLUG_USER_KEY_NUM];
LOCAL os_timer_t link_led_timer;
LOCAL uint8 link_led_level = 0;
//LOCAL uint8 device_status;
SmartSocketParameter_t tSmartSocketParameter;
//extern bool smartconfig_start(sc_callback_t cb, ...);
//extern bool smartconfig_stop(void);
//extern void ICACHE_FLASH_ATTR smartconfig_done(sc_status status, void *pdata);
//extern void startSmartConfig(void);

/******************************************************************************
 * FunctionName : user_plug_relay_schedule_validation
 * Description  : check if the input close time and open time is valid (no overlap with other schedule recipe)
 * Parameters   : uint8_t unIndex
 * 				  uint32_t unCloseTime
 * 				  uint32_t unOpenTime
 * Returns      : bool - valid/invalid
*******************************************************************************/
bool user_plug_relay_schedule_validation(uint8_t unIndex, uint32_t unCloseTime, uint32_t unOpenTime)
{
	uint8_t unRecipeIndex;
	if (unCloseTime >= unOpenTime){
		return false;
	}
	if ((unCloseTime >= RELAY_SCHEDULE_MAX_SEC_DAY) || (unOpenTime >= RELAY_SCHEDULE_MAX_SEC_DAY)){
		return false;
	}

	for (unRecipeIndex = 0; unRecipeIndex < RELAY_SCHEDULE_NUM; unRecipeIndex++){
		if (unIndex != unRecipeIndex){
			if (!(IS_RELAY_SCHEDULE_EMPTY(unRecipeIndex))){
				if (((unCloseTime >= tSmartSocketParameter.tRelaySchedule[unRecipeIndex].unRelayCloseTime) &&
						(unCloseTime <= tSmartSocketParameter.tRelaySchedule[unRecipeIndex].unRelayOpenTime)) ||
						((unOpenTime >= tSmartSocketParameter.tRelaySchedule[unRecipeIndex].unRelayCloseTime) &&
								(unOpenTime <= tSmartSocketParameter.tRelaySchedule[unRecipeIndex].unRelayOpenTime)) ){
					return false;
				}
			}
		}
	}
	return true;
}

/******************************************************************************
 * FunctionName : user_plug_relay_schedule_action
 * Description  : check if relay schedule recipe need to be executed
 * Parameters   : uint32_t unSystemTime
 * Returns      : bool - execute fail/success
*******************************************************************************/
bool user_plug_relay_schedule_action(uint32_t unSystemTime)
{
	uint32_t unSecondInDay;
	uint8_t unRecipeIndex;

	if (unSystemTime < 1485273600){	// 2017/01/25 0:0:0
		return false;
	}
	// change to UTC time
	unSystemTime -= (RELAY_SCHEDULE_TIME_ZONE * SECSPERHOUR);
	unSecondInDay = unSystemTime % SECSPERDAY;
//	printf("unSecondInDay is %u.\n", unSecondInDay);

	for (unRecipeIndex = 0; unRecipeIndex < RELAY_SCHEDULE_NUM; unRecipeIndex++){
		if (!(IS_RELAY_SCHEDULE_EMPTY(unRecipeIndex))){
			if ((unSecondInDay >= tSmartSocketParameter.tRelaySchedule[unRecipeIndex].unRelayCloseTime) &&
					(unSecondInDay <= tSmartSocketParameter.tRelaySchedule[unRecipeIndex].unRelayOpenTime)){
				if (user_plug_get_status() != RELAY_CLOSE_VALUE){
					user_plug_set_status(RELAY_CLOSE_VALUE);
				}
				return true;
			}
		}
	}

	// open
	if (user_plug_get_status() != RELAY_OPEN_VALUE){
		user_plug_set_status(RELAY_OPEN_VALUE);
	}
	return true;
}

/******************************************************************************
 * FunctionName : user_plug_get_status
 * Description  : get plug's status, 0x00 or 0x01
 * Parameters   : none
 * Returns      : uint8 - plug's status
*******************************************************************************/
uint8  
user_plug_get_status(void)
{
    return RELAY_GET_STATE();
}

/******************************************************************************
 * FunctionName : user_plug_set_status
 * Description  : set plug's status, 0x00 or 0x01
 * Parameters   : uint8 - status
 * Returns      : none
*******************************************************************************/
void  
user_plug_set_status(bool status)
{
	if (status > 1) {
		printf("error status input!\n");
		return;
	}
//	printf("status input! %d\n", status);
	if (RELAY_CLOSE_VALUE == status){
		RELAY_CLOSE();
		RELAY_LED_ON();
		DAT_nAddEventHistory(system_get_time(), SMART_SOCKET_EVENT_REMOTE, 1);
	}else if(RELAY_OPEN_VALUE == status){
		RELAY_OPEN();
		RELAY_LED_OFF();
		DAT_nAddEventHistory(system_get_time(), SMART_SOCKET_EVENT_REMOTE, 0);
	}

}

/******************************************************************************
 * FunctionName : user_plug_toggle_status
 * Description  : toggle relay close/open status
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
void
user_plug_toggle_status(void)
{
	user_plug_set_status((RELAY_CLOSE_VALUE == (RELAY_GET_STATE()))?(RELAY_OPEN_VALUE):(RELAY_CLOSE_VALUE));
}

/******************************************************************************
 * FunctionName : user_plug_short_press
 * Description  : key's short press function, needed to be installed
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
LOCAL void
user_plug_short_press(void)
{
	printf("Short press!\n");
	if ((1 == tSmartSocketParameter.tConfigure.bButtonRelayEnable) &&
			(0 == tSmartSocketParameter.tConfigure.bRelayScheduleEnable) &&
			(0 == tSmartSocketParameter.tConfigure.bCurrentFailed)){
		user_plug_toggle_status();
	}
}

/******************************************************************************
 * FunctionName : user_plug_long_press
 * Description  : key's long press function, needed to be installed
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
LOCAL void
user_plug_long_press(void)
{
	printf("Long press!\n");
	tSmartSocketParameter.tConfigure.bReSmartConfig = 1;
}

/******************************************************************************
 * FunctionName : user_link_led_init
 * Description  : int led gpio
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
LOCAL void  
user_link_led_init(void)
{
	GPIO_ConfigTypeDef io_out_conf;

	printf("Configure user link LED pin.\n");
	PIN_FUNC_SELECT(PLUG_LINK_LED_IO_MUX, PLUG_LINK_LED_IO_FUNC);
	io_out_conf.GPIO_IntrType = GPIO_PIN_INTR_DISABLE;
	io_out_conf.GPIO_Mode = GPIO_Mode_Output;
	io_out_conf.GPIO_Pin = PLUG_LINK_LED_IO_PIN ;
	io_out_conf.GPIO_Pullup = GPIO_PullUp_DIS;
	gpio_config(&io_out_conf);

	PLUG_LINK_LED_OFF;
}

LOCAL void  
user_link_led_timer_cb(void)
{
    link_led_level = (~link_led_level) & 0x01;
    GPIO_OUTPUT_SET(GPIO_ID_PIN(PLUG_LINK_LED_PIN_NUM), link_led_level);
}

void  
user_link_led_timer_init(int time)
{
    os_timer_disarm(&link_led_timer);
    os_timer_setfn(&link_led_timer, (os_timer_func_t *)user_link_led_timer_cb, NULL);
    os_timer_arm(&link_led_timer, time, 1);
    link_led_level = 0;
    GPIO_OUTPUT_SET(GPIO_ID_PIN(PLUG_LINK_LED_PIN_NUM), link_led_level);
}
/*
void
user_link_led_timer_done(void)
{
    os_timer_disarm(&link_led_timer);

    GPIO_OUTPUT_SET(GPIO_ID_PIN(PLUG_LINK_LED_IO_NUM), 1);
}
*/
/******************************************************************************
 * FunctionName : user_link_led_output
 * Description  : led flash mode
 * Parameters   : mode, on/off/xhz
 * Returns      : none
*******************************************************************************/
void
user_link_led_output(UserLinkLedPattern_t tPattern)
{

    switch (tPattern) {
        case LED_OFF:
            os_timer_disarm(&link_led_timer);
            PLUG_LINK_LED_OFF;
            break;

        case LED_ON:
            os_timer_disarm(&link_led_timer);
            PLUG_LINK_LED_ON;
            break;

        case LED_1HZ:
            user_link_led_timer_init(1000);
            break;

        case LED_5HZ:
            user_link_led_timer_init(200);
            break;

        case LED_20HZ:
            user_link_led_timer_init(50);
            break;

        default:
            printf("ERROR:LED MODE WRONG!\n");
            break;
    }

}

/******************************************************************************
 * FunctionName : user_get_key_status
 * Description  : a
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
BOOL  
user_get_key_status(void)
{
    return get_key_status(user_key[0]);
}

void relayInit(void)
{
	GPIO_ConfigTypeDef io_out_conf;

	printf("Configure relay pin.\n");
	PIN_FUNC_SELECT(RELAY_IO_MUX, RELAY_IO_FUNC);
	io_out_conf.GPIO_IntrType = GPIO_PIN_INTR_DISABLE;
	io_out_conf.GPIO_Mode = GPIO_Mode_Output;
	io_out_conf.GPIO_Pin = RELAY_IO_PIN ;
	io_out_conf.GPIO_Pullup = GPIO_PullUp_DIS;
	gpio_config(&io_out_conf);
	RELAY_OPEN();

	PIN_FUNC_SELECT(RELAY_LED_IO_MUX, RELAY_LED_IO_FUNC);
	io_out_conf.GPIO_IntrType = GPIO_PIN_INTR_DISABLE;
	io_out_conf.GPIO_Mode = GPIO_Mode_Output;
	io_out_conf.GPIO_Pin = RELAY_LED_IO_PIN ;
	io_out_conf.GPIO_Pullup = GPIO_PullUp_DIS;
	gpio_config(&io_out_conf);
	RELAY_LED_OFF();
}

/******************************************************************************
 * FunctionName : user_plug_init
 * Description  : init plug's key function and relay output
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
void  
user_plug_init(void)
{
    printf("user_plug_init start!\n");

    user_link_led_init();

//    wifi_status_led_install(PLUG_WIFI_LED_PIN_NUM, PLUG_WIFI_LED_IO_MUX, PLUG_WIFI_LED_IO_FUNC);

    user_key[0] = key_init_single(PLUG_USR_KEY_PIN_NUM, PLUG_USR_KEY_IO_MUX, PLUG_USR_KEY_IO_FUNC,
                                    user_plug_long_press, user_plug_short_press);

    keys.key_num = PLUG_USER_KEY_NUM;
    keys.key_list = user_key;

    key_init(&keys);

    relayInit();
}
#endif

