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

#define local_host_name		"iot.zulolo.cn"
#define local_host_port		"80"

nopoll_bool debug = nopoll_false;
nopoll_bool show_critical_only = nopoll_false;

void __report_critical (noPollCtx * ctx, noPollDebugLevel level, const char * log_msg, noPollPtr user_data)
{
        if (level == NOPOLL_LEVEL_CRITICAL) {
  	        printf ("CRITICAL: %s\n", log_msg);
	}
	return;
}

noPollCtx * create_ctx (void) {

	/* create a context */
	noPollCtx * ctx = nopoll_ctx_new ();
	nopoll_log_enable (ctx, debug);
	nopoll_log_color_enable (ctx, debug);

	/* configure handler */
	if (show_critical_only)
		nopoll_log_set_handler (ctx, __report_critical, NULL);
	return ctx;
}

LOCAL void websocket_main()
{
	noPollCtx  * ctx;
	noPollConn * conn;
	char         buffer[256];
	int          bytes_read;

	memset (buffer, 0, sizeof(buffer));

	while(user_esp_platform_get_device_connect_status() != DEVICE_GOT_IP){
		vTaskDelay(2000 / portTICK_RATE_MS);
	}

	/* create context */
	ctx = create_ctx ();

	/* check connections registered */
	if (nopoll_ctx_conns (ctx) != 0) {
		printf ("ERROR: expected to find 0 registered connections but found: %d\n", nopoll_ctx_conns (ctx));
		nopoll_cleanup_library ();
		vTaskDelete(NULL);
		return ;
	} /* end if */

	nopoll_ctx_unref (ctx);

	/* reinit again */
	ctx = create_ctx ();

	/* call to create a connection */
	printf ("nopoll_conn_new\n");
	conn = nopoll_conn_new (ctx, local_host_name, local_host_port, NULL, NULL, NULL, NULL);
	if (! nopoll_conn_is_ok (conn)) {
		printf ("ERROR: Expected to find proper client connection status, but found error..\n");
		nopoll_ctx_unref (ctx);
		nopoll_cleanup_library ();
		vTaskDelete(NULL);
		return ;
	}

	/* wait for the reply (try to read 1024, blocking and with a 3 seconds timeout) */
	printf ("Test 03: now reading reply..\n");
	bytes_read = nopoll_conn_read (conn, buffer, 14, nopoll_true, 3000);

	printf("Recv: %s\n", buffer);

	nopoll_conn_close (conn);
	nopoll_ctx_unref (ctx);
	nopoll_cleanup_library ();
	vTaskDelete(NULL);
	printf("delete the websocket_task\n");
}

/*start the websocket task*/
void websocket_start(void)
{
	xTaskCreate(websocket_main, "websocket_task", 2048, NULL, 4, NULL);
}

