#ifndef TTH_STRUCTS_H
#define TTH_STRUCTS_H

#include "tth_codes.h"

#include <string.h>
#include <stdint.h>

struct edit_words_data__tth {
    struct edit_words_data__tth *edit_list;
    char *word;
    enum tth_cause_code cause;
};

struct word_data__tth {
    struct word_data__tth *word_list;
    char *word;
};

/* one of each is created for every dictionary */

struct dict_data__tth {
    struct dict_data__tth *dict_list;
    char *name;
    char **words;
    int len;
};

/* one of each is created for every vhost for storing... */

/* ...settings */

struct settings__tth {
    int delay_time;
    int explanation_time;
    int aftermath_time;
    int8_t strict_mode:1;
    int word_number;
    int dictionary_id;
};

/* ...info */

struct info__tth {
    enum tth_state state;
    enum tth_substate substate;
    struct settings__tth *settings;
    int speaker_pos;
    int listener_pos;
    struct dict_data__tth *dict;
    int transport_delay;
    int8_t speaker_ready:1;
    int8_t listener_ready:1;
    int number_of_turn;
    int64_t start_time;
    char *word;
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

/* storage for in message fragments */
struct in_msg {
    struct in_msg *in_list;
    char *payload;
    int len;
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
    int score_explained;
    int score_guessed;
};

/* one of these is created for each vhost our protocol is used with */

struct per_vhost_data__tth {
    struct lws_context *context;
    struct lws_vhost *vhost;
    const struct lws_protocols *protocol;

    struct per_session_data__tth *pss_list; /* linked-list of live pss */
    struct user_data__tth *user_list; /* linked-list of users in room */

    struct msg *msg_list;
    struct in_msg *in_list;

    int clientCnt;

    struct info__tth *info;
    
    struct dict_data__tth *dict_list;

    struct word_data__tth *fresh_words;

    struct edit_words_data__tth *edit_words;

    struct edit_words_data__tth *words;
};

#endif
