#include "tth_misc.h"

#include "tth_signals.h"

#include "cJSON/cJSON.h"
#include <libwebsockets.h>
#include <string.h>

/* internal functions */

int __tth_add_dest(struct msg *pmsg, struct per_session_data__tth *pss) {
    struct dest_data__tth *pdest = malloc(sizeof(struct dest_data__tth));
    if (!pdest) {
        lwsl_user("OOM: dropping\n");
        return 1;
    }
    memset(pdest, 0, sizeof(struct dest_data__tth));
    pdest->pss = pss;
    pdest->clientId = pss->clientId;
    lws_ll_fwd_insert(pdest, dest_list, pmsg->dest_list);
    return 0;
}

int __tth_send_signal(void *_vhd, void *_pss, enum tth_dest_code dest_code, enum tth_code code, void *_amsg) {
    struct per_vhost_data__tth *vhd = (struct per_vhost_data__tth *)_vhd;
    struct per_session_data__tth *pss = (struct per_session_data__tth *)_pss;
    char *amsg = (char *)_amsg;
    int len = strlen(amsg) + 2; /* 2 digits for code and LWS_PRE*/

    struct msg *pmsg = malloc(sizeof(struct msg));

    if (!pmsg) {
        lwsl_user("OOM: dropping\n");
        return 1;
    }
    memset(pmsg, 0, sizeof(struct msg));

    pmsg->len = len;
    pmsg->payload = malloc(len + LWS_PRE);
    if (!pmsg->payload) {
        lwsl_user("OOM: dropping\n");
        return 1;
    }
    tth_add_code(pmsg->payload + LWS_PRE, code);
    memcpy(pmsg->payload + 2 + LWS_PRE, amsg, len - 2);

    switch (dest_code) {
        case TTH_DEST_CODE_THIS_CLIENT:
            __tth_add_dest(pmsg, pss);
            ___ll_bck_insert(struct msg, pmsg, msg_list, vhd->msg_list);
            lws_callback_on_writable(pss->wsi);
            break;

        case TTH_DEST_CODE_ALL:
            lws_start_foreach_llp(struct per_session_data__tth **, ppss, vhd->pss_list) {
                __tth_add_dest(pmsg, *ppss);
            } lws_end_foreach_llp(ppss, pss_list);
            ___ll_bck_insert(struct msg, pmsg, msg_list, vhd->msg_list);
            lws_callback_on_writable_all_protocol_vhost(vhd->vhost, vhd->protocol);
            break;

        case TTH_DEST_CODE_HOST:
            lws_start_foreach_llp(struct user_data__tth **, ppud, vhd->user_list) {
                if ((*ppud)->online) {
                    __tth_add_dest(pmsg, (*ppud)->pss);
                    ___ll_bck_insert(struct msg, pmsg, msg_list, vhd->msg_list);
                    lws_callback_on_writable((*ppud)->pss->wsi);
                    break;
                }
            } lws_end_foreach_llp(ppud, user_list);
            break;

        case TTH_DEST_CODE_SPEAKER:
            /* TODO: implement */
            return 1;

        case TTH_DEST_CODE_INVALID:
            return 1;

        default:
            break;
    }

    lws_start_foreach_llp(struct msg **, ppmsg, vhd->msg_list) {
        lwsl_user("msg: payload: %s\n", (char *)((*ppmsg)->payload + LWS_PRE));
        lws_start_foreach_llp(struct dest_data__tth **, ppdd, (*ppmsg)->dest_list) {
            lwsl_user("dest: clientId %i\n", (*ppdd)->clientId);
        } lws_end_foreach_llp(ppdd, dest_list);
    } lws_end_foreach_llp(ppmsg, msg_list);
    lwsl_user("\n");

    lws_start_foreach_llp(struct per_session_data__tth **, ppss, vhd->pss_list) {
        lwsl_user("pss: clientId: %i\n", (*ppss)->clientId);
    } lws_end_foreach_llp(ppss, pss_list);

    return 0;
}

/* external functions */

int tth_sMessage(void *vhd, void *pss, void *_amsg, void *_aseverity, void *_asignal) {
    char *amsg = (char *)_amsg;
    char *aseverity = (char *)_aseverity;
    char *asignal = (char *)_asignal;

    cJSON *_data = cJSON_CreateObject();
    if (!_data) {
        return 1;
    }
    
    cJSON *_msg = cJSON_CreateString(amsg);
    if (!_msg) {
        return 1;
    }

    cJSON *_severity = cJSON_CreateString(aseverity);
    if (!_severity) {
        return 1;
    }

    cJSON *_signal = cJSON_CreateString(asignal);
    if (!_signal) {
        return 1;
    }

    cJSON_AddItemToObjectCS(_data, "msg", _msg);
    cJSON_AddItemToObjectCS(_data, "severity", _severity);
    cJSON_AddItemToObjectCS(_data, "signal", _signal);

    char *json_msg = cJSON_Print(_data);
    if (!json_msg) {
        return 1;
    }
    __tth_send_signal(vhd, pss, TTH_DEST_CODE_THIS_CLIENT, TTH_CODE_SERVER_MESSAGE, json_msg);
    cJSON_Delete(_data);
    cJSON_free(json_msg);

    return 0;
}

int tth_sPlayerJoined(void *_vhd, void *pss, void *_ausername) {
    char *ausername = (char *)_ausername;
    struct per_vhost_data__tth *vhd = (struct per_vhost_data__tth *)_vhd;

    cJSON *_data = cJSON_CreateObject();
    if (!_data) {
        return 1;
    }

    cJSON *_username = cJSON_CreateString(ausername);
    if (!_username) {
        return 1;
    }
    cJSON_AddItemToObjectCS(_data, "username", _username);

    cJSON *_users = cJSON_CreateArray();
    if (!_users) {
        return 1;
    }
    cJSON_AddItemToObjectCS(_data, "playerList", _users);

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
        return 1;
    }
    cJSON_AddItemToObjectCS(_data, "host", _host);

    char *json_msg = cJSON_Print(_data);
    if (!json_msg) {
        return 1;
    }
    __tth_send_signal(vhd, pss, TTH_DEST_CODE_ALL, TTH_CODE_SERVER_PLAYER_JOINED, json_msg);
    cJSON_Delete(_data);
    cJSON_free(json_msg);

    return 0;
}

int tth_sPlayerLeft(void *_vhd, void *pss, void *_ausername) {
    char *ausername = (char *)_ausername;
    struct per_vhost_data__tth *vhd = (struct per_vhost_data__tth *)_vhd;

    cJSON *_data = cJSON_CreateObject();
    if (!_data) {
        return 1;
    }

    cJSON *_username = cJSON_CreateString(ausername);
    if (!_username) {
        return 1;
    }
    cJSON_AddItemToObjectCS(_data, "username", _username);

    cJSON *_users = cJSON_CreateArray();
    if (!_users) {
        return 1;
    }
    cJSON_AddItemToObjectCS(_data, "playerList", _users);

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
        return 1;
    }
    cJSON_AddItemToObjectCS(_data, "host", _host);

    char *json_msg = cJSON_Print(_data);
    if (!json_msg) {
        return 1;
    }
    __tth_send_signal(vhd, pss, TTH_DEST_CODE_ALL, TTH_CODE_SERVER_PLAYER_LEFT, json_msg);
    cJSON_Delete(_data);
    cJSON_free(json_msg);

    return 0;
}
