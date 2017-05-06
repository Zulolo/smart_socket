/******************************************************************************
 * Copyright (C) 2014 -2016  Espressif System
 *
 * FileName: user_main.c
 *
 * Description: get and config the device timer
 *
 * Modification history:
 * 2015/7/1, v1.0 create this file.
*******************************************************************************/
//git test2
#include "esp_common.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "user_config.h"
#include "user_devicefind.h"
#include "user_webserver.h"
#ifdef ESP_PLATFORM
#include "user_esp_platform.h"
#endif

#if HTTPD_SERVER
#include "espfsformat.h"
#include "espfs.h"
#include "captdns.h"
#include "httpd.h"
#include "cgiwifi.h"
#include "httpdespfs.h"
#include "user_cgi.h"
#include "webpages-espfs.h"

#ifdef ESPFS_HEATSHRINK
#include "heatshrink_config_custom.h"
#include "heatshrink_decoder.h"
#endif

#endif

#ifdef SERVER_SSL_ENABLE
#include "ssl/cert.h"
#include "ssl/private_key.h"
#else
    
#ifdef CLIENT_SSL_ENABLE
unsigned char *default_certificate;
unsigned int default_certificate_len = 0;
unsigned char *default_private_key;
unsigned int default_private_key_len = 0;
#endif
#endif

#include "cs5463.h"
#include "smart_socket_global.h"
#include "smart_config.h"
#include "user_plug.h"


//TODO 1. Use same coding standard (style)
// 2. Check all return value including system API like zalloc and home made
// 3. remove unused resources
// 4. shrink some buffer size
// 5. shrink flash size by put all same string like some error description into same const array

#if HTTPD_SERVER
HttpdBuiltInUrl builtInUrls[]={
	{"*", cgiRedirectApClientToHostname, "esp.nonet"},
	{"/", cgiRedirect, "/index.html"},
	{"/wifi", cgiRedirect, "/wifi/wifi.tpl"},
	{"/wifi/wifiscan.cgi", cgiWiFiScan, NULL},
	{"/wifi/wifi.tpl", cgiEspFsTemplate, tplWlan},
	{"/wifi/connect.cgi", cgiWiFiConnect, NULL},
	{"/wifi/connstatus.cgi", cgiWiFiConnStatus, NULL},
	{"/wifi/setmode.cgi", cgiWiFiSetMode, NULL},

	{"/config", cgiEspApi, NULL},
	{"/client", cgiEspApi, NULL},
	{"/upgrade", cgiEspApi, NULL},

	{"*", cgiEspFsHook, NULL}, //Catch-all cgi function for the filesystem
	{NULL, NULL, NULL} //end marker
};
#endif

extern xSemaphoreHandle xSmartSocketEventListSemaphore;
extern xSemaphoreHandle xSmartSocketParameterSemaphore;
extern xSemaphoreHandle xSmartSocketCaliSemaphore;
extern SmartSocketEventList_t tSmartSocketEventList;
extern SmartSocketParameter_t tSmartSocketParameter;
/******************************************************************************
 * FunctionName : user_rf_cal_sector_set
 * Description  : SDK just reversed 4 sectors, used for rf init data and paramters.
 *                We add this function to force users to set rf cal sector, since
 *                we don't know which sector is free in user's application.
 *                sector map for last several sectors : ABCCC
 *                A : rf cal
 *                B : rf init data
 *                C : sdk parameters
 * Parameters   : none
 * Returns      : rf cal sector
*******************************************************************************/
uint32 user_rf_cal_sector_set(void)
{
    flash_size_map size_map = system_get_flash_size_map();
    uint32 rf_cal_sec = 0;

    switch (size_map) {
        case FLASH_SIZE_4M_MAP_256_256:
            rf_cal_sec = 128 - 5;
            break;

        case FLASH_SIZE_8M_MAP_512_512:
            rf_cal_sec = 256 - 5;
            break;

        case FLASH_SIZE_16M_MAP_512_512:
        case FLASH_SIZE_16M_MAP_1024_1024:
            rf_cal_sec = 512 - 5;
            break;

        case FLASH_SIZE_32M_MAP_512_512:
        case FLASH_SIZE_32M_MAP_1024_1024:
            rf_cal_sec = 1024 - 5;
            break;

        default:
            rf_cal_sec = 0;
            break;
    }

    return rf_cal_sec;
}

void cs5463Task(void)
{
	printf("CS5463 manager start.\n");
	xTaskCreate(CS5463_Manager, "cs5463_manager", 256, NULL, 2, NULL);
}

int32_t systemInit(void)
{
	vSemaphoreCreateBinary(xSmartSocketEventListSemaphore);
	vSemaphoreCreateBinary(xSmartSocketParameterSemaphore);
	vSemaphoreCreateBinary(xSmartSocketCaliSemaphore);

	if((NULL == xSmartSocketEventListSemaphore) || (NULL == xSmartSocketParameterSemaphore)){
		printf("Create semaphore failed.\n");
		return (-1);
	}

    if(xSemaphoreTake(xSmartSocketParameterSemaphore, (portTickType)(10000/portTICK_RATE_MS)) == pdTRUE ){
        if (system_param_load(GET_USER_DATA_SECTORE(USER_DATA_CONF_PARA), 0,
        		&tSmartSocketParameter, sizeof(tSmartSocketParameter)) != true){
        	xSemaphoreGive(xSmartSocketParameterSemaphore);
    		printf("Load initial parameter failed.\n");
    		return (-1);
        }
        xSemaphoreGive(xSmartSocketParameterSemaphore);
    }else{
		printf("Take parameter semaphore failed.\n");
		return (-1);
    }

	if (tSmartSocketParameter.unValidation != 0xA5A5A5A5){
		// first run
		DAT_nFlashDataClean();
	}

	tSmartSocketParameter.tConfigure.bCS5463Cali = 0;
	tSmartSocketParameter.tConfigure.bCurrentFailed = 0;
	tSmartSocketParameter.tConfigure.bJustLongPressed = 0;
	tSmartSocketParameter.tConfigure.bFWUpgradeReset = 0;
	return 0;
}
/******************************************************************************
 * FunctionName : user_init
 * Description  : entry of user application, init user function here
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
extern void websocket_start(void);
void user_init(void)
{
	printf("Smart plug daemon start.\n");

    printf("SDK version:%s,%u\n", system_get_sdk_version(),__LINE__ );

    systemInit();

    /*Initialization of the peripheral drivers*/
    /*For light demo , it is user_light_init();*/
    /* Also check whether assigned ip addr by the router.If so, connect to ESP-server  */
    user_esp_platform_init();

    // cs5463Task() need to be put after user_esp_platform_init() because
    // cs5463Task() may need to control relay in calibration process and
    // user_esp_platform_init() will initial relay control
    cs5463Task();

    /*Establish a udp socket to receive local device detect info.*/
    /*Listen to the port 1025, as well as udp broadcast.
    /*If receive a string of device_find_request, it rely its IP address and MAC.*/
    user_devicefind_start();

#if WEB_SERVICE
    /*Establish a TCP server for http(with JSON) POST or GET command to communicate with the device.*/
    /*You can find the command in "2B-SDK-Espressif IoT Demo.pdf" to see the details.*/
    user_webserver_start();

#elif HTTPD_SERVER
//	/*Initialize DNS server for captive portal*/
//	captdnsInit();

	/*Initialize espfs containing static webpages*/
//    espFsInit((void*)(webpages_espfs_start));

	/*Initialize webserver*/
	httpdInit(builtInUrls, 80);

	websocket_start();
#endif

}


