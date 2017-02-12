/******************************************************************************
 * Copyright 2015-2017 Espressif Systems
 *
 * FileName: user_cgi.c
 *
 * Description: Specialized functions that provide an API into the 
 * functionality this ESP provides.
 *
 *******************************************************************************/
#include "esp_common.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include "lwip/mem.h"
#include "lwip/sockets.h"
#include "json/cJSON.h"

#include "user_iot_version.h"

#include "user_esp_platform.h"

#include "user_plug.h"

#include "upgrade.h"

#include "user_cgi.h"

#include "cs5463.h"
#include "smart_socket_global.h"
#include "user_data.h"

/******************************************************************************/
typedef struct _scaninfo {
    STAILQ_HEAD(, bss_info) *pbss;
    struct single_conn_param *psingle_conn_param;
    uint8 totalpage;
    uint8 pagenum;
    uint8 page_sn;
    uint8 data_cnt;
} scaninfo;
LOCAL scaninfo *pscaninfo;

//extern xSemaphoreHandle xSmartSocketEventListSemaphore;
extern xSemaphoreHandle xSmartSocketParameterSemaphore;
//extern SmartSocketEventList_t tSmartSocketEventList;
extern SmartSocketParameter_t tSmartSocketParameter;

LOCAL os_timer_t *restart_xms;
LOCAL rst_parm *rstparm;
LOCAL struct station_config *sta_conf;
LOCAL struct softap_config *ap_conf;


/******************************************************************************
 * FunctionName : user_binfo_get
 * Description  : get the user bin paramer as a JSON format
 * Parameters   : pcjson -- A pointer to a JSON object
 * Returns      : result
* { "status":"200", "user_bin":"userx.bin" }
*******************************************************************************/
LOCAL int  
user_binfo_get(cJSON *pcjson, const char* pname)
{
    char buff[12];

    cJSON_AddStringToObject(pcjson, "status", "200");
    if(system_upgrade_userbin_check() == 0x00) {
         sprintf(buff, "user1.bin");
    } else if (system_upgrade_userbin_check() == 0x01) {
         sprintf(buff, "user2.bin");
    } else{
        printf("system_upgrade_userbin_check fail\n");
        return -1;
    }
    cJSON_AddStringToObject(pcjson, "user_bin", buff);

    return 0;
}
/******************************************************************************
 * FunctionName : system_info_get
 * Description  : get the user bin paramer as a JSON format
 * Parameters   : pcjson -- A pointer to a JSON object
 * Returns      : result
{"Version":{"hardware":"0.3","sdk_version":"1.1.2","iot_version":"v1.0.5t23701(a)"},
"Device":{"product":"Plug","manufacturer":"Hai mei xinag hao"}}
*******************************************************************************/
LOCAL int  
system_info_get(cJSON *pcjson, const char* pname )
{
    char buff[64]={0};

    cJSON * pSubJson_Version = cJSON_CreateObject();
    if(NULL == pSubJson_Version){
        printf("pSubJson_Version creat fail\n");
        return -1;
    }
    cJSON_AddItemToObject(pcjson, "Version", pSubJson_Version);
    
    cJSON * pSubJson_Device = cJSON_CreateObject();
    if(NULL == pSubJson_Device){
        printf("pSubJson_Device creat fail\n");
        return -1;
    }
    cJSON_AddItemToObject(pcjson, "Device", pSubJson_Device);

    cJSON_AddStringToObject(pSubJson_Version,"hardware","0.3");
    cJSON_AddStringToObject(pSubJson_Version,"sdk_version",system_get_sdk_version());
    sprintf(buff,"%s%d.%d.%s %d(%s)",VERSION_TYPE,IOT_VERSION_MAJOR,\
    IOT_VERSION_MINOR,__TIME__,device_type,UPGRADE_FALG);
    cJSON_AddStringToObject(pSubJson_Version,"iot_version",buff);
    
    cJSON_AddStringToObject(pSubJson_Device,"manufacture","Ming zhi hai mei qi hao");

    cJSON_AddStringToObject(pSubJson_Device,"product", "Smart Plug");

    //char * p = cJSON_Print(pcjson);
    //printf("@.@ system_info_get exit with  %s len:%d \n", p, strlen(p));

    return 0;
}

/******************************************************************************
 * FunctionName : signal_strength_get
 * Description  : get wifi signal strength
 * Parameters   : pcjson -- A pointer to a JSON object
 * Returns      : result
{"response":{
"signal":-84}}
*******************************************************************************/
LOCAL int
signal_strength_get(cJSON *pcjson, const char* pname )
{
    cJSON * pSubJson_response = cJSON_CreateObject();

    if(NULL == pSubJson_response){
        printf("pSubJson_response create fail\n");
        return -1;
    }

    cJSON_AddItemToObject(pcjson, "response", pSubJson_response);

    cJSON_AddNumberToObject(pSubJson_response, "signal", wifi_station_get_rssi());
    return 0;
}

/******************************************************************************
 * FunctionName : current_value_get
 * Description  : set up current value as a JSON format
 * Parameters   : pcjson -- A pointer to a JSON object
 * Returns      : result
{"response":{
"current":3.2,
"unit":"A"}}
*******************************************************************************/
LOCAL int
current_value_get(cJSON *pcjson, const char* pname )
{
    cJSON * pSubJson_response = cJSON_CreateObject();

    if(NULL == pSubJson_response){
        printf("pSubJson_response create fail\n");
        return -1;
    }

    cJSON_AddItemToObject(pcjson, "response", pSubJson_response);

    cJSON_AddNumberToObject(pSubJson_response, "current", CS5463_fGetCurrent());
    cJSON_AddStringToObject(pSubJson_response, "unit", "A");
    return 0;
}

/******************************************************************************
 * FunctionName : voltage_value_get
 * Description  : set up voltage value as a JSON format
 * Parameters   : pcjson -- A pointer to a JSON object
 * Returns      : result
{"response":{
"voltage":3.2,
"unit":"V"}}
*******************************************************************************/
LOCAL int
voltage_value_get(cJSON *pcjson, const char* pname )
{
    cJSON * pSubJson_response = cJSON_CreateObject();

    if(NULL == pSubJson_response){
        printf("pSubJson_response create fail\n");
        return -1;
    }

    cJSON_AddItemToObject(pcjson, "response", pSubJson_response);

    cJSON_AddNumberToObject(pSubJson_response, "voltage", CS5463_fGetVoltage());
    cJSON_AddStringToObject(pSubJson_response, "unit", "V");
    return 0;
}

/******************************************************************************
 * FunctionName : power_value_get
 * Description  : set up power value as a JSON format
 * Parameters   : pcjson -- A pointer to a JSON object
 * Returns      : result
{"response":{
"power":3.2,
"unit":"W"}}
*******************************************************************************/
LOCAL int
power_value_get(cJSON *pcjson, const char* pname )
{
    cJSON * pSubJson_response = cJSON_CreateObject();

    if(NULL == pSubJson_response){
        printf("pSubJson_response creat fail\n");
        return -1;
    }

    cJSON_AddItemToObject(pcjson, "response", pSubJson_response);

    cJSON_AddNumberToObject(pSubJson_response, "power", CS5463_fGetPower());
    cJSON_AddStringToObject(pSubJson_response, "unit", "W");
    return 0;
}

/******************************************************************************
 * FunctionName : temperature_value_get
 * Description  : set up temperature value as a JSON format
 * Parameters   : pcjson -- A pointer to a JSON object
 * Returns      : result
{"response":{
"temperature":24,
"unit":"C"}}
*******************************************************************************/
LOCAL int
temperature_value_get(cJSON *pcjson, const char* pname )
{
    cJSON * pSubJson_response = cJSON_CreateObject();

    if(NULL == pSubJson_response){
        printf("pSubJson_response creat fail\n");
        return -1;
    }
    cJSON_AddItemToObject(pcjson, "response", pSubJson_response);

    cJSON_AddNumberToObject(pSubJson_response, "temperature", CS5463_fGetTemperature());
    cJSON_AddStringToObject(pSubJson_response, "unit", "C");
    return 0;
}

/******************************************************************************
 * FunctionName : trend_get
 * Description  : set up trend as a JSON format
 * Parameters   : pcjson -- A pointer to a JSON object
 * Parameters   : pname -- "command=trend&start_time=2233&end_time=4455"
 * (http://192.168.1.130/client?command=trend&start_time=2233&end_time=4455)
 * Returns      : result
{"trend":{"time":145879,
"type":3,
"data":1}}
*******************************************************************************/
LOCAL int
trend_get(cJSON *pcjson, const char* pname)
{
    cJSON * pSubJsonEvent;
    uint32_t unStartTime, unEndTime;
    TrendContent_t* pTrendContent;
    uint8_t unTrendRecordNum, unTrendRecordIndex;

//    printf("Get trend:%s.\n", pname);
    if (sscanf(pname, "command=trend&start_time=%u&end_time=%u", &unStartTime, &unEndTime) != 2){
        printf("Trend select time frame format error.\n");
        return (-1);
    }

    // Get oldest and latest record time
    if ((0 == unStartTime) && (0 == unEndTime)){
    	if (DAT_nGetTrendLength(&unStartTime, &unEndTime) < 0){
    		printf("Get trend length error.\n");
    		return (-1);
    	}else{
    		pSubJsonEvent = cJSON_CreateObject();
    	    if(NULL == pSubJsonEvent){
    	        printf("pSubJsonEvent creat fail\n");
    	        return (-1);
    	    }
    		cJSON_AddItemToObject(pcjson, "trendLength", pSubJsonEvent);
            cJSON_AddNumberToObject(pSubJsonEvent, "startTime", unStartTime);
            cJSON_AddNumberToObject(pSubJsonEvent, "endTime", unEndTime);
            return 0;
    	}
    }

    if (unEndTime <= unStartTime){
		printf("End time can not early than start time.\n");
		return -1;
    }
    // Get trend record, only return max MAX_TREND_NUM_PER_QUERY
    if ((unEndTime - unStartTime) > MAX_TREND_QUERY_LEN){
    	unStartTime = unEndTime - MAX_TREND_QUERY_LEN;
    }
    pTrendContent = DAT_pGetTrends(unStartTime, unEndTime, &unTrendRecordNum);
    if ((NULL == pTrendContent) || (0 == unTrendRecordNum)){
    	printf("Get record failed.\n");
    	return (-1);
    }else{
    	for (unTrendRecordIndex = 0; unTrendRecordIndex < unTrendRecordNum; unTrendRecordIndex++){
    		pSubJsonEvent = cJSON_CreateObject();
			if(NULL == pSubJsonEvent){
				printf("pSubJsonEvent creat fail\n");
				return (-1);
			}
    		cJSON_AddItemToObject(pcjson, "trend", pSubJsonEvent);
            cJSON_AddNumberToObject(pSubJsonEvent, "time", pTrendContent[unTrendRecordIndex].unTime);
            cJSON_AddNumberToObject(pSubJsonEvent, "power", pTrendContent[unTrendRecordIndex].fPower);
    	}
    	free(pTrendContent);
    	return 0;
    }
}

/******************************************************************************
 * FunctionName : event_history_get
 * Description  : set up event history as a JSON format
 * Parameters   : pcjson -- A pointer to a JSON object
 * Returns      : result
{"event":{"time":145879,
"type":3,
"data":1}}
*******************************************************************************/
LOCAL int
event_history_get(cJSON *pcjson, const char* pname )
{
    cJSON * pSubJsonEvent = cJSON_CreateObject();
    SmartSocketEvent_t tEvent;

//    printf("Get event history.\n");
    if(NULL == pSubJsonEvent){
        printf("pSubJsonEvent creat fail\n");
        return -1;
    }
    cJSON_AddItemToObject(pcjson, "event", pSubJsonEvent);
    tEvent = DAT_nGetEventHistory(0);
    if (SMART_SOCKET_EVENT_INVALID != tEvent.tEventType){
        cJSON_AddNumberToObject(pSubJsonEvent, "time", tEvent.unTime);
        cJSON_AddNumberToObject(pSubJsonEvent, "type", tEvent.tEventType);
        cJSON_AddNumberToObject(pSubJsonEvent, "data", tEvent.data);
    }
    return 0;
}

/******************************************************************************
 * FunctionName : manual_enable_get
 * Description  : get if button control relay is enabled or not
 * Parameters   : pcjson -- A pointer to a JSON object
 * Returns      : result
{"response":{"enable":0}}
*******************************************************************************/
LOCAL int
manual_enable_get(cJSON *pcjson, const char* pname )
{
    cJSON * pSubJson_response = cJSON_CreateObject();
    if(NULL == pSubJson_response){
        printf("pSubJson_response create fail\n");
        return (-1);
    }
    cJSON_AddItemToObject(pcjson, "response", pSubJson_response);

    cJSON_AddNumberToObject(pSubJson_response, "enable", tSmartSocketParameter.tConfigure.bButtonRelayEnable);

    return 0;
}

/******************************************************************************
 * FunctionName : manual_enable_set
 * Description  : enable or disable button control relay
 * Parameters   : pcjson -- A pointer to a JSON formatted string
 * Returns      : result
 {"set":{"enable":1 }}
*******************************************************************************/
LOCAL int
manual_enable_set(const char *pValue)
{
    cJSON * pJsonSub=NULL;
    cJSON * pJsonSub_enable=NULL;

    cJSON * pJson = cJSON_Parse(pValue);

    if(NULL == pJson){
        printf("pJson create fail\n");
        return (-1);
    }

    pJsonSub = cJSON_GetObjectItem(pJson, "set");
    if(NULL == pJsonSub){
    	printf("cJSON_GetObjectItem fail\n");
    	cJSON_Delete(pJson);
    	return (-1);
    }

    pJsonSub_enable = cJSON_GetObjectItem(pJsonSub, "enable");
    if(NULL == pJsonSub_enable){
    	printf("pJsonSub_enable fail\n");
    	cJSON_Delete(pJson);
    	return (-1);
    }
	if(pJsonSub_enable->type == cJSON_Number){
		tSmartSocketParameter.tConfigure.bButtonRelayEnable = pJsonSub_enable->valueint;
		cJSON_Delete(pJson);

		if(xSemaphoreTake(xSmartSocketParameterSemaphore, (portTickType)10) == pdTRUE ){
			system_param_save_with_protect(GET_USER_DATA_SECTORE(USER_DATA_CONF_PARA),
								&tSmartSocketParameter, sizeof(tSmartSocketParameter));
			xSemaphoreGive(xSmartSocketParameterSemaphore);
		}else{
			printf("Take parameter semaphore failed.\n");
			return (-1);
		}
		return 0;
	}else{
	    cJSON_Delete(pJson);
	    printf("switch_status_set fail\n");
	    return (-1);
	}
}

/******************************************************************************
 * FunctionName : debug_smart_config
 * Description  : re-start smart config, only used for debug
 * Parameters   : pcjson -- A pointer to a JSON formatted string
 * Returns      : result
 {"set":{"enable":1 }}
*******************************************************************************/
LOCAL int
debug_smart_config(const char *pValue)
{
    cJSON * pJsonSub=NULL;
    cJSON * pJsonSub_enable=NULL;

    cJSON * pJson = cJSON_Parse(pValue);

    if(NULL != pJson){
        pJsonSub = cJSON_GetObjectItem(pJson, "set");
    }

    if(NULL != pJsonSub){
        pJsonSub_enable = cJSON_GetObjectItem(pJsonSub, "enable");
    }

    if(NULL != pJsonSub_enable){
        if(pJsonSub_enable->type == cJSON_Number){
        	PLTFM_startSmartConfig();
            if(NULL != pJson){
            	cJSON_Delete(pJson);
            }
            return 0;
        }
    }

    if(NULL != pJson)cJSON_Delete(pJson);
    printf("debug_smart_config fail\n");
    return (-1);
}
/******************************************************************************
 * FunctionName : switch_status_get
 * Description  : get relay status
 * Parameters   : pcjson -- A pointer to a JSON object
 * Returns      : result
{"response":{
"status":0}} 
*******************************************************************************/
LOCAL int  
switch_status_get(cJSON *pcjson, const char* pname )
{
    cJSON * pSubJson_response = cJSON_CreateObject();
    if(NULL == pSubJson_response){
        printf("pSubJson_response creat fail\n");
        return (-1);
    }
    cJSON_AddItemToObject(pcjson, "response", pSubJson_response);
    
    cJSON_AddNumberToObject(pSubJson_response, "status", user_plug_get_status());

    return 0;
}

/******************************************************************************
 * FunctionName : switch_status_set
 * Description  : set relay to close or open
 * Parameters   : pcjson -- A pointer to a JSON formatted string
 * Returns      : result
 {"Response":
 {"status":1 }}
*******************************************************************************/
LOCAL int  
switch_status_set(const char *pValue)
{
    cJSON * pJsonSub=NULL;
    cJSON * pJsonSub_status=NULL;
    
    cJSON * pJson =  cJSON_Parse(pValue);

    if(NULL != pJson){
        pJsonSub = cJSON_GetObjectItem(pJson, "Response");
    }
    
    if(NULL != pJsonSub){
        pJsonSub_status = cJSON_GetObjectItem(pJsonSub, "status");
    }
    
    if(NULL != pJsonSub_status){
        if(pJsonSub_status->type == cJSON_Number){
            user_plug_set_status(pJsonSub_status->valueint);
            if(NULL != pJson)cJSON_Delete(pJson);
            return 0;
        }
    }
    
    if(NULL != pJson)cJSON_Delete(pJson);
    printf("switch_status_set fail\n");
    return (-1);
}


LOCAL int  
user_set_reboot(const char *pValue)
{
    printf("user_set_reboot %s \n", pValue);
    system_restart();
    return 0;
}

LOCAL int  
system_status_reset(const char *pValue)
{
    printf("system_status_reset %s \n", pValue);
    DAT_nFlashDataClean();
    return 0;
}

/******************************************************************************
 * FunctionName : user_upgrade_start
 * Description  : set and start FW upgrade
 * Parameters   : pcjson -- A pointer to a JSON formated string
 * Returns      : result
{
"set":{"upgrade_server":"120.41.31.19",
		"upgrade_host":"iot.zulolo.cn",
		"upgrade_port":80,
		"upgrade_token":"123456789ABCDEF...",
		"upgrade_url":"/Smart_plug_upgrade/fw_download/version/filename"}
}
*******************************************************************************/
LOCAL int
user_upgrade_start(const char *pValue)
{
	cJSON * pJson;
	cJSON * pJsonSub;
	cJSON * pJsonSubSet;

    pJson = cJSON_Parse(pValue);
    if(NULL == pJson){
        printf("current_threshold_set cJSON_Parse fail\n");
        return (-1);
    }

    pJsonSubSet = cJSON_GetObjectItem(pJson, "set");
    if(pJsonSubSet == NULL) {
        printf("cJSON_GetObjectItem set fail\n");
        cJSON_Delete(pJson);
        return (-1);
    }

    if((pJsonSub = cJSON_GetObjectItem(pJsonSubSet, "upgrade_server")) == NULL){
        printf("cJSON_GetObjectItem upgrade_server fail\n");
        cJSON_Delete(pJson);
        return (-1);
    }
    strncpy(tSmartSocketParameter.sFW_UpgradeServer, pJsonSub->valuestring, sizeof(tSmartSocketParameter.sFW_UpgradeServer));
    tSmartSocketParameter.sFW_UpgradeServer[sizeof(tSmartSocketParameter.sFW_UpgradeServer) - 1] = '\0';

    if((pJsonSub = cJSON_GetObjectItem(pJsonSubSet, "upgrade_host")) == NULL){
        printf("cJSON_GetObjectItem upgrade_host fail\n");
        cJSON_Delete(pJson);
        return (-1);
    }
    strncpy(tSmartSocketParameter.sFW_UpgradeHost, pJsonSub->valuestring, sizeof(tSmartSocketParameter.sFW_UpgradeHost));
    tSmartSocketParameter.sFW_UpgradeHost[sizeof(tSmartSocketParameter.sFW_UpgradeHost) - 1] = '\0';

    if((pJsonSub = cJSON_GetObjectItem(pJsonSubSet, "upgrade_token")) == NULL){
        printf("cJSON_GetObjectItem update_token fail\n");
        cJSON_Delete(pJson);
        return (-1);
    }
    memcpy(tSmartSocketParameter.sFW_UpgradeToken, pJsonSub->valuestring, UPGRADE_TOKEN_LENGTH);
    tSmartSocketParameter.sFW_UpgradeToken[UPGRADE_TOKEN_LENGTH] = '\0';

    if((pJsonSub = cJSON_GetObjectItem(pJsonSubSet, "upgrade_url")) == NULL){
        printf("cJSON_GetObjectItem upgrade_url fail\n");
        cJSON_Delete(pJson);
        return (-1);
    }
    memcpy(tSmartSocketParameter.sFW_UpgradeUrl, pJsonSub->valuestring, UPGRADE_URL_LENGTH);
    tSmartSocketParameter.sFW_UpgradeUrl[UPGRADE_URL_LENGTH] = '\0';

    if((pJsonSub = cJSON_GetObjectItem(pJsonSubSet, "upgrade_port")) != NULL){
        if ((pJsonSub->valueint > 0) && (pJsonSub->valueint <= MAX_TCP_PORT)){
        	tSmartSocketParameter.unFW_UpgradePort = pJsonSub->valueint;
        }
    }

    user_esp_platform_upgrade_begin();
    return 0;
}

LOCAL int
user_upgrade_reset(const char *pValue)
{
    printf("user_upgrade_reset %s \n", pValue);

    return 0;
}

/******************************************************************************
 * FunctionName : restart_10ms_cb
 * Description  : system restart or wifi reconnected after a certain time.
 * Parameters   : arg -- Additional argument to pass to the function
 * Returns      : none
*******************************************************************************/
LOCAL void  
restart_xms_cb(void *arg)
{
    if (rstparm != NULL) {
        switch (rstparm->parmtype) {
            case WIFI:
                if (sta_conf->ssid[0] != 0x00) {
                    printf("restart_xms_cb set sta_conf noreboot\n");
                    wifi_station_set_config(sta_conf);
                    wifi_station_disconnect();
                    wifi_station_connect();
                }

                if (ap_conf->ssid[0] != 0x00) {
                    wifi_softap_set_config(ap_conf);
                    printf("restart_xms_cb set ap_conf sys restart\n");
                    system_restart();
                }

                free(ap_conf);
                ap_conf = NULL;
                free(sta_conf);
                sta_conf = NULL;
                free(rstparm);
                rstparm = NULL;
                free(restart_xms);
                restart_xms = NULL;

                break;

            case DEEP_SLEEP:
            case REBOOT:
                break;

            default:
                break;
        }
    }
}

/******************************************************************************
 * FunctionName : wifi_station_get
 * Description  : set up the station paramer as a JSON format
 * Parameters   : pcjson -- A pointer to a JSON object
 * Returns      : result
*******************************************************************************/
LOCAL int  
wifi_station_get(cJSON *pcjson)
{
    struct ip_info ipconfig;
    uint8 buff[20];
    bzero(buff, sizeof(buff));

    cJSON * pSubJson_Connect_Station= cJSON_CreateObject();
    if(NULL == pSubJson_Connect_Station){
        printf("pSubJson_Connect_Station creat fail\n");
        return -1;
    }
    cJSON_AddItemToObject(pcjson, "Connect_Station", pSubJson_Connect_Station);

    cJSON * pSubJson_Ipinfo_Station= cJSON_CreateObject();
    if(NULL == pSubJson_Ipinfo_Station){
        printf("pSubJson_Ipinfo_Station creat fail\n");
        return -1;
    }
    cJSON_AddItemToObject(pcjson, "Ipinfo_Station", pSubJson_Ipinfo_Station);

    wifi_station_get_config(sta_conf);
    wifi_get_ip_info(STATION_IF, &ipconfig);

    sprintf(buff, IPSTR, IP2STR(&ipconfig.ip));
    cJSON_AddStringToObject(pSubJson_Ipinfo_Station,"ip",buff);
    sprintf(buff, IPSTR, IP2STR(&ipconfig.netmask));
    cJSON_AddStringToObject(pSubJson_Ipinfo_Station,"mask",buff);
    sprintf(buff, IPSTR, IP2STR(&ipconfig.gw));
    cJSON_AddStringToObject(pSubJson_Ipinfo_Station,"gw",buff);
    cJSON_AddStringToObject(pSubJson_Connect_Station, "ssid", sta_conf->ssid);
    cJSON_AddStringToObject(pSubJson_Connect_Station, "password", sta_conf->password);

    return 0;
}

/******************************************************************************
 * FunctionName : wifi_softap_get
 * Description  : set up the softap paramer as a JSON format
 * Parameters   : pcjson -- A pointer to a JSON object
 * Returns      : result
*******************************************************************************/
LOCAL int  
wifi_softap_get(cJSON *pcjson)
{
    struct ip_info ipconfig;
    uint8 buff[20];
    bzero(buff, sizeof(buff));
    
    cJSON * pSubJson_Connect_Softap= cJSON_CreateObject();
    if(NULL == pSubJson_Connect_Softap){
        printf("pSubJson_Connect_Softap creat fail\n");
        return -1;
    }
    cJSON_AddItemToObject(pcjson, "Connect_Softap", pSubJson_Connect_Softap);

    cJSON * pSubJson_Ipinfo_Softap= cJSON_CreateObject();
    if(NULL == pSubJson_Ipinfo_Softap){
        printf("pSubJson_Ipinfo_Softap creat fail\n");
        return -1;
    }
    cJSON_AddItemToObject(pcjson, "Ipinfo_Softap", pSubJson_Ipinfo_Softap);
    wifi_softap_get_config(ap_conf);
    wifi_get_ip_info(SOFTAP_IF, &ipconfig);

    sprintf(buff, IPSTR, IP2STR(&ipconfig.ip));
    cJSON_AddStringToObject(pSubJson_Ipinfo_Softap,"ip",buff);
    sprintf(buff, IPSTR, IP2STR(&ipconfig.netmask));
    cJSON_AddStringToObject(pSubJson_Ipinfo_Softap,"mask",buff);
    sprintf(buff, IPSTR, IP2STR(&ipconfig.gw));
    cJSON_AddStringToObject(pSubJson_Ipinfo_Softap,"gw",buff);
    cJSON_AddNumberToObject(pSubJson_Connect_Softap, "channel", ap_conf->channel);
    cJSON_AddStringToObject(pSubJson_Connect_Softap, "ssid", ap_conf->ssid);
    cJSON_AddStringToObject(pSubJson_Connect_Softap, "password", ap_conf->password);
    
    switch (ap_conf->authmode) {
        case AUTH_OPEN:
            cJSON_AddStringToObject(pSubJson_Connect_Softap, "authmode", "OPEN");
            break;
        case AUTH_WEP:
            cJSON_AddStringToObject(pSubJson_Connect_Softap, "authmode", "WEP");
            break;
        case AUTH_WPA_PSK:
            cJSON_AddStringToObject(pSubJson_Connect_Softap, "authmode", "WPAPSK");
            break;
        case AUTH_WPA2_PSK:
            cJSON_AddStringToObject(pSubJson_Connect_Softap, "authmode", "WPA2PSK");
            break;
        case AUTH_WPA_WPA2_PSK:
            cJSON_AddStringToObject(pSubJson_Connect_Softap, "authmode", "WPAPSK/WPA2PSK");
            break;
        default :
            cJSON_AddNumberToObject(pSubJson_Connect_Softap, "authmode",  ap_conf->authmode);
            break;
    }
    return 0;
}

/******************************************************************************
 * FunctionName : update_url_get
 * Description  : get update FW URL
 * Parameters   : pcjson -- A pointer to a JSON object
 * Returns      : result
{
"Response":{"upgrade_url":"192.168.31.157",
			"upgrade_port":80}
}
*******************************************************************************/
LOCAL int
update_url_get(cJSON *pcjson, const char* pname)
{
    cJSON * pSubJsonResponse;

    pSubJsonResponse = cJSON_CreateObject();
    if(NULL == pSubJsonResponse){
        printf("pSubJsonResponse create fail\n");
        return (-1);
    }
    cJSON_AddItemToObject(pcjson, "Response", pSubJsonResponse);
    cJSON_AddStringToObject(pSubJsonResponse, "upgrade_url", tSmartSocketParameter.sFW_UpgradeServer);
    cJSON_AddNumberToObject(pSubJsonResponse, "upgrade_port", tSmartSocketParameter.unFW_UpgradePort);

	return 0;
}

/******************************************************************************
 * FunctionName : update_url_get
 * Description  : get update FW URL
 * Parameters   : pcjson -- A pointer to a JSON object
 * Returns      : result
{
"Response":{"upgrade_progress":0.85,
			"upgrade_status":1}
}
  * #define UPGRADE_FLAG_IDLE      0x00
  * #define UPGRADE_FLAG_START     0x01
  * #define UPGRADE_FLAG_FINISH    0x02
*******************************************************************************/
LOCAL int
upgrade_status_get(cJSON *pcjson, const char* pname)
{
    cJSON * pSubJsonResponse;

    pSubJsonResponse = cJSON_CreateObject();
    if(NULL == pSubJsonResponse){
        printf("pSubJsonResponse create fail\n");
        return (-1);
    }
    cJSON_AddItemToObject(pcjson, "Response", pSubJsonResponse);
    cJSON_AddNumberToObject(pSubJsonResponse, "upgrade_progress", system_upgrade_get_progress());
    cJSON_AddNumberToObject(pSubJsonResponse, "upgrade_status", system_upgrade_flag_check());

	return 0;
}

/******************************************************************************
 * FunctionName : current_threshold_set
 * Description  : set current threshold
 * Parameters   : pcjson -- A pointer to a JSON formated string
 * Returns      : result
 * {"set":{"current_threshold":10}}
*******************************************************************************/
LOCAL int
current_threshold_set(const char* pValue)
{
	cJSON * pJson;
	cJSON * pJsonSub;
	cJSON * pJsonSubSet;

    pJson = cJSON_Parse(pValue);
    if(NULL == pJson){
        printf("current_threshold_set cJSON_Parse fail\n");
        return (-1);
    }

    pJsonSubSet = cJSON_GetObjectItem(pJson, "set");
    if(pJsonSubSet == NULL) {
        printf("cJSON_GetObjectItem set fail\n");
        cJSON_Delete(pJson);
        return (-1);
    }

    if((pJsonSub = cJSON_GetObjectItem(pJsonSubSet, "current_threshold")) == NULL){
        printf("cJSON_GetObjectItem current_threshold fail\n");
        cJSON_Delete(pJson);
        return (-1);
    }
    if ((pJsonSub->valuedouble < MIN_CURRENT_THRESHOLD) || (pJsonSub->valuedouble > MAX_CURRENT_THRESHOLD)){
        printf("Threshold value error.\n");
        cJSON_Delete(pJson);
        return (-1);
    }
    tSmartSocketParameter.fCurrentThreshold = pJsonSub->valuedouble;

    cJSON_Delete(pJson);
	if(xSemaphoreTake(xSmartSocketParameterSemaphore, (portTickType)10) == pdTRUE){
		system_param_save_with_protect(GET_USER_DATA_SECTORE(USER_DATA_CONF_PARA),
							&tSmartSocketParameter, sizeof(tSmartSocketParameter));
		xSemaphoreGive(xSmartSocketParameterSemaphore);
	}else{
		printf("Take parameter semaphore failed.\n");
		return (-1);
	}
	return 0;
}

/******************************************************************************
 * FunctionName : current_threshold_get
 * Description  : get current threshold configuration
 * Parameters   : pcjson -- A pointer to a JSON object
 * Returns      : result
{
"Response":{"current_threshold":10}
}
*******************************************************************************/
LOCAL int
current_threshold_get(cJSON *pcjson, const char* pname)
{
    cJSON * pSubJsonResponse;

    pSubJsonResponse = cJSON_CreateObject();
    if(NULL == pSubJsonResponse){
        printf("pSubJsonResponse create fail\n");
        return (-1);
    }
    cJSON_AddItemToObject(pcjson, "Response", pSubJsonResponse);
    cJSON_AddNumberToObject(pSubJsonResponse, "current_threshold", tSmartSocketParameter.fCurrentThreshold);

	return 0;
}

/******************************************************************************
 * FunctionName : sntp_set
 * Description  : set SNTP server URL address
 * Parameters   : pcjson -- A pointer to a JSON formated string
 * Returns      : result
 * {"set":{"sntp":{"index":0,"url":"cn.pool.ntp.org"}}}
 * {"set":{"enable":1}}
*******************************************************************************/
LOCAL int
sntp_set(const char* pValue)
{
	cJSON * pJson;
	cJSON * pJsonSub;
	cJSON * pJsonSubSet;
	cJSON * pJsonSubIndex;
	cJSON * pJsonSubURL;
	uint8_t unIndex;

    pJson = cJSON_Parse(pValue);
    if(NULL == pJson){
        printf("sntp_set cJSON_Parse fail\n");
        return (-1);
    }

    pJsonSubSet = cJSON_GetObjectItem(pJson, "set");
    if(pJsonSubSet == NULL) {
        printf("cJSON_GetObjectItem set fail\n");
        cJSON_Delete(pJson);
        return (-1);
    }

    if((pJsonSub = cJSON_GetObjectItem(pJsonSubSet, "sntp")) != NULL){
        pJsonSubIndex= cJSON_GetObjectItem(pJsonSub, "index");
        if(NULL == pJsonSubIndex){
            printf("cJSON_GetObjectItem sntp fail.\n");
            cJSON_Delete(pJson);
            return (-1);
        }else{
        	if ((pJsonSubIndex->valueint < 0) || (pJsonSubIndex->valueint >= MAX_SNTP_SERVER_NUM)){
                printf("SNTP index error.\n");
                cJSON_Delete(pJson);
                return (-1);
        	}else{
        		unIndex = pJsonSubIndex->valueint;
        	}
        }

        pJsonSubURL = cJSON_GetObjectItem(pJsonSub,"url");
        if(NULL == pJsonSubURL){
			printf("cJSON_GetObjectItem pJsonSubURL fail\n");
			cJSON_Delete(pJson);
			return (-1);
		}else{
        	if (strlen(pJsonSubURL->valuestring) >= MAX_SNTP_SERVER_ADDR_LEN){
                printf("URL length error.\n");
                cJSON_Delete(pJson);
                return (-1);
        	}else{
        		strcpy(tSmartSocketParameter.sSNTP_Server[unIndex], pJsonSubURL->valuestring);
        	}
        }

    	cJSON_Delete(pJson);
    	if(xSemaphoreTake(xSmartSocketParameterSemaphore, (portTickType)10) == pdTRUE ){
    		system_param_save_with_protect(GET_USER_DATA_SECTORE(USER_DATA_CONF_PARA),
    							&tSmartSocketParameter, sizeof(tSmartSocketParameter));
    		xSemaphoreGive(xSmartSocketParameterSemaphore);
    		return 0;
    	}else{
    		printf("Take parameter semaphore failed.\n");
    		return (-1);
    	}
    }

    if((pJsonSub = cJSON_GetObjectItem(pJsonSubSet, "enable")) != NULL){
		if ((pJsonSub->valueint != 0) && (pJsonSub->valueint != 1)){
            printf("SNTP enable value error\n");
            cJSON_Delete(pJson);
            return (-1);
    	}else{
        	tSmartSocketParameter.tConfigure.bSNTPEnable = pJsonSub->valueint;
        	cJSON_Delete(pJson);
        	if(xSemaphoreTake(xSmartSocketParameterSemaphore, (portTickType)10) == pdTRUE ){
        		system_param_save_with_protect(GET_USER_DATA_SECTORE(USER_DATA_CONF_PARA),
        							&tSmartSocketParameter, sizeof(tSmartSocketParameter));
        		xSemaphoreGive(xSmartSocketParameterSemaphore);
        		return 0;
        	}else{
        		printf("Take parameter semaphore failed.\n");
        		return (-1);
        	}
    	}
    }
    printf("Unrecognized SNTP configuration.\n");
    cJSON_Delete(pJson);
	return (-1);
}

/******************************************************************************
 * FunctionName : sntp_get
 * Description  : get sntp configuration
 * Parameters   : pcjson -- A pointer to a JSON object
 * Returns      : result
{
"Response":{"enable":1,
			"server_0":"cn.pool.ntp.org",
			"server_1":"asia.pool.ntp.org",
			"server_2":"pool.ntp.org"}
}
*******************************************************************************/
LOCAL int
sntp_get(cJSON *pcjson, const char* pname)
{
	uint8_t unIndex;
	char cServerName[16];
    cJSON * pSubJsonResponse;

    pSubJsonResponse = cJSON_CreateObject();
    if(NULL == pSubJsonResponse){
        printf("pSubJsonResponse create fail\n");
        return (-1);
    }
    cJSON_AddItemToObject(pcjson, "Response", pSubJsonResponse);
    cJSON_AddNumberToObject(pSubJsonResponse, "enable", tSmartSocketParameter.tConfigure.bSNTPEnable);

    for (unIndex = 0; unIndex < MAX_SNTP_SERVER_NUM; unIndex++){
    	sprintf(cServerName, "server_%u", unIndex);
        cJSON_AddStringToObject(pSubJsonResponse, cServerName, tSmartSocketParameter.sSNTP_Server[unIndex]);
    }

	return 0;
}

/******************************************************************************
 * FunctionName : relay_schedule_set
 * Description  : set relay schedule
 * Parameters   : pcjson -- A pointer to a JSON formated string
 * Returns      : result
 * {"set":{"schedule":{"index":0,"close_time":3248,"open_time":3456}}}
 * {"set":{"enable":1}}
*******************************************************************************/
LOCAL int
relay_schedule_set(const char* pValue)
{
	cJSON * pJson;
	cJSON * pJsonSub;
	cJSON * pJsonSubSet;
	cJSON * pJsonSubIndex;
	cJSON * pJsonSubCloseTime;
	cJSON * pJsonSubOpenTime;
	uint8_t unIndex;
	uint32_t unCloseTime;
	uint32_t unOpenTime;

    pJson = cJSON_Parse(pValue);
    if(NULL == pJson){
        printf("relay_schedule_set cJSON_Parse fail\n");
        return (-1);
    }

    pJsonSubSet = cJSON_GetObjectItem(pJson, "set");
    if(pJsonSubSet == NULL) {
        printf("cJSON_GetObjectItem set fail\n");
        cJSON_Delete(pJson);
        return (-1);
    }

    if((pJsonSub = cJSON_GetObjectItem(pJsonSubSet, "schedule")) != NULL){
        pJsonSubIndex= cJSON_GetObjectItem(pJsonSub, "index");
        if(NULL == pJsonSubIndex){
            printf("cJSON_GetObjectItem schedule fail.\n");
            cJSON_Delete(pJson);
            return (-1);
        }else{
        	if ((pJsonSubIndex->valueint < 0) || (pJsonSubIndex->valueint >= RELAY_SCHEDULE_NUM)){
                printf("Schedule index error.\n");
                cJSON_Delete(pJson);
                return (-1);
        	}else{
        		unIndex = pJsonSubIndex->valueint;
        	}
        }

        pJsonSubCloseTime = cJSON_GetObjectItem(pJsonSub,"close_time");
        if(NULL == pJsonSubCloseTime){
			printf("cJSON_GetObjectItem pJsonSubCloseTime fail\n");
			cJSON_Delete(pJson);
			return (-1);
		}else{
        	if ((pJsonSubCloseTime->valueint < 0) || (pJsonSubCloseTime->valueint >= RELAY_SCHEDULE_MAX_SEC_DAY)){
                printf("Schedule close time error.\n");
                cJSON_Delete(pJson);
                return (-1);
        	}else{
        		unCloseTime = pJsonSubCloseTime->valueint;
        	}
        }

        pJsonSubOpenTime = cJSON_GetObjectItem(pJsonSub,"open_time");
        if(NULL == pJsonSubOpenTime){
			printf("cJSON_GetObjectItem pJsonSubOpenTime fail\n");
			cJSON_Delete(pJson);
			return (-1);
		}else{
        	if ((pJsonSubOpenTime->valueint < 0) || (pJsonSubOpenTime->valueint >= RELAY_SCHEDULE_MAX_SEC_DAY) ||
        			(pJsonSubOpenTime->valueint <= (unCloseTime + RELAY_SCHEDULE_MIN_CLOSE_TIME))){
                printf("Schedule open time error.\n");
                cJSON_Delete(pJson);
                return (-1);
        	}else{
        		unOpenTime = pJsonSubOpenTime->valueint;
        	}
        }
        if (user_plug_relay_schedule_validation(unIndex, unCloseTime, unOpenTime) == true){
        	tSmartSocketParameter.tRelaySchedule[unIndex].unRelayCloseTime = unCloseTime;
        	tSmartSocketParameter.tRelaySchedule[unIndex].unRelayOpenTime = unOpenTime;
        }

    	cJSON_Delete(pJson);
    	if(xSemaphoreTake(xSmartSocketParameterSemaphore, (portTickType)10) == pdTRUE ){
    		system_param_save_with_protect(GET_USER_DATA_SECTORE(USER_DATA_CONF_PARA),
    							&tSmartSocketParameter, sizeof(tSmartSocketParameter));
    		xSemaphoreGive(xSmartSocketParameterSemaphore);
    		return 0;
    	}else{
    		printf("Take parameter semaphore failed.\n");
    		return (-1);
    	}
    }

    if((pJsonSub = cJSON_GetObjectItem(pJsonSubSet, "enable")) != NULL){
		if ((pJsonSub->valueint != 0) && (pJsonSub->valueint != 1)){
            printf("Schedule enable value error\n");
            cJSON_Delete(pJson);
            return (-1);
    	}else{
        	tSmartSocketParameter.tConfigure.bRelayScheduleEnable = pJsonSub->valueint;
        	cJSON_Delete(pJson);
        	if(xSemaphoreTake(xSmartSocketParameterSemaphore, (portTickType)10) == pdTRUE ){
        		system_param_save_with_protect(GET_USER_DATA_SECTORE(USER_DATA_CONF_PARA),
        							&tSmartSocketParameter, sizeof(tSmartSocketParameter));
        		xSemaphoreGive(xSmartSocketParameterSemaphore);
        		return 0;
        	}else{
        		printf("Take parameter semaphore failed.\n");
        		return (-1);
        	}
    	}
    }
    printf("Unrecognized relay schedule.\n");
    cJSON_Delete(pJson);
	return (-1);
}

/******************************************************************************
 * FunctionName : relay_schedule_get
 * Description  : get relay schedule
 * Parameters   : pcjson -- A pointer to a JSON object
 * Returns      : result
{
"Response":{"enable":1,
			"schedule":{"close_time":3243,"open_time":3248},
			"schedule":{"close_time":4243,"open_time":4248},
			"schedule":{"close_time":5243,"open_time":5248},
			"schedule":{"close_time":6243,"open_time":6248}}
}
*******************************************************************************/
LOCAL int
relay_schedule_get(cJSON *pcjson, const char* pname)
{
	uint8_t unIndex;
    cJSON * pSubJsonResponse;
	cJSON * pSubJsonschedule;

    pSubJsonResponse = cJSON_CreateObject();
    if(NULL == pSubJsonResponse){
        printf("pSubJsonResponse create fail\n");
        return (-1);
    }
    cJSON_AddItemToObject(pcjson, "Response", pSubJsonResponse);
    cJSON_AddNumberToObject(pSubJsonResponse, "enable", tSmartSocketParameter.tConfigure.bRelayScheduleEnable);

    for (unIndex = 0; unIndex < RELAY_SCHEDULE_NUM; unIndex++){
        pSubJsonschedule= cJSON_CreateObject();
        if(NULL == pSubJsonschedule){
            printf("pSubJsonEnable create fail\n");
            return (-1);
        }
        cJSON_AddItemToObject(pSubJsonResponse, "schedule", pSubJsonschedule);
        cJSON_AddNumberToObject(pSubJsonschedule, "close_time", tSmartSocketParameter.tRelaySchedule[unIndex].unRelayCloseTime);
        cJSON_AddNumberToObject(pSubJsonschedule, "open_time", tSmartSocketParameter.tRelaySchedule[unIndex].unRelayOpenTime);
    }

	return 0;
}

/******************************************************************************
 * FunctionName : wifi_softap_get
 * Description  : set up the softap paramer as a JSON format
 * Parameters   : pcjson -- A pointer to a JSON object
 * Returns      : result
{
"Response":{"Station":
{"Connect_Station":{"ssid":"","password":""},
"Ipinfo_Station":{"ip":"0.0.0.0","mask":"0.0.0.0","gw":"0.0.0.0"}},
"Softap":
{"Connect_Softap":{"authmode":"OPEN","channel":1,"ssid":"ESP_A132F0","password":""},
"Ipinfo_Softap":{"ip":"192.168.4.1","mask":"255.255.255.0","gw":"192.168.4.1"}}
}}
*******************************************************************************/
LOCAL int  
wifi_info_get(cJSON *pcjson, const char* pname)
{

    ap_conf = (struct softap_config *)zalloc(sizeof(struct softap_config));
    sta_conf = (struct station_config *)zalloc(sizeof(struct station_config));

    cJSON * pSubJson_Response= cJSON_CreateObject();
    if(NULL == pSubJson_Response){
        printf("pSubJson_Response creat fail\n");
        return -1;
    }
    cJSON_AddItemToObject(pcjson, "Response", pSubJson_Response);

    cJSON * pSubJson_Station= cJSON_CreateObject();
    if(NULL == pSubJson_Station){
        printf("pSubJson_Station creat fail\n");
        return -1;
    }
    cJSON_AddItemToObject(pSubJson_Response, "Station", pSubJson_Station);
    
    cJSON * pSubJson_Softap= cJSON_CreateObject();
    if(NULL == pSubJson_Softap){
        printf("pSubJson_Softap creat fail\n");
        return -1;
    }
    cJSON_AddItemToObject(pSubJson_Response, "Softap", pSubJson_Softap);

    if((wifi_station_get(pSubJson_Station) == -1)||(wifi_softap_get(pSubJson_Softap) == -1)){

        free(sta_conf);
        free(ap_conf);
        sta_conf = NULL;
        ap_conf = NULL;
        return -1;
    } else{
        free(sta_conf);
        free(ap_conf);
        sta_conf = NULL;
        ap_conf = NULL;
        return 0;
    }

}

/******************************************************************************
 * FunctionName : wifi_station_set
 * Description  : parse the station parmer as a JSON format
 * Parameters   : pcjson -- A pointer to a JSON formated string
 * Returns      : result
 * {"Request":{"Station":{"Connect_Station":{"ssid":"","password":"","token":""}}}}
 * {"Request":{"Softap":{"Connect_Softap":
   {"authmode":"OPEN", "channel":6, "ssid":"IOT_SOFTAP","password":""}}}}
 * {"Request":{"Station":{"Connect_Station":{"token":"u6juyl9t6k4qdplgl7dg7m90x96264xrzse6mx1i"}}}}
*******************************************************************************/
LOCAL int  
wifi_info_set(const char* pValue)
{
    cJSON * pJson;
    cJSON * pJsonSub;
    cJSON * pJsonSub_request;
    cJSON * pJsonSub_Connect_Station;
    cJSON * pJsonSub_Connect_Softap;
    cJSON * pJsonSub_Sub;

    user_esp_platform_set_connect_status(DEVICE_CONNECTING);
    
    if (restart_xms != NULL) {
        os_timer_disarm(restart_xms);
    }
    
    if (ap_conf == NULL) {
        ap_conf = (struct softap_config *)zalloc(sizeof(struct softap_config));
    }
    
    if (sta_conf == NULL) {
        sta_conf = (struct station_config *)zalloc(sizeof(struct station_config));
    }

    pJson = cJSON_Parse(pValue);
    if(NULL == pJson){
        printf("wifi_info_set cJSON_Parse fail\n");
        return -1;
    }
    
    pJsonSub_request = cJSON_GetObjectItem(pJson, "Request");
    if(pJsonSub_request == NULL) {
        printf("cJSON_GetObjectItem pJsonSub_request fail\n");
        return -1;
    }

    if((pJsonSub = cJSON_GetObjectItem(pJsonSub_request, "Station")) != NULL){

        pJsonSub_Connect_Station= cJSON_GetObjectItem(pJsonSub,"Connect_Station");
        if(NULL == pJsonSub_Connect_Station){
            printf("cJSON_GetObjectItem Connect_Station fail\n");
            return -1;
        }

        pJsonSub_Sub = cJSON_GetObjectItem(pJsonSub_Connect_Station,"ssid");
        if(NULL != pJsonSub_Sub){       
            if( strlen(pJsonSub_Sub->valuestring)<=32 )
                memcpy(sta_conf->ssid, pJsonSub_Sub->valuestring, strlen(pJsonSub_Sub->valuestring));
            else
                 os_printf("ERR:arr_overflow,%u,%d\n",__LINE__,strlen(pJsonSub_Sub->valuestring));
        }

        pJsonSub_Sub = cJSON_GetObjectItem(pJsonSub_Connect_Station,"password");
        if(NULL != pJsonSub_Sub){
            if( strlen(pJsonSub_Sub->valuestring)<=64 )
                memcpy(sta_conf->password, pJsonSub_Sub->valuestring, strlen(pJsonSub_Sub->valuestring));
            else
                 os_printf("ERR:arr_overflow,%u,%d\n",__LINE__,strlen(pJsonSub_Sub->valuestring));
        }

#if ESP_PLATFORM
        pJsonSub_Sub = cJSON_GetObjectItem(pJsonSub_Connect_Station,"token");
        if(NULL != pJsonSub_Sub){       
            user_esp_platform_set_token(pJsonSub_Sub->valuestring);
        }
#endif
    }

    if((pJsonSub = cJSON_GetObjectItem(pJsonSub_request, "Softap")) != NULL){
        pJsonSub_Connect_Softap= cJSON_GetObjectItem(pJsonSub,"Connect_Softap");
        if(NULL == pJsonSub_Connect_Softap){
            printf("cJSON_GetObjectItem Connect_Softap fail!\n");
            return -1;
        }

        pJsonSub_Sub = cJSON_GetObjectItem(pJsonSub_Connect_Softap,"ssid");
        if(NULL != pJsonSub_Sub){
            /*
                printf("pJsonSub_Connect_Softap pJsonSub_Sub->ssid %s  len%d\n",
                pJsonSub_Sub->valuestring, strlen(pJsonSub_Sub->valuestring));
            */
            if( strlen(pJsonSub_Sub->valuestring)<=32 )
                memcpy(ap_conf->ssid, pJsonSub_Sub->valuestring, strlen(pJsonSub_Sub->valuestring));
            else
                 os_printf("ERR:arr_overflow,%u,%d\n",__LINE__,strlen(pJsonSub_Sub->valuestring));
        }

        pJsonSub_Sub = cJSON_GetObjectItem(pJsonSub_Connect_Softap,"password");
        if(NULL != pJsonSub_Sub){
        /*
            printf("pJsonSub_Connect_Softap pJsonSub_Sub->password %s  len%d\n",
                pJsonSub_Sub->valuestring, strlen(pJsonSub_Sub->valuestring));
          */          
            if( strlen(pJsonSub_Sub->valuestring)<=64 )
                memcpy(ap_conf->password, pJsonSub_Sub->valuestring, strlen(pJsonSub_Sub->valuestring));
            else
                 os_printf("ERR:arr_overflow,%u,%d\n",__LINE__,strlen(pJsonSub_Sub->valuestring));
        }

        pJsonSub_Sub = cJSON_GetObjectItem(pJsonSub_Connect_Softap,"channel");
        if(NULL != pJsonSub_Sub){
            /*
            printf("pJsonSub_Connect_Softap channel %d\n",pJsonSub_Sub->valueint);
            */
            ap_conf->channel = pJsonSub_Sub->valueint;
        }

        pJsonSub_Sub = cJSON_GetObjectItem(pJsonSub_Connect_Softap,"authmode");
        if(NULL != pJsonSub_Sub){
            if (strcmp(pJsonSub_Sub->valuestring, "OPEN") == 0) {
                ap_conf->authmode = AUTH_OPEN;
            } else if (strcmp(pJsonSub_Sub->valuestring, "WPAPSK") == 0) {
                ap_conf->authmode = AUTH_WPA_PSK;
            } else if (strcmp(pJsonSub_Sub->valuestring, "WPA2PSK") == 0) {
                ap_conf->authmode = AUTH_WPA2_PSK;
            } else if (strcmp(pJsonSub_Sub->valuestring, "WPAPSK/WPA2PSK") == 0) {
                ap_conf->authmode = AUTH_WPA_WPA2_PSK;
            } else {
                ap_conf->authmode = AUTH_OPEN;
            }
            /*
            printf("pJsonSub_Connect_Softap ap_conf->authmode %d\n",ap_conf->authmode);
            */
        }

    }

    cJSON_Delete(pJson);

    if (rstparm == NULL) {
        rstparm = (rst_parm *)zalloc(sizeof(rst_parm));
    }
    rstparm->parmtype = WIFI;
    
    if (sta_conf->ssid[0] != 0x00 || ap_conf->ssid[0] != 0x00) {
        ap_conf->ssid_hidden = 0;
        ap_conf->max_connection = 4;
    
        if (restart_xms == NULL) {
            restart_xms = (os_timer_t *)malloc(sizeof(os_timer_t));
            if(NULL == restart_xms){
                printf("ERROR:wifi_info_set,memory exhausted, check it\n");
            }
        }

        os_timer_disarm(restart_xms);
        os_timer_setfn(restart_xms, (os_timer_func_t *)restart_xms_cb, NULL);
        os_timer_arm(restart_xms, 20, 0);  // delay 10ms, then do
    } else {
        free(ap_conf);
        free(sta_conf);
        free(rstparm);
        sta_conf = NULL;
        ap_conf = NULL;
        rstparm =NULL;
    }
    
    return 0;
}

/******************************************************************************
 * FunctionName : scan_result_output
 * Description  : set up the scan data as a JSON format
 * Parameters   : pcjson -- A pointer to a JSON object
                : total -- flag that send the total scanpage or not
 * Returns      : result
*******************************************************************************/
LOCAL int  
scan_result_output(cJSON *pcjson, bool total)
{
    int count=2;//default no more than 8 
    u8 buff[20];
    LOCAL struct bss_info *bss;
    cJSON * pSubJson_page;
    char *pchar;
    
    while((bss = STAILQ_FIRST(pscaninfo->pbss)) != NULL && count-- ){

        pSubJson_page= cJSON_CreateObject();
        if(NULL == pSubJson_page){
            printf("pSubJson_page creat fail\n");
            return -1;
        }
        cJSON_AddItemToArray(pcjson, pSubJson_page);//pcjson

        memset(buff, 0, sizeof(buff));
        sprintf(buff, MACSTR, MAC2STR(bss->bssid));
        cJSON_AddStringToObject(pSubJson_page, "bssid", buff);
        cJSON_AddStringToObject(pSubJson_page, "ssid", bss->ssid);
        cJSON_AddNumberToObject(pSubJson_page, "rssi", -(bss->rssi));
        cJSON_AddNumberToObject(pSubJson_page, "channel", bss->channel);
        switch (bss->authmode) {
            case AUTH_OPEN:
                cJSON_AddStringToObject(pSubJson_page, "authmode", "OPEN");
                break;
            case AUTH_WEP:
                cJSON_AddStringToObject(pSubJson_page, "authmode", "WEP");
                break;
            case AUTH_WPA_PSK:
                cJSON_AddStringToObject(pSubJson_page, "authmode", "WPAPSK");
                break;
            case AUTH_WPA2_PSK:
                cJSON_AddStringToObject(pSubJson_page, "authmode", "WPA2PSK");
                break;
            case AUTH_WPA_WPA2_PSK:
                cJSON_AddStringToObject(pSubJson_page, "authmode", "WPAPSK/WPA2PSK");
                break;
            default :
                cJSON_AddNumberToObject(pSubJson_page, "authmode", bss->authmode);
                break;
        }
        STAILQ_REMOVE_HEAD(pscaninfo->pbss, next);
        free(bss);
    }

    return 0;
}

/******************************************************************************
 * FunctionName : device_get
 * Description  : set up the device information parmer as a JSON format
 * Parameters   : pcjson -- A pointer to a JSON object
 * Returns      : result
{"Status":{
"status":3}}
*******************************************************************************/
LOCAL int  
connect_status_get(cJSON *pcjson, const char* pname )
{
    cJSON * pSubJson_Status = cJSON_CreateObject();
    if(NULL == pSubJson_Status){
        printf("pSubJson_Status creat fail\n");
        return (-1);
    }
    cJSON_AddItemToObject(pcjson, "Status", pSubJson_Status);

    cJSON_AddNumberToObject(pSubJson_Status, "status", user_esp_platform_get_connect_status());
    return 0;
}

/******************************************************************************
 * FunctionName : device_get
 * Description  : set up the device information parmer as a JSON format
 * Parameters   : pcjson -- A pointer to a JSON object
 * Returns      : result
{"response":{
"status":10010}}
*******************************************************************************/
LOCAL int
meter_status_get(cJSON *pcjson, const char* pname )
{
    cJSON * pSubJson_response = cJSON_CreateObject();

    if(NULL == pSubJson_response){
        printf("pSubJson_response create fail\n");
        return (-1);
    }

    cJSON_AddItemToObject(pcjson, "response", pSubJson_response);

    cJSON_AddNumberToObject(pSubJson_response, "status", CS5463_unGetStatus());

    return 0;
}

typedef int (* cgigetCallback)(cJSON *pcjson, const char* pchar);
typedef int (* cgisetCallback)(const char* pchar);

typedef struct {
    const char *file;
    const char *cmd;
    cgigetCallback  get;
    cgisetCallback  set;
} EspCgiApiEnt;

const EspCgiApiEnt espCgiApiNodes[]={

    {"config", "switch", switch_status_get, switch_status_set},
	{"config", "manual", manual_enable_get, manual_enable_set},
	{"config", "debug", NULL, debug_smart_config},
	{"config", "schedule", relay_schedule_get, relay_schedule_set},
	{"config", "threshold", current_threshold_get, current_threshold_set},
	{"config", "sntp", sntp_get, sntp_set},
	{"client", "updateurl", update_url_get, NULL},
	{"client", "rssi", signal_strength_get,NULL},
	{"client", "current", current_value_get,NULL},
	{"client", "voltage", voltage_value_get,NULL},
	{"client", "power", power_value_get,NULL},
	{"client", "temperature", temperature_value_get,NULL},
	{"client", "trend", trend_get,NULL},
	{"client", "event", event_history_get,NULL},
    {"config", "reboot", NULL,user_set_reboot},
    {"config", "wifi", wifi_info_get,wifi_info_set},
//    {"client", "scan",  scan_info_get, NULL},
    {"client", "status", connect_status_get, NULL},
	{"client", "meterstatus", meter_status_get, NULL},
    {"config", "reset", NULL,system_status_reset},
    {"client", "info",  system_info_get, NULL},
	{"upgrade", "getstatus", upgrade_status_get, NULL},
    {"upgrade", "getuser", user_binfo_get, NULL},
    {"upgrade", "start", NULL, user_upgrade_start},
    {"upgrade", "reset", NULL, user_upgrade_reset},
    {NULL, NULL, NULL}
};

int   cgiEspApi(HttpdConnData *connData) {
    char *file=&connData->url[1]; //skip initial slash
    int len, i;
    int ret=0;
    
    char *pchar = NULL;
    cJSON *pcjson = NULL;
    char *pbuf=(char*)zalloc(48);
    
    httpdStartResponse(connData, 200);
//  httpdHeader(connData, "Content-Type", "text/json");
    httpdHeader(connData, "Content-Type", "text/plain");
    httpdEndHeaders(connData);
    httpdFindArg(connData->getArgs, "command", pbuf, 48);

//    printf("File %s Command %s\n", file, pbuf);

    //Find the command/file combo in the espCgiApiNodes table
    i=0;
    while (espCgiApiNodes[i].cmd!=NULL) {
        if (strcmp(espCgiApiNodes[i].file, file)==0 && strcmp(espCgiApiNodes[i].cmd, pbuf)==0) break;
        i++;
    }

    if (espCgiApiNodes[i].cmd==NULL) {
        //Not found
        len=sprintf(pbuf, "{\n \"status\": \"404 Not Found\"\n }\n");
        //printf("Resp %s\n", pbuf);
        httpdSend(connData, pbuf, len);
    } else {
        if (connData->requestType==HTTPD_METHOD_POST) {
            //Found, req is using POST
            //printf("post cmd found %s",espCgiApiNodes[i].cmd);
            if(NULL != espCgiApiNodes[i].set){
                espCgiApiNodes[i].set(connData->post->buff);
            }
            
            //ToDo: Use result of json parsing code somehow
            len=sprintf(pbuf, "{\n \"status\": \"ok\"\n }\n");
            httpdSend(connData, pbuf, len);
            
        } else {
            //Found, req is using GET
//        	printf("GET command %s:%s found.\n", espCgiApiNodes[i].file, espCgiApiNodes[i].cmd);
            //printf("get cmd found %s\n",espCgiApiNodes[i].cmd);
            pcjson=cJSON_CreateObject();
            if(NULL == pcjson) {
                printf(" ERROR! cJSON_CreateObject fail!\n");
                return HTTPD_CGI_DONE;
            }
            ret=espCgiApiNodes[i].get(pcjson, connData->getArgs);//espCgiApiNodes[i].cmd);
            if(ret == 0){
                pchar = cJSON_Print(pcjson);
                len = strlen(pchar);
                //printf("Resp %s\n", pchar);
                httpdSend(connData, pchar, len);
            }
            
            if(pcjson){
                cJSON_Delete(pcjson);
            }
            if(pchar){
                free(pchar);
                pchar=NULL;
            }
        }
    }

    if(pbuf){
        free(pbuf);
        pbuf=NULL;
    }
    return HTTPD_CGI_DONE;
}

