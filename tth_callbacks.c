#include "tth_structs.h"
#include "tth_misc.h"

#include "tth_callbacks.h"
#include "tth_signals.h"

#include <stdlib.h>
#include <string.h>
#include <libwebsockets.h>
#include "cJSON/cJSON.h"

/* server */

int tth_callback_error(void *vhd, void *pss, char *msg, int len) {
    char *_msg = malloc(len + 1);
    if (!_msg) {
        lwsl_user("OOM: dropping\n");
        return 1;
    }
    memcpy(_msg, msg, len);
    *(_msg + len) = 0;
    lwsl_err("tth: tth_callback_error on message: \"%s\"\n", _msg);
    int needed = snprintf(NULL, 0, "tth: tth_callback_error on message: \"%s\"\n", _msg);
    char *ret_msg = malloc(needed);
    if (!ret_msg) {
        lwsl_user("OOM: dropping\n");
        return 1;
    }
    sprintf(ret_msg, "tth: tth_callback_error on message: \"%s\"\n", _msg);
    tth_sMessage(vhd, pss, ret_msg, "error", "");
    free(ret_msg);
    free(_msg);
    return 1;
}

int tth_callback_server(void *vhd, void *pss, int code, char *msg, int len) {
    char *_msg = malloc(len + 1);
    if (!_msg) {
        lwsl_user("OOM: dropping\n");
        return 1;
    }
    memcpy(_msg, msg, len);
    *(_msg + len) = 0;
    lwsl_warn("tth: tth_callbacks: got message with code %i \"%s\" that can be only emitted by server, ignoring\n", code, _msg);
    int needed = snprintf(NULL, 0, "tth: tth_callbacks: got message with code %i \"%s\" that can be only emitted by server, ignoring\n", code, _msg);
    char *ret_msg = malloc(needed);
    if (!ret_msg) {
        lwsl_user("OOM: dropping\n");
        return 1;
    }
    sprintf(ret_msg, "tth: tth_callbacks: got message with code %i \"%s\" that can be only emitted by server, ignoring\n", code, _msg);
    tth_sMessage(vhd, pss, ret_msg, "error", "");
    free(ret_msg);
    free(_msg);
    return 1;
}

/* client */

int tth_callback_client_join_room(void *_pss, void *_vhd, char *msg, int len) {
    struct per_session_data__tth *pss = (struct per_session_data__tth *)_pss;
    struct per_vhost_data__tth *vhd = (struct per_vhost_data__tth *)_vhd;
    char *_msg = malloc(len + 1);
    if (!_msg) {
        lwsl_user("OOM: dropping\n");
        tth_sMessage(vhd, pss, "Server error", "error", "cJoinRoom");
        return 1;
    }
    memcpy(_msg, msg, len);
    *(_msg + len) = 0;
    lwsl_user("tth: tth_callback_client_join_room: \"%s\"\n", _msg);
    free(_msg);

    cJSON *_username;
    cJSON *_time_zone_offset;
    cJSON *_data = cJSON_ParseWithLength(msg, len);

    if (_data == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            lwsl_err("tth: tth_callback_client_join_room: cJSON: %s\n", error_ptr);
        }
        tth_sMessage(vhd, pss, "Server error", "error", "cJoinRoom");
        return 1;
    }

    _username = cJSON_GetObjectItemCaseSensitive(_data, "username");
    if (!cJSON_IsString(_username)) {
        lwsl_user("Improper type of field `username`, not a string\n");
        tth_sMessage(vhd, pss, "Improper type of field `username`, not a string", "error", "cJoinRoom");
        return 1;
    }
    
    _time_zone_offset = cJSON_GetObjectItemCaseSensitive(_data, "time_zone_offset");
    if (!cJSON_IsNumber(_time_zone_offset)) {
        lwsl_user("Improper type of field `time_zone_offset`, not a number\n");
        tth_sMessage(vhd, pss, "Improper type of field `time_zone_offset`, not a number", "error", "cJoinRoom");
        return 1;
    }

    if (*(_username->valuestring) == 0) {
        lwsl_user("Improper value of field `username`, <empty string>\n");
        tth_sMessage(vhd, pss, "Username can't be emty", "error", "cJoinRoom");
        return 1;
    }

    char *username = malloc(strlen(_username->valuestring));
    if (!username) {
        lwsl_user("OOM: dropping\n");
        tth_sMessage(vhd, pss, "Server error", "error", "cJoinRoom");
        return 1;
    }
    strcpy(username, _username->valuestring);
    
    int time_zone_offset = (int)_time_zone_offset->valuedouble;
    cJSON_Delete(_data);

    // trying to find user with this pss
    struct user_data__tth *puser_pss = NULL;
    struct user_data__tth *puser = NULL;
    lws_start_foreach_llp(struct user_data__tth **, ppud, vhd->user_list) {
        if ((*ppud)->client_id == pss->client_id) {
            puser_pss = *ppud;
        }
        if (!strcmp((*ppud)->username, username)) {
            puser = *ppud;
        }
    } lws_end_foreach_llp(ppud, user_list);

    // if user is already in room
    if (puser_pss) {
        lwsl_user("Failure: client is already in room\n");
        tth_sMessage(vhd, pss, "Client is already in room", "error", "cJoinRoom");
        return 1;
    }

    // creating user if not exists
    if (!puser) {
        puser = malloc(sizeof(struct user_data__tth));
        if (!puser) {
            lwsl_user("OOM: dropping\n");
            tth_sMessage(vhd, pss, "Server error", "error", "cJoinRoom");
            return 1;
        }
        memset(puser, 0, sizeof(struct user_data__tth));
        ___ll_bck_insert(struct user_data__tth, puser, user_list, vhd->user_list);
    }

    // check if data is valid
    if (puser->online) {
        lwsl_user("Failure: user %i is already in room\n", pss->client_id);
        tth_sMessage(vhd, pss, "Username is already in use", "error", "cJoinRoom");
        return 1;
    }

    // marking user online and adding info
    puser->username = username;
    puser->time_zone_offset = time_zone_offset;
    puser->online = 1;
    puser->pss = pss;
    puser->client_id = pss->client_id;

    tth_sPlayerJoined(vhd, pss, username);
    tth_sYouJoined(vhd, pss, puser);

    return 0;
}

int tth_callback_client_leave_room(void *_pss, void *_vhd, char *msg, int len) {
    struct per_session_data__tth *pss = (struct per_session_data__tth *)_pss;
    struct per_vhost_data__tth *vhd = (struct per_vhost_data__tth *)_vhd;
    char *_msg = malloc(len + 1);
    memcpy(_msg, msg, len);
    *(_msg + len) = 0;
    lwsl_user("tth: tth_callback_client_leave_room: \"%s\"\n", _msg);
    free(_msg);

    // finding user with this pss
    struct user_data__tth *puser = NULL;
    lws_start_foreach_llp(struct user_data__tth **, ppud, vhd->user_list) {
        if ((*ppud)->client_id == pss->client_id) {
            puser = *ppud;
            break;
        }
    } lws_end_foreach_llp(ppud, user_list);

    // if no user
    if (!puser) {
        lwsl_user("Failure: no user for this pss\n");
        tth_sMessage(vhd, pss, "You are not in room", "error", "cLeaveRoom");
        return 1;
    }

    // check if data is valid
    if (!puser->online) {
        lwsl_user("Failure: user %i not in room\n", pss->client_id);
        tth_sMessage(vhd, pss, "You are not in room", "error", "cLeaveRoom");
        return 1;
    }
 
    // marking user offline
    puser->online = 0;
    puser->client_id = 0;
    puser->pss = NULL;
    
    tth_sPlayerLeft(vhd, pss, puser->username);

    return 0;
}
