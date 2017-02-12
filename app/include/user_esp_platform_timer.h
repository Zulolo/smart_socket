#ifndef __USER_DEVICEFIND_H__
#define __USER_DEVICEFIND_H__

#define TIMER_NUMBER 10

struct esp_platform_wait_timer_param {
    int wait_time_second;
    char wait_time_param[12];
    char wait_action[16];
};

struct wait_param {
    uint32 min_time_backup;
    uint16 action_number;
    uint16 count;
    char action[TIMER_NUMBER][15];
};

void user_platform_timer_start(char* pbuffer);
void user_platform_timer_restore(void);
void user_platform_timer_bkup(void);
bool PLTF_isTimerRunning(void);

#endif
