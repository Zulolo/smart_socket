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
#include "smart_socket_global.h"

//LOCAL struct plug_saved_param plug_param;
//LOCAL struct keys_param keys;
//LOCAL struct single_key_param *single_key[PLUG_KEY_NUM];
LOCAL os_timer_t link_led_timer;
LOCAL uint8 link_led_level = 0;

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
	printf("status input! %d\n", status);

	if (RELAY_CLOSE_VALUE == status){
		RELAY_CLOSE();
		RELAY_LED_ON();
		URDT_nAddEventHistory(system_get_time(), SMART_SOCKET_EVENT_REMOTE, 1);
	}else if(RELAY_OPEN_VALUE == status){
		RELAY_OPEN();
		RELAY_LED_OFF();
		URDT_nAddEventHistory(system_get_time(), SMART_SOCKET_EVENT_REMOTE, 0);
	}
}

///******************************************************************************
// * FunctionName : user_plug_short_press
// * Description  : key's short press function, needed to be installed
// * Parameters   : none
// * Returns      : none
//*******************************************************************************/
//LOCAL void
//user_plug_short_press(void)
//{
//    user_plug_set_status((~plug_param.status) & 0x01);
//    spi_flash_erase_sector(PRIV_PARAM_START_SEC + PRIV_PARAM_SAVE);
//    spi_flash_write((PRIV_PARAM_START_SEC + PRIV_PARAM_SAVE) * SPI_FLASH_SEC_SIZE,
//                (uint32 *)&plug_param, sizeof(struct plug_saved_param));
//}
//
///******************************************************************************
// * FunctionName : user_plug_long_press
// * Description  : key's long press function, needed to be installed
// * Parameters   : none
// * Returns      : none
//*******************************************************************************/
//LOCAL void
//user_plug_long_press(void)
//{
//    int boot_flag=12345;
//    user_esp_platform_set_active(0);
//    system_restore();
//
//    system_rtc_mem_write(70, &boot_flag, sizeof(boot_flag));
//    printf("long_press boot_flag %d  \n",boot_flag);
//    system_rtc_mem_read(70, &boot_flag, sizeof(boot_flag));
//    printf("long_press boot_flag %d  \n",boot_flag);
//
//#if RESTORE_KEEP_TIMER
//    user_platform_timer_bkup();
//#endif
//
//    system_restart();
//}

/******************************************************************************
 * FunctionName : user_link_led_init
 * Description  : int led gpio
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
LOCAL void  
user_link_led_init(void)
{
    PIN_FUNC_SELECT(PLUG_LINK_LED_IO_MUX, PLUG_LINK_LED_IO_FUNC);
}

LOCAL void  
user_link_led_timer_cb(void)
{
    link_led_level = (~link_led_level) & 0x01;
    GPIO_OUTPUT_SET(GPIO_ID_PIN(PLUG_LINK_LED_IO_NUM), link_led_level);
}

void  
user_link_led_timer_init(int time)
{
    os_timer_disarm(&link_led_timer);
    os_timer_setfn(&link_led_timer, (os_timer_func_t *)user_link_led_timer_cb, NULL);
    os_timer_arm(&link_led_timer, time, 1);
    link_led_level = 0;
    GPIO_OUTPUT_SET(GPIO_ID_PIN(PLUG_LINK_LED_IO_NUM), link_led_level);
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
user_link_led_output(uint8 mode)
{

    switch (mode) {
        case LED_OFF:
            os_timer_disarm(&link_led_timer);
            GPIO_OUTPUT_SET(GPIO_ID_PIN(PLUG_LINK_LED_IO_NUM), 1);
            break;
    
        case LED_ON:
            os_timer_disarm(&link_led_timer);
            GPIO_OUTPUT_SET(GPIO_ID_PIN(PLUG_LINK_LED_IO_NUM), 0);
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
    return ((system_adc_read() > 10) ? (1):(0));//get_key_status(single_key[0]);
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

    wifi_status_led_install(PLUG_WIFI_LED_IO_NUM, PLUG_WIFI_LED_IO_MUX, PLUG_WIFI_LED_IO_FUNC);

//    single_key[0] = key_init_single(PLUG_KEY_0_IO_NUM, PLUG_KEY_0_IO_MUX, PLUG_KEY_0_IO_FUNC,
//                                    user_plug_long_press, user_plug_short_press);
//
//    keys.key_num = PLUG_KEY_NUM;
//    keys.single_key = single_key;
//
//    key_init(&keys);

//    // relay open/close setting saved in flash and loaded at start up
//	  // Not safe, now at start up just always relay open
//    spi_flash_read((PRIV_PARAM_START_SEC + PRIV_PARAM_SAVE) * SPI_FLASH_SEC_SIZE,
//                (uint32 *)&plug_param, sizeof(struct plug_saved_param));

    relayInit();

    // default to be off, for safety.
//    if (plug_param.status == 0xff) {
//        plug_param.status = 0;
//    }

}
#endif

