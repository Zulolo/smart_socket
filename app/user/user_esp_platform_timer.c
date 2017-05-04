/******************************************************************************
 * Copyright (C) 2014 -2016  Espressif System
 *
 * FileName: esp_platform_user_timer.c
 *
 * Description: get and config the device timer
 *
 * Modification history:
 * 2015/7/1, v1.0 create this file.
*******************************************************************************/
#include "esp_common.h"
#include "user_config.h"
#include "user_esp_platform.h"
#include "user_esp_platform_timer.h"

#include "user_plug.h"

#define DEBUG

#ifdef DEBUG
#define TM_DEBUG os_printf
#else
#define TM_DEBUG
#endif

#define RTC_MEM_TIMER_ADDR 72

LOCAL os_timer_t tSchedulingTimer;
uint32 unMinWaitSecond;

char *pTimerSplitsStrings[TIMER_NUMBER] = {0};

LOCAL struct esp_platform_wait_timer_param *pTimerWaitParam = NULL;

LOCAL struct wait_param tWaitAction;

LOCAL struct timer_bkup_param tTimerParam;

extern SmartSocketParameter_t tSmartSocketParameter;
extern xSemaphoreHandle xSmartSocketParameterSemaphore;;

void esp_platform_timer_schedule(struct esp_platform_wait_timer_param *timer_wait_param, uint16 count);

bool PLTF_isTimerRunning(void)
{
	if (NULL == pTimerWaitParam){
		return false;
	}else{
		return true;
	}
}

/******************************************************************************
 * FunctionName : split
 * Description  : split string p1 according to sting p2 and save the splits
 * Parameters   : p1 , p2 ,splits[]
 * Returns      : the number of splits
*******************************************************************************/
uint8
split(char *p1, char *p2, char *splits[])
{
    int i = 0;
    int j = 0;

    while (i != -1) {
        int start = i;
        int end = indexof(p1, p2, start);

        if (end == -1) {
            end = strlen(p1);
        }

        char *p = (char *) zalloc(end - start+1);
        memcpy(p, p1 + start, end - start);
        p[end - start] = '\0';
        splits[j] = p;
        j++;
        i = end + 1;

        if (i > strlen(p1)) {
            break;
        }
    }

    return j;
}

/******************************************************************************
 * FunctionName : indexof
 * Description  : calculate the offset of p2 relate to start of p1
 * Parameters   : p1,p1,start
 * Returns      : the offset of p2 relate to the start
*******************************************************************************/
int  
indexof(char *p1, char *p2, int start)
{
    char *find = (char *)strstr(p1 + start, p2);

    if (find != NULL) {
        return (find - p1);
    }

    return -1;
}

/******************************************************************************
 * FunctionName : esp_platform_find_min_time
 * Description  : find the minimum wait second in timer list
 * Parameters   : timer_wait_param -- param of timer action and wait time param
 *                count -- The number of timers given by server
 * Returns      : none
*******************************************************************************/
void  
esp_platform_find_min_time(struct esp_platform_wait_timer_param *timer_wait_param , uint16 count)
{
    uint16 i = 0;
    unMinWaitSecond = 0xFFFFFFFF;

    for (i = 0; i < count ; i++) {
        if (timer_wait_param[i].wait_time_second < unMinWaitSecond && timer_wait_param[i].wait_time_second > 0) {
            unMinWaitSecond = timer_wait_param[i].wait_time_second;
        }
    }

    if (0xFFFFFFFF == unMinWaitSecond){
    	unMinWaitSecond = 0;
    }
}

/******************************************************************************
 * FunctionName : user_platform_timer_first_start
 * Description  : calculate the wait time of each timer
 * Parameters   : count -- The number of timers given by server
 * Returns      : none
*******************************************************************************/
void  
user_platform_timer_first_start(uint16 count)
{
    int i = 0;
    struct esp_platform_wait_timer_param *pt_w_p;

    if(NULL == pTimerWaitParam){
        pTimerWaitParam = (struct esp_platform_wait_timer_param *)zalloc(sizeof(struct esp_platform_wait_timer_param)*TIMER_NUMBER);
    }
    pt_w_p = pTimerWaitParam;
    memset(pt_w_p, 0, sizeof(struct esp_platform_wait_timer_param)*TIMER_NUMBER);
    
    //update timestamp base
    tTimerParam.unTimestamp = tTimerParam.unTimestamp + unMinWaitSecond;
    TM_DEBUG("next timestamp= %d s \n", tTimerParam.unTimestamp);
    
    for (i = 0 ; i < count ; i++) {
        char *str = pTimerSplitsStrings[i];
        /* l60=on_off_switch
         * w214694=off_switch
         * w41894=off_switch
         * f1436325630=off_switch  */

        if ('f' == str[0]) {
        	char *fixed_wait[2];

            TM_DEBUG("timer is fixed mode\n");

            split(str, "=", fixed_wait);
            
			if((strlen(fixed_wait[0]) - 1) <= 12 )
				memcpy(pt_w_p->wait_time_param, fixed_wait[0] + 1, strlen(fixed_wait[0]) - 1);
			else
				os_printf("ERR:arr_overflow,%u,%d\n",__LINE__, (strlen(fixed_wait[0]) - 1) );

			if(strlen(fixed_wait[1]) <= 16 )
				memcpy(pt_w_p->wait_action, fixed_wait[1], strlen(fixed_wait[1]) );
			else
				os_printf("ERR:arr_overflow,%u,%d\n",__LINE__, strlen(fixed_wait[1]) );
            
            pt_w_p->wait_time_second = atoi(pt_w_p->wait_time_param) - tTimerParam.unTimestamp;
            free(fixed_wait[0]);
            free(fixed_wait[1]);
        }else if ('l' == str[0]) {
            char *loop_wait[2];

            TM_DEBUG("timer is loop mode\n");/*every sec or min or hour or day tiger the action*/

            split(str, "=", loop_wait);

             if((strlen(loop_wait[0]) - 1) <= 12 )
                memcpy(pt_w_p->wait_time_param, loop_wait[0] + 1, strlen(loop_wait[0]) - 1);
            else
                os_printf("ERR:arr_overflow,%u,%d\n",__LINE__, (strlen(loop_wait[0]) - 1) );
        
             if(strlen(loop_wait[1]) <= 16 )
                memcpy(pt_w_p->wait_action, loop_wait[1], strlen(loop_wait[1]));
            else
                os_printf("ERR:arr_overflow,%u,%d\n",__LINE__, strlen(loop_wait[1]) );

            pt_w_p->wait_time_second = atoi(pt_w_p->wait_time_param) - (tTimerParam.unTimestamp % atoi(pt_w_p->wait_time_param));
            free(loop_wait[0]);
            free(loop_wait[1]);
        } else if ('w' == str[0]) {
            char *week_wait[2];
            int monday_wait_time = 0;

            TM_DEBUG("timer is weekend mode\n");

            split(str, "=", week_wait);

			if((strlen(week_wait[0]) - 1) <= 12 )
				memcpy(pt_w_p->wait_time_param, week_wait[0] + 1, strlen(week_wait[0]) - 1);
			else
				os_printf("ERR:arr_overflow,%u,%d\n",__LINE__, (strlen(week_wait[0]) - 1) );

			if(strlen(week_wait[1]) <= 16 )
				memcpy(pt_w_p->wait_action, week_wait[1], strlen(week_wait[1]));
			else
				os_printf("ERR:arr_overflow,%u,%d\n",__LINE__, strlen(week_wait[1]) );

            monday_wait_time = (tTimerParam.unTimestamp - 1388966400) % (7 * 24 * 3600);	//GMT: Mon, 06 Jan 2014 00:00:00 GMT
            
            TM_DEBUG("monday_wait_time=%d \n", monday_wait_time);
            
            if(atoi(pt_w_p->wait_time_param) > monday_wait_time) {
                pt_w_p->wait_time_second = atoi(pt_w_p->wait_time_param) - monday_wait_time;
            } else {
                pt_w_p->wait_time_second = 7 * 24 * 3600 - monday_wait_time + atoi(pt_w_p->wait_time_param);
            }
            free(week_wait[0]);
            free(week_wait[1]);
        }
        
        TM_DEBUG("pt_w_p->wait_time_second=%d \n", pt_w_p->wait_time_second);
        
        pt_w_p++;
    }

    esp_platform_find_min_time(pTimerWaitParam, count);
    if(unMinWaitSecond == 0) {
        return;
    }

    esp_platform_timer_schedule(pTimerWaitParam, count);
}

/******************************************************************************
 * FunctionName : user_esp_platform_device_action
 * Description  : Execute the actions of minimum wait time
 * Parameters   : pwait_action -- point the list of actions which need execute
 *
 * Returns      : none
*******************************************************************************/
void  
user_esp_platform_device_action(struct wait_param *pwait_action)
{
    uint8 i = 0;
    uint16 count = pwait_action->count;
    uint16 action_number = pwait_action->action_number;

    for (i = 0; i < action_number && pwait_action->action[i][0] != '0'; i++) {
        TM_DEBUG("%s\n",pwait_action->action[i]);

        if (strncmp(pwait_action->action[i], "on_switch", 9) == 0) {
            user_plug_set_status(PLUG_STATUS_CLOSE);
        } else if (strncmp(pwait_action->action[i], "off_switch", 10) == 0) {
            user_plug_set_status(PLUG_STATUS_OPEN);
        } else if (strncmp(pwait_action->action[i], "on_off_switch", 13) == 0) {
            if (user_plug_get_status() == 0) {
                user_plug_set_status(PLUG_STATUS_CLOSE);
            } else {
                user_plug_set_status(PLUG_STATUS_OPEN);
            }
        } else {
            return;
        }
    }
    user_platform_timer_first_start(count);
}

/******************************************************************************
 * FunctionName : user_esp_platform_wait_time_overflow_check
 * Description  : Processing the message about timer from the server
 * Parameters   : timer_wait_param -- The received data from the server
 *                count -- the espconn used to connetion with the host
 * Returns      : none
*******************************************************************************/

void  
user_esp_platform_wait_time_overflow_check(struct wait_param *pwait_action)
{
    TM_DEBUG("min_wait_second = %d\n", pwait_action->min_time_backup);
//    static bool bOvflwChkDcrsScnd = false;

//    if(bOvflwChkDcrsScnd != false){
//        pwait_action->min_time_backup -= 3600;
//    }

    if (pwait_action->min_time_backup >= 3600) {
    	pwait_action->min_time_backup -= 3600;
        os_timer_disarm(&tSchedulingTimer);
        os_timer_setfn(&tSchedulingTimer, (os_timer_func_t *)user_esp_platform_wait_time_overflow_check, pwait_action);
        os_timer_arm(&tSchedulingTimer, 3600000, 0);
        TM_DEBUG("min_wait_second is extended\n");
//        bOvflwChkDcrsScnd = true;
    } else {
        os_timer_disarm(&tSchedulingTimer);
        os_timer_setfn(&tSchedulingTimer, (os_timer_func_t *)user_esp_platform_device_action, pwait_action);
        os_timer_arm(&tSchedulingTimer, pwait_action->min_time_backup * 1000, 0);
        TM_DEBUG("min_wait_second is = %dms\n", pwait_action->min_time_backup * 1000);
//        bOvflwChkDcrsScnd = false;
    }
}

/******************************************************************************
 * FunctionName : esp_platform_timer_schedule
 * Description  : Processing the message about timer from the server
 * Parameters   : timer_wait_param -- The received data from the server
 *                count -- the espconn used to connetion with the host
 * Returns      : none
*******************************************************************************/
void  
esp_platform_timer_schedule(struct esp_platform_wait_timer_param *timer_wait_param, uint16 count)
{
    uint16 i = 0;
    uint16 action_number;

    tWaitAction.count = count;
    action_number = 0;

    for (i = 0; i < count; i++) {
        if (timer_wait_param[i].wait_time_second == unMinWaitSecond) {
            memcpy(tWaitAction.action[action_number], timer_wait_param[i].wait_action, strlen(timer_wait_param[i].wait_action));
            TM_DEBUG("*****%s*****\n", timer_wait_param[i].wait_action);
            action_number++;
        }
    }

    tWaitAction.action_number = action_number;
    tWaitAction.min_time_backup = unMinWaitSecond;
    user_esp_platform_wait_time_overflow_check(&tWaitAction);
}

/******************************************************************************
 * FunctionName : user_platform_timer_bkup
 * Description  : backup the timer related data to RTC memory
 * Parameters   : void --

 * Returns      : none
*******************************************************************************/
void  
user_platform_timer_bkup(void)
{

	tSmartSocketParameter.tTimerBkupParam.unTimestamp = sntp_get_current_timestamp();
	tSmartSocketParameter.tTimerBkupParam.unMagic = 0xA5A5A5A5;
	tSmartSocketParameter.tTimerBkupParam.unBufferSize = tTimerParam.unBufferSize;
    memcpy(tSmartSocketParameter.sTimerSplitsString, tTimerParam.pSplitBuffer, tTimerParam.unBufferSize);
	if(xSemaphoreTake(xSmartSocketParameterSemaphore, (portTickType)(10000/portTICK_RATE_MS)) == pdTRUE ){
		system_param_save_with_protect(GET_USER_DATA_SECTORE(USER_DATA_CONF_PARA),
							&tSmartSocketParameter, sizeof(tSmartSocketParameter));
		xSemaphoreGive(xSmartSocketParameterSemaphore);
	}else{
		printf("Take parameter semaphore failed.\n");
	}
}

/******************************************************************************
 * FunctionName : user_platform_timer_restore
 * Description  : recover the timer related data from RTC memory
 * Parameters   : void
 * Returns      : none
*******************************************************************************/
void  
user_platform_timer_restore(void)
{
    /*check the maic number, 0xa5a5 means saved before reboot*/
    if((0xA5A5A5A5 == tSmartSocketParameter.tTimerBkupParam.unMagic) &&
    		(tSmartSocketParameter.sTimerSplitsString[0] != '\0') &&
			(tSmartSocketParameter.tTimerBkupParam.unBufferSize != 0)){
        printf(">>>user_platform_timer_restore \n");
        printf(">>>copy timer configuration %u back \n", tSmartSocketParameter.tTimerBkupParam.unBufferSize);
        tTimerParam.unBufferSize = tSmartSocketParameter.tTimerBkupParam.unBufferSize;
        tTimerParam.pSplitBuffer = malloc(tSmartSocketParameter.tTimerBkupParam.unBufferSize);
        if(NULL == tTimerParam.pSplitBuffer){
            printf(">>>system memory exhausted, check it\n");
            return;
        }
        memcpy(tTimerParam.pSplitBuffer, tSmartSocketParameter.sTimerSplitsString, tSmartSocketParameter.tTimerBkupParam.unBufferSize);

        /*make sure the min_wait_second be zero*/
        unMinWaitSecond = 0;
        
        tTimerParam.unTimestamp = sntp_get_current_timestamp();
        uint16 count = split(tTimerParam.pSplitBuffer, ";" , pTimerSplitsStrings);
        user_platform_timer_first_start(count);
        
    }else{
        /*clean the dirty data*/
        memset(&tTimerParam, 0, sizeof(tTimerParam));
    }

}

void
user_platform_timer_reset(void)
{
	os_timer_disarm(&tSchedulingTimer);
	tSmartSocketParameter.tTimerBkupParam.unTimestamp = 0;
	tSmartSocketParameter.tTimerBkupParam.unMagic = 0;
	tSmartSocketParameter.tTimerBkupParam.unBufferSize = 0;
    memset(tSmartSocketParameter.sTimerSplitsString, 0, TIMER_SAVE_FLASH_NUMBER*EACH_TIMER_SAVE_FLASH_MAX_LEN);
	if(xSemaphoreTake(xSmartSocketParameterSemaphore, (portTickType)(10000/portTICK_RATE_MS)) == pdTRUE ){
		system_param_save_with_protect(GET_USER_DATA_SECTORE(USER_DATA_CONF_PARA),
							&tSmartSocketParameter, sizeof(tSmartSocketParameter));
		xSemaphoreGive(xSmartSocketParameterSemaphore);
	}else{
		printf("Take parameter semaphore failed.\n");
	}
}

/******************************************************************************
 * FunctionName : user_platform_timer_start
 * Description  : Processing the message about timer from the server
 * Parameters   : pbuffer -- The received data from the server

 * Returns      : none
*******************************************************************************/
void  
user_platform_timer_start(char *pbuffer)
{
    int str_begin = 0;
    int str_end = 0;
    uint8 i = 0;
    char *pstr_start = NULL;
    char *pstr_end = NULL;
    char *pstr = NULL;
    char timestamp_str[11]={0};
    
    unMinWaitSecond = 0;

	if ((pstr = (char *)strstr(pbuffer, "\"timestamp\":")) != NULL) {
		pstr_start = pstr + 13;
		pstr_end = (char *)strstr(pstr_start, ",");

		if((pstr_end - pstr_start) <= 11){
			memcpy(timestamp_str, pstr_start, pstr_end - pstr_start);
		}else{
			os_printf("ERR:arr_overflow,%u,%d\n",__LINE__, (pstr_end - pstr_start) );
			return;
		}

		tTimerParam.unTimestamp = atoi(timestamp_str);
	}

	for (i = 0 ; i < TIMER_NUMBER ; i++) {
		if (pTimerSplitsStrings[i] != NULL) {
			free(pTimerSplitsStrings[i]);
			pTimerSplitsStrings[i] = NULL;
		}
	}

	if ((pstr_start = (char *)strstr(pbuffer, "\"timers\": \"")) != NULL) {
		str_begin = 11;
		str_end = indexof(pstr_start, "\"", str_begin);

		if (str_begin == str_end) {
			os_timer_disarm(&tSchedulingTimer);
			return;
		}

		/*""timestamp": 1436324774, "timers": "l60=on_off_switch", "weekday": 3, "now": "2015-07-08 11:06:14"}*/
		/*""timestamp": 1436325489, "timers": "f1436325630=off_switch;l60=on_off_switch", "weekday": 3, "now": "2015-07-08 11:18:09"}*/
		/*""timestamp": 1436326580, "timers": "l60=on_off_switch;w214694=off_switch;w41894=off_switch", "weekday": 3, "now": "2015-07-08 11:36:20"}"}*/

		/*free the old one when a new time configuration cames*/
		if(tTimerParam.pSplitBuffer != NULL){
			free(tTimerParam.pSplitBuffer);
		}

		/* save the buffer size and string when a new time configuration cames */
		tTimerParam.unBufferSize = str_end - str_begin + 1;
		if (tTimerParam.unBufferSize >= TIMER_SAVE_FLASH_NUMBER*EACH_TIMER_SAVE_FLASH_MAX_LEN){
			os_printf("ERR:arr_overflow,%u,%d\n",__LINE__, (pstr_end - pstr_start) );
			return;
		}
		tTimerParam.pSplitBuffer = (char *)zalloc(tTimerParam.unBufferSize);
		memcpy(tTimerParam.pSplitBuffer, pstr_start + str_begin, str_end - str_begin);

		uint8 count = split(tTimerParam.pSplitBuffer, ";", pTimerSplitsStrings);
		user_platform_timer_bkup();
		user_platform_timer_first_start(count);
	}
}

