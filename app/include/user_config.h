#ifndef __USER_CONFIG_H__
#define __USER_CONFIG_H__

/*support one platform at the same project*/
#define ESP_PLATFORM            1
#define LEWEI_PLATFORM          0

/*support one server at the same project*/
#define HTTPD_SERVER            1
#define WEB_SERVICE             0

/*support one device at the same project*/
#define PLUG_DEVICE             1
#define LIGHT_DEVICE            0
#define SENSOR_DEVICE           0 //TBD


#if LIGHT_DEVICE
#define USE_US_TIMER
#endif

#define RESTORE_KEEP_TIMER 0

#if ESP_PLATFORM

#if PLUG_DEVICE || LIGHT_DEVICE
#define BEACON_TIMEOUT  150000000
#define BEACON_TIME     50000
#endif

#if SENSOR_DEVICE
#define HUMITURE_SUB_DEVICE         1
#define FLAMMABLE_GAS_SUB_DEVICE    0
#endif

#if SENSOR_DEVICE
#define SENSOR_DEEP_SLEEP

#if HUMITURE_SUB_DEVICE
#define SENSOR_DEEP_SLEEP_TIME    30000000
#elif FLAMMABLE_GAS_SUB_DEVICE
#define SENSOR_DEEP_SLEEP_TIME    60000000
#endif
#endif

#define AP_CACHE
#ifdef AP_CACHE
#define AP_CACHE_NUMBER    5
#endif


//#define SOFTAP_ENCRYPT
#ifdef SOFTAP_ENCRYPT
#define PASSWORD    "v*%W>L<@i&Nxe!"
#endif

#define USE_DNS
#define FW_UPGRADE_DOMAIN      "iot.zulolo.cn"

/*SSL not Ready*/
//#define SERVER_SSL_ENABLE 
//#define CLIENT_SSL_ENABLE
//#define UPGRADE_SSL_ENABLE

#elif LEWEI_PLATFORM

#endif

#endif

