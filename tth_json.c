#include "tth_json.h"

#include "tth_misc.h"

#include "cJSON/cJSON.h"
#include <libwebsockets.h>

/* external funcitons */

void *tth_get_playerList(void *_vhd) {
    struct per_vhost_data__tth *vhd = (struct per_vhost_data__tth *)_vhd;

    cJSON *_users = cJSON_CreateArray();
    if (!_users) {
        return NULL;
    }

    lws_start_foreach_llp(struct user_data__tth **, ppud, vhd->user_list) {
        cJSON *__user = cJSON_CreateObject();
        if (!__user) {
            break;
        }

        cJSON *__username = cJSON_CreateString((*ppud)->username);
        if (!__username) {
            break;
        }
        cJSON_AddItemToObjectCS(__user, "username", __username);

        cJSON *__online = cJSON_CreateBool((*ppud)->online);
        if (!__online) {
            break;
        }
        cJSON_AddItemToObjectCS(__user, "online", __online);

        cJSON_AddItemToArray(_users, __user);
    } lws_end_foreach_llp(ppud, user_list);

    return _users;
}

void *tth_get_host(void *_vhd) {
    struct per_vhost_data__tth *vhd = (struct per_vhost_data__tth *)_vhd;

    char *ahost = NULL;
    lws_start_foreach_llp(struct user_data__tth **, ppud, vhd->user_list) {
        if ((*ppud)->online) { 
            ahost = (*ppud)->username;
            break;
        }
    } lws_end_foreach_llp(ppud, user_list);
    if (!ahost) {
        ahost = "";
    }
    cJSON *_host = cJSON_CreateString(ahost);
    if (!_host) {
        return NULL;
    }

    return _host;
}

void *tth_get_state(void *_vhd) {
    struct per_vhost_data__tth *vhd = (struct per_vhost_data__tth *)_vhd;

    char *state = NULL;
    switch (vhd->info->state) {
        case TTH_STATE_WAIT:
            state = "wait";
            break;
        case TTH_STATE_PLAY:
            state = "play";
            break;
        case TTH_STATE_END:
            state = "end";
            break;
        default:
            return NULL;
    }

    cJSON *_state = cJSON_CreateString(state);
    if (!_state) {
        return NULL;
    }
    
    return _state;
}

void *tth_get_substate(void *_vhd) {
    struct per_vhost_data__tth *vhd = (struct per_vhost_data__tth *)_vhd;

    char *substate = NULL;
    switch (vhd->info->substate) {
        case TTH_SUBSTATE_WAIT:
            substate = "wait";
            break;
        case TTH_SUBSTATE_EXPLANATION:
            substate = "explanation";
            break;
        case TTH_SUBSTATE_EDIT:
            substate = "edit";
            break;
        default:
            return NULL;
    }

    cJSON *_substate = cJSON_CreateString(substate);
    if (!_substate) {
        return NULL;
    }

    return _substate;
}
