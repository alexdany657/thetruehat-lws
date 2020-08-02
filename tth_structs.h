#ifndef TTH_STRUCTS_H
#define TTH_STRUCTS_H

#include <string.h>
#include <stdint.h>

/* one of these created for each */

struct dest_data__tth {
    struct dest_data__tth *dest_list;
    int8_t sent:1;
    int clientId;
    struct per_session_data__tth *pss;
};

/* one of these created for each message */

struct msg {
    void *payload; /* is malloc'd */
    size_t len;
    struct msg *msg_list; /* list of messages */
    struct dest_data__tth *dest_list; /* list of clientId dest */
};

/* one of these is created for each client connecting to us */

struct per_session_data__tth {
    struct per_session_data__tth *pss_list;
    struct lws *wsi;
    int clientId;
};

/* one of these is create for each user joining this room */

struct user_data__tth {
    struct user_data__tth *user_list; /* ll */
    struct per_session_data__tth *pss;
    int clientId;
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
};

#endif
