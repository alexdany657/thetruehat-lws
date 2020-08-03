#ifndef TTH_STRUCTS_H
#define TTH_STRUCTS_H

#include "tth_codes.h"

#include <string.h>
#include <stdint.h>

/* one of each is created for every vhost for storing... */

/* ...settings */

struct settings__tth {
    int delay_time;
    int exmanation_time;
    int aftermath_time;
    int8_t strict_mode:1;
    int wordNumber;
};

/* ...info */

struct info__tth {
    enum tth_state state;
    enum tth_substate substate;
    struct settings__tth *settings;
};

/* one of these created for each destination client */

struct dest_data__tth {
    struct dest_data__tth *dest_list;
    int8_t sent:1;
    int client_id;
    struct per_session_data__tth *pss;
};

/* one of these created for each message */

struct msg {
    void *payload; /* is malloc'd */
    size_t len;
    struct msg *msg_list; /* list of messages */
    struct dest_data__tth *dest_list; /* list of client_id dest */
};

/* one of these is created for each client connecting to us */

struct per_session_data__tth {
    struct per_session_data__tth *pss_list;
    struct lws *wsi;
    int client_id;
};

/* one of these is create for each user joining this room */

struct user_data__tth {
    struct user_data__tth *user_list; /* ll */
    struct per_session_data__tth *pss;
    int client_id;
    int8_t online:1; /* whether user is online */
    char *username; /* username */
    int time_zone_offset; /* time zone offset of this user*/
    /* WIP */
};

/* one of these is created for each vhost our protocol is used with */

struct per_vhost_data__tth {
    struct lws_context *context;
    struct lws_vhost *vhost;
    const struct lws_protocols *protocol;

    struct per_session_data__tth *pss_list; /* linked-list of live pss */
    struct user_data__tth *user_list; /* linked-list of users in room */

    struct msg *msg_list;

    int clientCnt;

    struct info__tth *info;
};

#endif
