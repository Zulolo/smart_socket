/*
 * websocket.c
 *
 *  Created on: May 3, 2017
 *      Author: zulolo
 */
#include "esp_common.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "nopoll/nopoll.h"

#include "smart_socket_global.h"
#include "user_esp_platform.h"
#include "user_cgi.h"

#define REMOTE_HOST_NAME			"iot.zulolo.cn"
#define REMOTE_HOST_PORT			"80"
#define WEBSOCKET_CMD_SEG_NUM		4

LOCAL nopoll_bool debug = nopoll_false;
LOCAL nopoll_bool show_critical_only = nopoll_false;
//extern SmartSocketParameter_t tSmartSocketParameter;
extern const EspCgiApiEnt espCgiApiNodes[];
LOCAL char *pWebsocketCmdSplitStrings[WEBSOCKET_CMD_SEG_NUM] = {0};
const char cNopollSendTextError[] = "ERROR: Expected to find proper send operation..\n";

LOCAL int numberof(char cChar, char* pStr)
{
	int nIndex;
	int nNumber = 0;
	int nStrLen = strlen(pStr);

	for (nIndex = 0; nIndex < nStrLen; nIndex++){
		if (cChar == pStr[nIndex]){
			nNumber++;
		}
	}
    return nNumber;
}

LOCAL void __report_critical (noPollCtx * ctx, noPollDebugLevel level, const char * log_msg, noPollPtr user_data)
{
        if (level == NOPOLL_LEVEL_CRITICAL) {
  	        printf ("CRITICAL: %s\n", log_msg);
	}
	return;
}

LOCAL noPollCtx * create_ctx (void) {

	/* create a context */
	noPollCtx * ctx = nopoll_ctx_new ();
	nopoll_log_enable (ctx, debug);
	nopoll_log_color_enable (ctx, debug);

	/* configure handler */
	if (show_critical_only)
		nopoll_log_set_handler (ctx, __report_critical, NULL);
	return ctx;
}

LOCAL int parseCommand(noPollConn* pNopollConn, const EspCgiApiEnt* pEspCgiApiNodes, char* pBuffer)
{
	uint8 unIndex;
	uint8 unCount;
	int nLen;
	cJSON *pJson = NULL;
	int nRslt = 0;
	char* pJsonChar;
	char *pbuf=(char*)zalloc(64);

	if (NULL == pbuf){
		printf("ERROR: No free memory for parseCommand..\n");
		return (-1);
	}
	// 'post/config/switch/{"Response":{"status":1}}'
	// 'get/config/switch/what_ever'
	if (numberof('/', pBuffer) != (WEBSOCKET_CMD_SEG_NUM - 1)){
		printf("ERROR: Websocket received format error..\n");
		nLen = sprintf(pbuf, "{\n \"status\": \"404 Not Found\"\n }\n");
		if (nopoll_conn_send_text(pNopollConn, pbuf, nLen) != nLen) {
			printf(cNopollSendTextError);
		}
		free(pbuf);
		return (-1);
	}

	for (unIndex = 0 ; unIndex < WEBSOCKET_CMD_SEG_NUM ; unIndex++) {
		if (pWebsocketCmdSplitStrings[unIndex] != NULL) {
			free(pWebsocketCmdSplitStrings[unIndex]);
			pWebsocketCmdSplitStrings[unIndex] = NULL;
		}
	}

	unCount = split(pBuffer, "/", pWebsocketCmdSplitStrings);

	unIndex = 0;
    while (pEspCgiApiNodes[unIndex].cmd!=NULL) {
        if (strcmp(pEspCgiApiNodes[unIndex].file, pWebsocketCmdSplitStrings[1])==0 &&
        		strcmp(pEspCgiApiNodes[unIndex].cmd, pWebsocketCmdSplitStrings[2])==0){
        	break;
        }
        unIndex++;
    }

    if (espCgiApiNodes[unIndex].cmd == NULL) {
    	nLen = sprintf(pbuf, "{\n \"status\": \"404 Not Found\"\n }\n");
    	if (nopoll_conn_send_text (pNopollConn, pbuf, nLen) != nLen) {
    		printf(cNopollSendTextError);
    	}
    	free(pbuf);
        return (-1);
    } else {
        if (strcmp("post", pWebsocketCmdSplitStrings[0])==0) {
            //Found, req is using POST
            //printf("post cmd found %s",espCgiApiNodes[i].cmd);
            if(NULL != espCgiApiNodes[unIndex].set){
                espCgiApiNodes[unIndex].set(pWebsocketCmdSplitStrings[3]);
                nLen = sprintf(pbuf, "{\n \"status\": \"ok\"\n }\n");
                if (nopoll_conn_send_text(pNopollConn, pbuf, nLen) != nLen) {
					printf(cNopollSendTextError);
				}
                free(pbuf);
				return 0;
            } else {
            	nLen = sprintf(pbuf, "{\n \"status\": \"400 Can not understand\"\n }\n");
				if (nopoll_conn_send_text(pNopollConn, pbuf, nLen) != nLen) {
					printf(cNopollSendTextError);
				}
				free(pbuf);
				return (-1);
            }
        } else {
            //Found, req is using GET
//        	printf("GET command %s:%s found.\n", espCgiApiNodes[i].file, espCgiApiNodes[i].cmd);
            //printf("get cmd found %s\n",espCgiApiNodes[i].cmd);
            pJson=cJSON_CreateObject();
            if(NULL == pJson) {
                printf("ERROR! cJSON_CreateObject fail!\n");
            	nLen = sprintf(pbuf, "{\n \"status\": \"500 Internal error\"\n }\n");
				if (nopoll_conn_send_text(pNopollConn, pbuf, nLen) != nLen) {
					printf(cNopollSendTextError);
				}
				free(pbuf);
				return (-1);
            } else {
                nRslt=espCgiApiNodes[unIndex].get(pJson, pWebsocketCmdSplitStrings[3]);
                if(nRslt == 0){
                    pJsonChar = cJSON_Print(pJson);
                    if (NULL == pJsonChar){
                    	printf("ERROR! cJSON_Print fail!\n");
                    	cJSON_Delete(pJson);
                        free(pbuf);
        				return (-1);
                    } else {
                        nLen = strlen(pJsonChar);
                        if (nopoll_conn_send_text(pNopollConn, pJsonChar, nLen) != nLen) {
        					printf(cNopollSendTextError);
        				}
                        cJSON_Delete(pJson);
                        free(pJsonChar);
                        free(pbuf);
        				return 0;
                    }
                }
            }
        }
    }
}

LOCAL void websocket_main()
{
	noPollCtx  * ctx;
	noPollConn * conn;
	char         buffer[256];
	int          bytes_read;

	while(user_esp_platform_get_device_connect_status() != DEVICE_GOT_IP){
		vTaskDelay(2000 / portTICK_RATE_MS);
	}

	/* create context */
	ctx = create_ctx();

	/* check connections registered */
	if (nopoll_ctx_conns(ctx) != 0) {
		printf ("ERROR: expected to find 0 registered connections but found: %d\n", nopoll_ctx_conns (ctx));
		nopoll_cleanup_library();
		vTaskDelete(NULL);
		return ;
	} /* end if */

	nopoll_ctx_unref(ctx);

	/* reinit again */
	ctx = create_ctx();

	/* call to create a connection */
	printf("nopoll_conn_new\n");
	conn = nopoll_conn_new (ctx, REMOTE_HOST_NAME, REMOTE_HOST_PORT, NULL, NULL, NULL, NULL);
	if (! nopoll_conn_is_ok (conn)) {
		printf("ERROR: Expected to find proper client connection status, but found error..\n");
		nopoll_ctx_unref(ctx);
		nopoll_cleanup_library();
		vTaskDelete(NULL);
		return ;
	}

	/* wait for the reply (try to read 1024, blocking and with a 3 seconds timeout) */
	printf ("Now reading reply..\n");
	while(1){
		memset(buffer, 0, sizeof(buffer));
		bytes_read = nopoll_conn_read(conn, buffer, sizeof(buffer), nopoll_true, 2000);
		if (((-1) == bytes_read) || (sizeof(buffer) == bytes_read)) {
			// -1 means read error, sizeof(buffer) means flushing the buffer
			continue;
		}
		printf("Recv: %s\n", buffer);
		parseCommand(conn, espCgiApiNodes, buffer);
	}

	nopoll_conn_close(conn);
	nopoll_ctx_unref(ctx);
	nopoll_cleanup_library();
	vTaskDelete(NULL);
	printf("delete the websocket_task\n");
}

/*start the websocket task*/
void websocket_start(void)
{
	xTaskCreate(websocket_main, "websocket_task", 2048, NULL, 4, NULL);
}

