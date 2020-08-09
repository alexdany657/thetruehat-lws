#include "tth_signals.h"

#include "tth_misc.h"

#include "tth_json.h"

#include "cJSON/cJSON.h"
#include <libwebsockets.h>
#include <string.h>
#include <time.h>

/* internal functions */

int __tth_add_dest(struct msg *pmsg, struct per_session_data__tth *pss) {
    struct dest_data__tth *pdest = malloc(sizeof(struct dest_data__tth));
    if (!pdest) {
        lwsl_user("OOM: dropping\n");
        return 1;
    }
    memset(pdest, 0, sizeof(struct dest_data__tth));
    pdest->pss = pss;
    pdest->client_id = pss->client_id;
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
            {
            int cnt = 0;
            lws_start_foreach_llp(struct user_data__tth **, ppud, vhd->user_list) {
                if (cnt == vhd->info->speaker_pos) {
                    __tth_add_dest(pmsg, (*ppud)->pss);
                    ___ll_bck_insert(struct msg, pmsg, msg_list, vhd->msg_list);
                    lws_callback_on_writable((*ppud)->pss->wsi);
                }
                cnt++;
            } lws_end_foreach_llp(ppud, user_list);
            return 0;
            }

        case TTH_DEST_CODE_INVALID:
            return 1;

        default:
            break;
    }

    /*
    lws_start_foreach_llp(struct msg **, ppmsg, vhd->msg_list) {
        lwsl_user("msg: payload: %s\n", (char *)((*ppmsg)->payload + LWS_PRE));
        lws_start_foreach_llp(struct dest_data__tth **, ppdd, (*ppmsg)->dest_list) {
            lwsl_user("dest: client_id %i\n", (*ppdd)->client_id);
        } lws_end_foreach_llp(ppdd, dest_list);
    } lws_end_foreach_llp(ppmsg, msg_list);
    lwsl_user("\n");

    lws_start_foreach_llp(struct per_session_data__tth **, ppss, vhd->pss_list) {
        lwsl_user("pss: client_id: %i\n", (*ppss)->client_id);
    } lws_end_foreach_llp(ppss, pss_list);
    */

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

int tth_sPlayerJoined(void *vhd, void *pss, void *_ausername) {
    char *ausername = (char *)_ausername;

    cJSON *_data = cJSON_CreateObject();
    if (!_data) {
        return 1;
    }

    cJSON *_username = cJSON_CreateString(ausername);
    if (!_username) {
        return 1;
    }
    cJSON_AddItemToObjectCS(_data, "username", _username);

    cJSON *_users = (cJSON *)tth_get_playerList(vhd);
    if (!_users) {
        return 1;
    }
    cJSON_AddItemToObjectCS(_data, "playerList", _users);

    cJSON *_host = (cJSON *)tth_get_host(vhd);
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

int tth_sPlayerLeft(void *vhd, void *pss, void *_ausername) {
    char *ausername = (char *)_ausername;

    cJSON *_data = cJSON_CreateObject();
    if (!_data) {
        return 1;
    }

    cJSON *_username = cJSON_CreateString(ausername);
    if (!_username) {
        return 1;
    }
    cJSON_AddItemToObjectCS(_data, "username", _username);

    cJSON *_users = (cJSON *)tth_get_playerList(vhd);
    if (!_users) {
        return 1;
    }
    cJSON_AddItemToObjectCS(_data, "playerList", _users);

    cJSON *_host = (cJSON *)tth_get_host(vhd);
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

int tth_sYouJoined(void *vhd, void *pss, void *_puser) {
    struct user_data__tth *puser = (struct user_data__tth *)_puser;

    cJSON *_data = cJSON_CreateObject();
    if (!_data) {
        return 1;
    }

    cJSON *_users = (cJSON *)tth_get_playerList(vhd);
    if (!_users) {
        return 1;
    }
    cJSON_AddItemToObjectCS(_data, "playerList", _users);

    cJSON *_host = (cJSON *)tth_get_host(vhd);
    if (!_host) {
        return 1;
    }
    cJSON_AddItemToObjectCS(_data, "host", _host);

    cJSON *_state = (cJSON *)tth_get_state(vhd);
    if (!_state) {
        return 1;
    }
    cJSON_AddItemToObjectCS(_data, "state", _state);

    cJSON *_settings = (cJSON *)tth_get_settings(vhd);
    if (!_settings) {
        return 1;
    }
    cJSON_AddItemToObjectCS(_data, "settings", _settings);

    cJSON *_substate = (cJSON *)tth_get_substate(vhd);
    if (!_substate) {
        return 1;
    }
    cJSON_AddItemToObjectCS(_data, "substate", _substate);

    char *json_msg = cJSON_Print(_data);
    if (!json_msg) {
        return 1;
    }

    __tth_send_signal(vhd, pss, TTH_DEST_CODE_THIS_CLIENT, TTH_CODE_SERVER_YOU_JOINED, json_msg);
    cJSON_Delete(_data);
    cJSON_free(json_msg);

    return 0;
}

int tth_sGameStarted(void *vhd, void *pss) {
    cJSON *_data = cJSON_CreateObject();
    if (!_data) {
        return 1;
    }

    cJSON *_speaker = (cJSON *)tth_get_speaker(vhd);
    if (!_speaker) {
        return 1;
    }
    cJSON_AddItemToObjectCS(_data, "speaker", _speaker);

    cJSON *_listener = (cJSON *)tth_get_listener(vhd);
    if (!_listener) {
        return 1;
    }
    cJSON_AddItemToObjectCS(_data, "listener", _listener);

    cJSON *_words_count = (cJSON *)tth_get_words_count(vhd);
    if (!_words_count) {
        return 1;
    }
    cJSON_AddItemToObjectCS(_data, "wordsCount", _words_count);

    char *json_msg = cJSON_Print(_data);
    if (!json_msg) {
        return 1;
    }

    __tth_send_signal(vhd, pss, TTH_DEST_CODE_ALL, TTH_CODE_SERVER_GAME_STARTED, json_msg);
    cJSON_Delete(_data);
    cJSON_free(json_msg);

    return 0;
}

int tth_sExplanationStarted(void *vhd, void *pss) {
    cJSON *_data = cJSON_CreateObject();
    if (!_data) {
        return 1;
    }

    cJSON *_start_time = (cJSON *)tth_get_start_time(vhd);
    if (!_start_time) {
        return 1;
    }
    cJSON_AddItemToObjectCS(_data, "startTime", _start_time);

    char *json_msg = cJSON_Print(_data);
    if (!json_msg) {
        return 1;
    }

    __tth_send_signal(vhd, pss, TTH_DEST_CODE_ALL, TTH_CODE_SERVER_EXPLANATION_STARTED, json_msg);
    cJSON_Delete(_data);
    cJSON_free(json_msg);

    return 0;
}

int tth_sNewWord(void *vhd, void *pss) {
    cJSON *_data = cJSON_CreateObject();
    if (!_data) {
        return 1;
    }

    char *json_msg = cJSON_Print(_data);
    if (!json_msg) {
        return 1;
    }

    // TODO

    __tth_send_signal(vhd, pss, TTH_DEST_CODE_ALL, TTH_CODE_SERVER_NEW_WORD, json_msg);
    cJSON_Delete(_data);
    cJSON_free(json_msg);

    return 0;
}

int tth_sWordExplanationEnded(void *vhd, void *pss) {
    cJSON *_data = cJSON_CreateObject();
    if (!_data) {
        return 1;
    }

    char *json_msg = cJSON_Print(_data);
    if (!json_msg) {
        return 1;
    }

    // TODO

    __tth_send_signal(vhd, pss, TTH_DEST_CODE_ALL, TTH_CODE_SERVER_WORD_EXPLANATION_ENDED, json_msg);
    cJSON_Delete(_data);
    cJSON_free(json_msg);

    return 0;
}

int tth_sExplanationEnded(void *vhd, void *pss) {
    cJSON *_data = cJSON_CreateObject();
    if (!_data) {
        return 1;
    }

    cJSON *_words_count = (cJSON *)tth_get_words_count(vhd);
    if (!_words_count) {
        return 1;
    }
    cJSON_AddItemToObjectCS(_data, "wordsCount", _words_count);

    char *json_msg = cJSON_Print(_data);
    if (!json_msg) {
        return 1;
    }

    __tth_send_signal(vhd, pss, TTH_DEST_CODE_ALL, TTH_CODE_SERVER_EXPLANATION_ENDED, json_msg);
    cJSON_Delete(_data);
    cJSON_free(json_msg);

    return 0;
}

int tth_sWordsToEdit(void *vhd, void *pss) {
    cJSON *_data = cJSON_CreateObject();
    if (!_data) {
        return 1;
    }
    
    char *json_msg = cJSON_Print(_data);
    if (!json_msg) {
        return 1;
    }
    
    // TODO

    __tth_send_signal(vhd, pss, TTH_DEST_CODE_ALL, TTH_CODE_SERVER_WORD_EXPLANATION_ENDED, json_msg);
    cJSON_Delete(_data);
    cJSON_free(json_msg);

    return 0;
}

int tth_sNextTurn(void *vhd, void *pss) {
    cJSON *_data = cJSON_CreateObject();
    if (!_data) {
        return 1;
    }

    char *json_msg = cJSON_Print(_data);
    if (!json_msg) {
        return 1;
    }

    // TODO

    __tth_send_signal(vhd, pss, TTH_DEST_CODE_ALL, TTH_CODE_SERVER_NEXT_TURN, json_msg);
    cJSON_Delete(_data);
    cJSON_free(json_msg);

    return 0;
}

int tth_sGameEnded(void *vhd, void *pss) {
    cJSON *_data = cJSON_CreateObject();
    if (!_data) {
        return 1;
    }

    char *json_msg = cJSON_Print(_data);
    if (!json_msg) {
        return 1;
    }

    // TODO

    __tth_send_signal(vhd, pss, TTH_DEST_CODE_ALL, TTH_CODE_SERVER_GAME_ENDED, json_msg);
    cJSON_Delete(_data);
    cJSON_free(json_msg);

    return 0;
}
