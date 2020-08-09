#include "tth_timeout.h"

#include "tth_misc.h"

#include <stdlib.h>
#include <signal.h>
#include <string.h>

/* global storage */

static struct timer_data__tth *timer;

/* internal */

void __cock_alarm(void) {
    struct itimerval *ttwait = malloc(sizeof(struct itimerval));
    if (!ttwait) {
        abort();
    }
    memset(ttwait, 0, sizeof(struct itimerval));
    gettimeofday(&(ttwait->it_value), NULL);
    ttwait->it_value.tv_sec = timer->time->tv_sec - ttwait->it_value.tv_sec;
    ttwait->it_value.tv_usec = timer->time->tv_usec - ttwait->it_value.tv_usec;
    if (ttwait->it_value.tv_usec < 0) {
        ttwait->it_value.tv_usec += 1000000;
        ttwait->it_value.tv_sec -= 1;
    }

    setitimer(ITIMER_REAL, ttwait, NULL);
}

void __sigalrm_handler(int sig) {
    struct timer_data__tth *curr = timer;
    timer = timer->timer_list;

    curr->handler(curr->args);
    free(curr->time);
    free(curr);

    if (timer) {
        __cock_alarm();
    }
}

/* external */

int tth_set_timeout(struct timeval *time, alarmhandler_t handler, void *args) {
    struct timer_data__tth *new = malloc(sizeof(struct timer_data__tth));
    if (!new) {
        return 1;
    }
    struct timeval *now = malloc(sizeof(struct timeval));
    if (!now) {
        return 1;
    }
    gettimeofday(now, NULL);
    new->time = now;
    new->time->tv_sec += time->tv_sec;
    new->time->tv_usec += time->tv_usec;

    new->handler = handler;
    new->args = args;
    new->timer_list = NULL;

    ___ll_insert_when(struct timer_data__tth, new, timer_list, timer, pptd,
            ((*pptd)->timer_list->time->tv_sec > new->time->tv_sec ||
             ((*pptd)->timer_list->time->tv_sec == new->time->tv_sec &&
              (*pptd)->timer_list->time->tv_usec > new->time->tv_usec)));

    if (timer == new) {
        __cock_alarm();
    }
    return 0;
}

int tth_timer_init(void) {
    timer = NULL;
    signal(SIGALRM, __sigalrm_handler);
    return 0;
}
