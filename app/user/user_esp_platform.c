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

#define RESPONSE_FRAME  "{\"status\": 200, \"datapoint\": {\"x\": %d}, \"nonce\": %d, \"deliver_to_device\": true}\n"
#define FIRST_FRAME     "{\"nonce\": %d, \"path\": \"/v1/device/identify\", \"method\": \"GET\",\"meta\": {\"Authorization\": \"token %s\"}}\n"

#define BEACON_FRAME    "{\"path\": \"/v1/ping/\", \"method\": \"POST\",\"meta\": {\"Authorization\": \"token %s\"}}\n"
#define RPC_RESPONSE_FRAME  "{\"status\": 200, \"nonce\": %d, \"deliver_to_device\": true}\n"
#define TIMER_FRAME     "{\"body\": {}, \"get\":{\"is_humanize_format_simple\":\"true\"},\"meta\": {\"Authorization\": \"Token %s\"},\"path\": \"/v1/device/timers/\",\"post\":{},\"method\": \"GET\"}\n"
#define pheadbuffer "Connection: close\r\n\
Cache-Control: no-cache\r\n\
User-Agent: Mozilla/5.0 (Windows NT 5.1) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/30.0.1599.101 Safari/537.36 \r\n\
Accept: */*\r\n\
Authorization: token %s\r\n\
Accept-Encoding: gzip,deflate,sdch\r\n\
Accept-Language: zh-CN,zh;q=0.8\r\n\r\n"

//LOCAL uint8 ping_status=TRUE;

#define ACTIVE_FRAME    "{\"nonce\": %d,\"path\": \"/v1/device/activate/\", \"method\": \"POST\", \"body\": {\"encrypt_method\": \"PLAIN\", \"token\": \"%s\", \"bssid\": \""MACSTR"\",\"rom_version\":\"%s\"}, \"meta\": {\"Authorization\": \"token %s\"}}\n"
#define UPGRADE_FRAME  "{\"path\": \"/v1/messages/\", \"method\": \"POST\", \"meta\": {\"Authorization\": \"token %s\"},\
\"get\":{\"action\":\"%s\"},\"body\":{\"pre_rom_version\":\"%s\",\"rom_version\":\"%s\"}}\n"

#define REBOOT_MAGIC  (12345)

LOCAL int  user_boot_flag;

struct esp_platform_saved_param esp_param;

LOCAL uint8 device_status;

LOCAL struct rst_info rtc_info;

LOCAL uint8 iot_version[20];

LOCAL struct client_conn_param client_param;

extern SmartSocketParameter_t tSmartSocketParameter;

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
            
            user_link_led_output(LED_OFF);
            device_status = DEVICE_GOT_IP;
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
 * FunctionName : user_esp_platform_parse_nonce
 * Description  : parse the device nonce
 * Parameters   : pbuffer -- the recivce data point
 * Returns      : the nonce
*******************************************************************************/
int  
user_esp_platform_parse_nonce(char *pbuffer)
{
    char *pstr = NULL;
    char *pparse = NULL;
    char noncestr[11] = {0};
    int nonce = 0;
    pstr = (char *)strstr(pbuffer, "\"nonce\": ");

    if (pstr != NULL) {
        pstr += 9;
        pparse = (char *)strstr(pstr, ",");

        if (pparse != NULL) {

            if( (pparse - pstr)<=11)
                memcpy(noncestr, pstr, pparse - pstr);
            else
                os_printf("ERR:arr_overflow,%u,%d\n",__LINE__,(pparse - pstr));
            
        } else {
            pparse = (char *)strstr(pstr, "}");

            if (pparse != NULL) {
                if( (pparse - pstr)<=11)
                    memcpy(noncestr, pstr, pparse - pstr);
                else
                    os_printf("ERR:arr_overflow,%u,%d\n",__LINE__,(pparse - pstr));
            } else {
                pparse = (char *)strstr(pstr, "]");

                if (pparse != NULL) {
                    if( (pparse - pstr)<=11)
                        memcpy(noncestr, pstr, pparse - pstr);
                    else
                        os_printf("ERR:arr_overflow,%u,%d\n",__LINE__,(pparse - pstr));
                } else {
                    return 0;
                }
            }
        }

        nonce = atoi(noncestr);
    }

    return nonce;
}

/******************************************************************************
 * FunctionName : user_esp_platform_get_info
 * Description  : get and update the espressif's device status
 * Parameters   : pespconn -- the espconn used to connect with host
 *                pbuffer -- prossing the data point
 * Returns      : none
*******************************************************************************/
void  
user_esp_platform_get_info( struct client_conn_param *pclient_param, uint8 *pbuffer)
{
    char *pbuf = NULL;
    int nonce = 0;

    pbuf = (char *)zalloc(packet_size);

    nonce = user_esp_platform_parse_nonce(pbuffer);

    if (pbuf != NULL) {
        sprintf(pbuf, RESPONSE_FRAME, user_plug_get_status(), nonce);

        ESP_DBG("%s\n", pbuf);
        write(pclient_param->sock_fd, pbuf, strlen(pbuf));
        free(pbuf);
        pbuf = NULL;
    }
}

/******************************************************************************
 * FunctionName : user_esp_platform_set_info
 * Description  : prossing the data and controling the espressif's device
 * Parameters   : sockfd -- the socket handle used to connect with host
 *                pbuffer -- prossing the data point
 * Returns      : none
*******************************************************************************/
void  
user_esp_platform_set_info( struct client_conn_param *pclient_param, uint8 *pbuffer)
{

#if PLUG_DEVICE
    char *pstr = NULL;
    pstr = (char *)strstr(pbuffer, "plug-status");

    if (pstr != NULL) {
        pstr = (char *)strstr(pbuffer, "body");

        if (pstr != NULL) {

            if (strncmp(pstr + 27, "1", 1) == 0) {
                user_plug_set_status(0x01);
            } else if (strncmp(pstr + 27, "0", 1) == 0) {
                user_plug_set_status(0x00);
            }
        }
    }
    user_esp_platform_get_info(pclient_param, pbuffer);
}

/******************************************************************************
 * FunctionName : user_mdns_conf
 * Description  : mdns init
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
struct mdns_info {
	char *host_name;
	char *server_name;
	uint16 server_port;
	unsigned long ipAddr;
	char *txt_data[10];
};


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
    user_platform_timer_restore();
}

/******************************************************************************
 * FunctionName : user_esp_platform_param_recover
 * Description  : espconn struct parame init when get ip addr
 * Parameters   : none
 * Returns      : none
*******************************************************************************/

//LOCAL void
//user_platform_stationap_enable(void)
//{
//    wifi_set_opmode(STATIONAP_MODE);
//}

void startSmartConfig(void)
{
//	printf("No previous AP record found, enter smart config. \n");
	wifi_set_opmode(STATION_MODE);
	smartconfig_stop();
	xTaskCreate(smartconfig_task, "smartconfig_task", 256, NULL, 2, NULL);

	while(device_status != DEVICE_GOT_IP){
		ESP_DBG("configing...\n");
		vTaskDelay(2000 / portTICK_RATE_MS);
	}
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
    u8 timeout_count = 0;
    int32 nNetTimeout=1000;// 1 Sec
    uint32_t unSNTPTime;

    u8 connect_retry_c;
    u8 total_connect_retry_c;

    struct ip_info sta_ipconfig;
    
    bool ValueFromReceive = false;
    portBASE_TYPE xStatus;

    int stack_counter=0;
    int quiet=0;
    
    client_param.sock_fd=-1;
    
    user_esp_platform_param_recover();

    user_plug_init();

    wifi_station_ap_number_set(AP_CACHE_NUMBER);

    //printf("rtc_info.reason 000-%d, 000-%d reboot, flag %d\n",rtc_info.reason,(REBOOT_MAGIC == user_boot_flag),user_boot_flag);

    /*if not reboot back, power on with key pressed, enter stationap mode*/
//    if( (REBOOT_MAGIC != user_boot_flag) && (1 == user_get_key_status())){
//        /*device power on with stationap mode defaultly, neednt config again*/
//        //user_platform_stationap_enable();
//        printf("enter softap+station mode\n");
//
//        user_link_led_output(LED_ON);//gpio 12
//
//        //for cloud test only
//        if(0){/*for test*/
//            struct station_config *sta_config = (struct station_config *)zalloc(sizeof(struct station_config));
//            wifi_station_get_ap_info(sta_config);
//            memset(sta_config, 0, sizeof(struct station_config));
//            sprintf(sta_config->ssid, "IOT_DEMO_TEST");
//            sprintf(sta_config->password, "0000");
//            wifi_station_set_config(sta_config);
//            free(sta_config);
//        }
//
//        if(TRUE == esp_param.tokenrdy)
//        	goto Local_mode;
//
//    }else{
        struct station_config *sta_config5 = (struct station_config *)zalloc(sizeof(struct station_config)*5);
        ret = wifi_station_get_ap_info(sta_config5);
        free(sta_config5);
        printf("wifi_station_get_ap num %d\n",ret);
        if(0 == ret) {
			/*AP_num == 0, no ap cached,start smartcfg*/
        	startSmartConfig();
        }else{
            /* entry station mode and connect to ap cached */
            printf("entry station mode to connect server \n");
            wifi_set_opmode(STATION_MODE);
        }
//    }

        sntp_setservername(0, tSmartSocketParameter.cSNTP_Server[0]);
        sntp_setservername(1, tSmartSocketParameter.cSNTP_Server[1]);
        sntp_setservername(2, tSmartSocketParameter.cSNTP_Server[2]);
        sntp_init();

//Local_mode:
//	wifi_set_opmode(STATIONAP_MODE);
//
//    if(client_param.sock_fd >= 0)
//    	close(client_param.sock_fd);
//
//    vQueueDelete(QueueStop);
//    QueueStop = NULL;
    while(1){
//        unSNTPTime = sntp_get_current_timestamp();
//        os_printf("time:%u\r\n",unSNTPTime);
//        os_printf("date:%s\r\n",sntp_get_real_time(unSNTPTime));
    	if (1 == tSmartSocketParameter.tConfigure.bReSmartConfig){
    		device_status = DEVICE_CONNECTING;

            wifi_station_disconnect();

//            wifi_station_set_config(sta_conf);

//    		smartconfig_stop();
    		startSmartConfig();
    		tSmartSocketParameter.tConfigure.bReSmartConfig = 0;
    	}
    	vTaskDelay(1000/portTICK_RATE_MS);
    }
    printf("user_esp_platform_maintainer task end.\n");
    vTaskDelete(NULL);

}


void   user_esp_platform_init(void)
{
//    if (QueueStop == NULL)
//        QueueStop = xQueueCreate(1,1);

//    if (QueueStop != NULL){
        xTaskCreate(user_esp_platform_maintainer, "platform_maintainer", 384, NULL, 5, NULL);//512, 274 left,384
//    }
        
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

#endif
