#ifndef TTH_TIMEOUT_H
#define TTH_TIMEOUT_H

#include <time.h>
#include <sys/time.h>

typedef void (*alarmhandler_t)(void *);

struct timer_data__tth {
    struct timer_data__tth *timer_list;
    struct timeval *time;
    alarmhandler_t handler;
    void *args;
};

int tth_set_timeout(struct timeval *time, alarmhandler_t handler, void *args);

int tth_timer_init(void);

#endif
