/******************************************************************************
 * Copyright (C) 2014 -2016  Espressif System
 *
 * FileName: user_esp_platform.c
 *
 * Description: The client mode configration.
 *              Check your hardware connection with the host while use this mode.
 *
 * Modification history:
 * 2015/7/1, v1.0 create this file.
*******************************************************************************/
#include "esp_common.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "lwip/lwip/sys.h"
#include "lwip/lwip/ip_addr.h"
#include "lwip/lwip/netdb.h"
#include "lwip/lwip/sockets.h"
#include "lwip/lwip/err.h"
#include "lwip/apps/sntp.h"

#include "user_iot_version.h"
#include "smartconfig.h"

#include "upgrade.h"

#include "user_esp_platform.h"
#include "smart_socket_global.h"

//#define DEMO_TEST /*for test*/
#define DEBUG

#define ESP_DBG os_printf

#include "user_plug.h"

#define DOWNLOAD_FW_UPGRADE_HEADER "Connection: close\r\n\
Cache-Control: no-cache\r\n\
User-Agent: Mozilla/5.0 (Windows NT 5.1) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/30.0.1599.101 Safari/537.36 \r\n\
Accept: */*\r\n\
Authorization: token %s\r\n\
Accept-Encoding: gzip,deflate,sdch\r\n\
Accept-Language: zh-CN,zh;q=0.8\r\n\r\n"

#define REBOOT_MAGIC  (12345)

LOCAL int  user_boot_flag;
struct esp_platform_saved_param esp_param;
LOCAL struct rst_info rtc_info;
LOCAL uint8 iot_version[20];
LOCAL uint8 device_status;
LOCAL os_timer_t tCheckSNTPTimer;
//LOCAL xTaskHandle *ota_task_handle = NULL;

extern SmartSocketParameter_t tSmartSocketParameter;

/******************************************************************************
 * FunctionName : user_esp_platform_get_connect_status
 * Description  : get each connection step's status
 * Parameters   : none
 * Returns      : status
*******************************************************************************/
uint8
user_esp_platform_get_connect_status(void)
{
    uint8 status = wifi_station_get_connect_status();

    if (status == STATION_GOT_IP) {
        status = (device_status == 0) ? DEVICE_CONNECTING : device_status;
    }

    ESP_DBG("status %d\n", status);
    return status;
}

/******************************************************************************
 * FunctionName : user_esp_platform_set_connect_status
 * Description  : set each connection step's status
 * Parameters   : none
 * Returns      : status
*******************************************************************************/

void
user_esp_platform_set_connect_status(uint8 status)
{
    device_status = status;
}

/******************************************************************************
 * FunctionName : user_esp_platform_upgrade_cb
 * Description  : Processing the downloaded data from the server
 * Parameters   : pespconn -- the espconn used to connetion with the host
 * Returns      : none
*******************************************************************************/
LOCAL void
user_esp_platform_upgrade_rsp(void *arg)
{
    struct upgrade_server_info *server = arg;

    if (true == server->upgrade_flag){
    	system_upgrade_reboot();
    }

    if(server != NULL){
        free(server->url);
        server->url = NULL;
        free(server);
        server = NULL;
    }
}

/******************************************************************************
 * FunctionName : user_esp_platform_upgrade_begin
 * Description  : Processing the received data from the server
 * Parameters   : pespconn -- the espconn used to connetion with the host
 *                server -- upgrade param
 * Returns      : none
*******************************************************************************/
int user_esp_platform_upgrade_begin(void)
{
//    char cUserBin[10] = {0};
    struct upgrade_server_info *pUpgradeServer;

    pUpgradeServer = (struct upgrade_server_info *)zalloc(sizeof(struct upgrade_server_info));
    if (NULL == pUpgradeServer){
    	return (-1);
    }
//    server->pclient_param = NULL;
    bzero(&pUpgradeServer->sockaddrin, sizeof(struct sockaddr_in));

    pUpgradeServer->sockaddrin.sin_family = AF_INET;
    // I am quite clear what I am doing, compiler doesn't know
    pUpgradeServer->sockaddrin.sin_addr.s_addr = inet_addr(tSmartSocketParameter.sFW_UpgradeServer);
    pUpgradeServer->sockaddrin.sin_port = htons(tSmartSocketParameter.unFW_UpgradePort);

    pUpgradeServer->check_cb = user_esp_platform_upgrade_rsp;
    //server->check_times = 120000;/*rsp once finished*/

    if (pUpgradeServer->url == NULL) {
        pUpgradeServer->url = (uint8 *)zalloc(512);
    }
//
//    if (system_upgrade_userbin_check() == UPGRADE_FW_BIN1) {
//        memcpy(cUserBin, "user2.bin", 10);
//    } else if (system_upgrade_userbin_check() == UPGRADE_FW_BIN2) {
//        memcpy(cUserBin, "user1.bin", 10);
//    }

    sprintf(pUpgradeServer->url, "GET %s%u HTTP/1.0\r\nHost: %s:%d\r\n"DOWNLOAD_FW_UPGRADE_HEADER"",
			tSmartSocketParameter.sFW_UpgradeUrl, ((system_upgrade_userbin_check() == USER_BIN1) ? USER_BIN2 : USER_BIN1) + 1,
					tSmartSocketParameter.sFW_UpgradeHost,ntohs(pUpgradeServer->sockaddrin.sin_port), tSmartSocketParameter.sFW_UpgradeToken);//  IPSTR  IP2STR(server->sockaddrin.sin_addr.s_addr)
    ESP_DBG("%s\n",pUpgradeServer->url);

    if (system_upgrade_start(pUpgradeServer) == true) {
        ESP_DBG("upgrade is already started\n");
    }

    return 0;
}


void wifi_conn_event_cb(System_Event_t *event)
{
    if (event == NULL) {
        return;
    }

    switch (event->event_id) {
        case EVENT_STAMODE_GOT_IP:
        	device_status = DEVICE_GOT_IP;
//        	tSmartSocketParameter.tConfigure.bIPGotten = 1;
            break;
        default:
            break;
    }
}

/******************************************************************************
 * FunctionName : smartconfig_done
 * Description  : callback function which be called during the samrtconfig process
 * Parameters   : status -- the samrtconfig status
 *                pdata --
 * Returns      : none
*******************************************************************************/
void  
smartconfig_done(sc_status status, void *pdata)
{
    switch(status) {
        case SC_STATUS_WAIT:
            printf("SC_STATUS_WAIT\n");
            user_link_led_output(LED_1HZ);
            break;
        case SC_STATUS_FIND_CHANNEL:
            printf("SC_STATUS_FIND_CHANNEL\n");
            user_link_led_output(LED_1HZ);
            break;
        case SC_STATUS_GETTING_SSID_PSWD:
            printf("SC_STATUS_GETTING_SSID_PSWD\n");
            user_link_led_output(LED_20HZ);
            
            sc_type *type = pdata;
            if (*type == SC_TYPE_ESPTOUCH) {
                printf("SC_TYPE:SC_TYPE_ESPTOUCH\n");
            } else {
                printf("SC_TYPE:SC_TYPE_AIRKISS\n");
            }
            break;
        case SC_STATUS_LINK:
            printf("SC_STATUS_LINK\n");
            struct station_config *sta_conf = pdata;
    
            wifi_station_set_config(sta_conf);
            wifi_station_disconnect();
            wifi_station_connect();
            break;
        case SC_STATUS_LINK_OVER:
            printf("SC_STATUS_LINK_OVER\n");
            if (pdata != NULL) {
                uint8 phone_ip[4] = {0};
                memcpy(phone_ip, (uint8*)pdata, 4);
                printf("Phone ip: %d.%d.%d.%d\n",phone_ip[0],phone_ip[1],phone_ip[2],phone_ip[3]);
            }
            smartconfig_stop();
            
            user_link_led_output(LED_ON);
            device_status = DEVICE_GOT_IP;
//            tSmartSocketParameter.tConfigure.bIPGotten = 1;
            break;
    }

}

/******************************************************************************
 * FunctionName : smartconfig_task
 * Description  : start the samrtconfig proces and call back 
 * Parameters   : pvParameters
 * Returns      : none
*******************************************************************************/
void  
smartconfig_task(void *pvParameters)
{
    printf("smartconfig_task start\n");
    printf("wifi_set_opmode \n");
	wifi_set_opmode(STATION_MODE);
	printf("wifi_station_disconnect \n");
    wifi_station_disconnect();
	printf("smartconfig_stop \n");
	smartconfig_stop();
	printf("smartconfig_start \n");
    smartconfig_start(smartconfig_done);

    vTaskDelete(NULL);
}
/******************************************************************************
 * FunctionName : user_esp_platform_get_token
 * Description  : get the espressif's device token
 * Parameters   : token -- the parame point which write the flash
 * Returns      : none
*******************************************************************************/
void  
user_esp_platform_get_token(uint8_t *token)
{
    if (token == NULL) {
        return;
    }

    memcpy(token, esp_param.token, sizeof(esp_param.token));
}
/******************************************************************************
 * FunctionName : user_esp_platform_set_token
 * Description  : save the token for the espressif's device
 * Parameters   : token -- the parame point which write the flash
 * Returns      : none
*******************************************************************************/
void  
user_esp_platform_set_token(uint8_t *token)
{
    if (token == NULL) {
        return;
    }

    esp_param.activeflag = 0;
    esp_param.tokenrdy=1;
    
    if(strlen(token)<=40)
        memcpy(esp_param.token, token, strlen(token));
    else
        os_printf("ERR:arr_overflow,%u,%d\n",__LINE__,strlen(token));

    system_param_save_with_protect(ESP_PARAM_START_SEC, &esp_param, sizeof(esp_param));
}

/******************************************************************************
 * FunctionName : user_esp_platform_set_active
 * Description  : set active flag
 * Parameters   : activeflag -- 0 or 1
 * Returns      : none
*******************************************************************************/
void  
user_esp_platform_set_active(uint8 activeflag)
{
    esp_param.activeflag = activeflag;
    if(0 == activeflag){
    	esp_param.tokenrdy=0;
    }

    system_param_save_with_protect(ESP_PARAM_START_SEC, &esp_param, sizeof(esp_param));
}

/******************************************************************************
 * FunctionName : user_esp_platform_param_recover
 * Description  : espconn struct parame init when get ip addr
 * Parameters   : none
 * Returns      : none
*******************************************************************************/

LOCAL void  
user_esp_platform_param_recover(void)
{
    struct ip_info sta_info;
    struct dhcp_client_info dhcp_info;
    
    sprintf(iot_version,"%s%d.%d.%dt%d(%s)",VERSION_TYPE,IOT_VERSION_MAJOR,\
    IOT_VERSION_MINOR,IOT_VERSION_REVISION,device_type,UPGRADE_FALG);
    printf("IOT VERSION:%s\n", iot_version);

    
    system_rtc_mem_read(0,&rtc_info,sizeof(struct rst_info));
     if(rtc_info.reason == 1 || rtc_info.reason == 2) {
         ESP_DBG("flag = %d,epc1 = 0x%08x,epc2=0x%08x,epc3=0x%08x,excvaddr=0x%08x,depc=0x%08x,\nFatal \
exception (%d): \n",rtc_info.reason,rtc_info.epc1,rtc_info.epc2,rtc_info.epc3,rtc_info.excvaddr,rtc_info.depc,rtc_info.exccause);
     }
    struct rst_info info = {0};
    system_rtc_mem_write(0,&info,sizeof(struct rst_info));

    system_param_load(ESP_PARAM_START_SEC, 0, &esp_param, sizeof(esp_param));
    
    /***add by tzx for saving ip_info to avoid dhcp_client start****/
    /*
    system_rtc_mem_read(64,&dhcp_info,sizeof(struct dhcp_client_info));
    if(dhcp_info.flag == 0x01 ) {
        printf("set default ip ??\n");
        sta_info.ip = dhcp_info.ip_addr;
        sta_info.gw = dhcp_info.gw;
        sta_info.netmask = dhcp_info.netmask;
        if ( true != wifi_set_ip_info(STATION_IF,&sta_info)) {
            printf("set default ip wrong\n");
        }
    }
    memset(&dhcp_info,0,sizeof(struct dhcp_client_info));
    system_rtc_mem_write(64,&dhcp_info,sizeof(struct rst_info));
*/
    system_rtc_mem_read(70, &user_boot_flag, sizeof(user_boot_flag));
    
    int boot_flag = 0xffffffff;
    system_rtc_mem_write(70,&boot_flag,sizeof(boot_flag));

    /*restore system timer after reboot, note here not for power off */
    // Move this restore to SNTP OK
 //   user_platform_timer_restore();
}

//LOCAL void
//user_platform_stationap_enable(void)
//{
//    wifi_set_opmode(STATIONAP_MODE);
//}

void PLTFM_startSmartConfig(void)
{
//	printf("No previous AP record found, enter smart config. \n");
	printf("device_status = 0 \n");
	device_status = 0; //tSmartSocketParameter.tConfigure.bIPGotten = 0; //
	printf("xTaskCreate \n");
	xTaskCreate(smartconfig_task, "smartconfig_task", 256, NULL, 2, NULL);
}

void reconnectAP(void)
{
	/* entry station mode and connect to ap cached */
//	printf("entry station mode to connect server \n");
	wifi_set_opmode(STATION_MODE);
	wifi_set_event_handler_cb(wifi_conn_event_cb);
}

void
user_check_sntp_stamp(void)
{
	uint32 current_stamp;
	current_stamp = sntp_get_current_timestamp();
	if(current_stamp == 0){
		os_timer_arm(&tCheckSNTPTimer, 2000, 0);
	}else{
		os_timer_disarm(&tCheckSNTPTimer);
		os_printf("sntp: %d, %s \n",current_stamp, sntp_get_real_time(current_stamp));
		if (false == PLTF_isTimerRunning()){
			user_platform_timer_restore();
		}
	}
}

void PLTF_startSBTP(void)
{
	sntp_stop();
	sntp_setservername(0, tSmartSocketParameter.sSNTP_Server[0]);
	sntp_setservername(1, tSmartSocketParameter.sSNTP_Server[1]);
	sntp_setservername(2, tSmartSocketParameter.sSNTP_Server[2]);
	sntp_set_timezone(0);
	sntp_init();

    os_timer_disarm(&tCheckSNTPTimer);
    os_timer_setfn(&tCheckSNTPTimer, (os_timer_func_t *)user_check_sntp_stamp, NULL);
    os_timer_arm(&tCheckSNTPTimer, 2000, 0);
}
/******************************************************************************
 * FunctionName : user_esp_platform_init
 * Description  : device parame init based on espressif platform
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
void  
user_esp_platform_maintainer(void *pvParameters)
{
    int ret;
    struct station_config *sta_config;

    user_esp_platform_param_recover();

    user_plug_init();

    wifi_station_ap_number_set(AP_CACHE_NUMBER);

	sta_config = (struct station_config *)zalloc(sizeof(struct station_config)*5);
	ret = wifi_station_get_ap_info(sta_config);
	free(sta_config);
	printf("wifi_station_get_ap num %d\n",ret);
	if(0 == ret) {
		/*AP_num == 0, no ap cached,start smartcfg*/
		PLTFM_startSmartConfig();

		while(device_status < DEVICE_GOT_IP){ //tSmartSocketParameter.tConfigure.bIPGotten != 1){
			ESP_DBG("Smart configuring...\n");
			vTaskDelay(2000 / portTICK_RATE_MS);
		}

	}else{
		reconnectAP();

		while(device_status < DEVICE_GOT_IP){ //tSmartSocketParameter.tConfigure.bIPGotten != 1){
			ESP_DBG("Connecting...\n");
			vTaskDelay(2000 / portTICK_RATE_MS);
		}
	}

	PLTF_startSBTP();

    while(1){
    	// Internet functions will be located here to connect to server and listening
    	// if there is any command like relay status change, upgrade or timer update
    	// even if you are not in local network with smart plug

//    	if ((1 == tSmartSocketParameter.tConfigure.bRelayScheduleEnable) &&
//    			(0 == tSmartSocketParameter.tConfigure.bCurrentFailed)){
//    		user_plug_relay_schedule_action(sntp_get_current_timestamp());
//    	}

    	vTaskDelay(5000/portTICK_RATE_MS);
    }
    printf("user_esp_platform_maintainer task end.\n");
    vTaskDelete(NULL);

}

void user_esp_platform_init(void)
{
    xTaskCreate(user_esp_platform_maintainer, "platform_maintainer", 384, NULL, 5, NULL);//512, 274 left,384
}

//sint8   user_esp_platform_deinit(void)
//{
//    bool ValueToSend = true;
//    portBASE_TYPE xStatus;
//    if (QueueStop == NULL)
//        return -1;
//
//    xStatus = xQueueSend(QueueStop,&ValueToSend,0);
//    if (xStatus != pdPASS){
//        ESP_DBG("WEB SEVER Could not send to the queue!\n");
//        return -1;
//    } else {
//        taskYIELD();
//        return pdPASS;
//    }
//}

