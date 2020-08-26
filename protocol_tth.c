/**
 * some code taken from official lws example "lws-minimal": https://libwebsockets.org/git/libwebsockets/tree/minimal-examples/ws-server/minimal-ws-server?h=v4.0-stable
 */

#if !defined (LWS_PLUGIN_STATIC)
#define LWS_DLL
#define LWS_INTERNAL
#include <libwebsockets.h>
#endif

#include <string.h>
#include <stdio.h>

#include "tth_structs.h"
#include "tth_misc.h"

#include "tth_callbacks.h"
#include "tth_signals.h"
#include "tth_codes.h"

#define TRANSPORT_DELAY 1000

void __load_dict(void *_vhd) {
    struct per_vhost_data__tth *vhd = (struct per_vhost_data__tth *)_vhd;

    struct dict_data__tth *new_dict = malloc(sizeof(struct dict_data__tth));
    if (!new_dict) {
        lwsl_user("OOM: dropping\n");
        abort();
    }
    new_dict->dict_list = NULL;
    new_dict->name = "dict.1";

    FILE *fdict = fopen("dict.1", "r");
    if (!fdict) {
        lwsl_err("error reading dictionary from file\n");
        abort();
    }

    int n;
    ssize_t nread;
    char *line = NULL;
    size_t len = 0;
    int cnt = 0;
    nread = getline(&line, &len, fdict);
    n = atoi(line);
    new_dict->len = n;
    new_dict->words = malloc(n * sizeof(char *));
    nread = getline(&line, &len, fdict);
    while (nread != -1 && cnt < n) {
        int _len = 0;
        while (*(line + _len)) {
            _len++;
        }
        if (_len > 1 && *(line + _len - 2) == '\n') {
            _len--;
        }
        char *_line = malloc(_len * sizeof(char));
        strncpy(_line, line, _len);
        if (_len > 0) {
            *(_line + _len - 1) = 0;
        }
        *(new_dict->words+cnt) = _line;
        nread = getline(&line, &len, fdict);
        cnt++;
    }
    while (nread != -1) {
        nread = getline(&line, &len, fdict);
    }
    fclose(fdict);

    vhd->dict_list = new_dict;
    vhd->info->dict = new_dict;

}

static int callback_tth(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len) {
    struct per_session_data__tth *pss = (struct per_session_data__tth *)user;
    struct per_vhost_data__tth *vhd = (struct per_vhost_data__tth *)lws_protocol_vh_priv_get(lws_get_vhost(wsi), lws_get_protocol(wsi));

    switch (reason) {
        case LWS_CALLBACK_PROTOCOL_INIT:
            vhd = lws_protocol_vh_priv_zalloc(lws_get_vhost(wsi), lws_get_protocol(wsi), sizeof(struct per_vhost_data__tth));
            vhd->context = lws_get_context(wsi);
            vhd->protocol = lws_get_protocol(wsi);
            vhd->vhost = lws_get_vhost(wsi);
            vhd->clientCnt = 1;
            vhd->msg_list = NULL;
            vhd->in_list = NULL;
            vhd->info = malloc(sizeof(struct info__tth));
            if (!vhd->info) {
                lwsl_user("OOM: dropping\n");
                return 1;
            }
            memset(vhd->info, 0, sizeof(struct info__tth));
            vhd->info->settings = malloc(sizeof(struct settings__tth));
            if (!vhd->info->settings) {
                lwsl_user("OOM: dropping\n");
                return 1;
            }

            vhd->info->state = TTH_STATE_WAIT;
            vhd->info->substate = TTH_SUBSTATE_UNDEFINED;
            vhd->info->transport_delay = TRANSPORT_DELAY;
            vhd->info->settings->word_number = 8;
            vhd->info->settings->delay_time = 3000;
            vhd->info->settings->explanation_time = 40000;
            vhd->info->settings->aftermath_time = 3000;
            vhd->info->settings->strict_mode = 0;
            vhd->info->dict = NULL;
            vhd->info->start_time = malloc(sizeof(struct timeval));
            if (!vhd->info->start_time) {
                lwsl_user("OOM: dropping\n");
                return 1;
            }
            vhd->info->end_explanation_time = malloc(sizeof(struct timeval));
            if (!vhd->info->end_explanation_time) {
                lwsl_user("OOM: dropping\n");
                return 1;
            }
            vhd->info->end_aftermath_time = malloc(sizeof(struct timeval));
            if (!vhd->info->end_aftermath_time) {
                lwsl_user("OOM: dropping\n");
                return 1;
            }
            vhd->fresh_words = NULL;
            vhd->edit_words = NULL;
            vhd->words = NULL;
            __load_dict(vhd);
            break;

        case LWS_CALLBACK_ESTABLISHED:
            /* add ourselves to the list of live pss held in the vhd */
            lws_ll_fwd_insert(pss, pss_list, vhd->pss_list);
            pss->wsi = wsi;
            lwsl_user("initial wsi: %p\n", wsi);
            pss->client_id = vhd->clientCnt++;
            break;

        case LWS_CALLBACK_CLOSED:
            lwsl_user("pss: close: client_id %i\n", pss->client_id);
            /* remove our closing pss from the list of live pss */
            lws_ll_fwd_remove(struct per_session_data__tth, pss_list, pss, vhd->pss_list);

            /* if this client has logged in user --- we need to make it offline */
            struct user_data__tth *puser = NULL;
            lws_start_foreach_llp(struct user_data__tth **, ppud, vhd->user_list) {
                if ((*ppud)->client_id == pss->client_id) {
                    puser = *ppud;
                    break;
                }
            } lws_end_foreach_llp(ppud, user_list);
            if (puser) {
                puser->online = 0;
                puser->pss = NULL;
                tth_sPlayerLeft(vhd, pss, puser->username);
            }
            break;

        case LWS_CALLBACK_SERVER_WRITEABLE:
            {
                int8_t writed = 0;
                struct msg *pmsg_del = NULL;
                lws_start_foreach_llp(struct msg **, ppmsg, vhd->msg_list) {
                    int8_t shouldDelete = 1;
                    // lwsl_warn("msg:\npss client_id:%i\npayload: %s\n", pss->client_id, (char *)((*ppmsg)->payload + LWS_PRE));
                    lws_start_foreach_llp(struct dest_data__tth **, ppdd, (*ppmsg)->dest_list) {
                        // lwsl_warn("dest client_id: %i\n", (*ppdd)->client_id);
                        if (!((*ppdd)->sent)) {
                            if ((*ppdd)->client_id == pss->client_id) {
                                if (writed) {
                                    // lwsl_warn("\n");
                                    break;
                                }
                                // sending message
                                int m;
                                m = lws_write(wsi, ((unsigned char *)(*ppmsg)->payload) + LWS_PRE, (*ppmsg)->len, LWS_WRITE_TEXT);
                                if (m < (int)(*ppmsg)->len) {
                                    lwsl_err("ERROR %d writing to ws\n", m);
                                    return -1;
                                }
                                writed = 1;
                                (*ppdd)->sent = 1;
                            } else {
                                shouldDelete = 0;
                            }
                        }
                    } lws_end_foreach_llp(ppdd, dest_list);
                    if (shouldDelete) {
                        pmsg_del = *ppmsg;
                    }
                    // lwsl_warn("\n");
                    if (writed) {
                        lws_callback_on_writable(wsi);
                        break;
                    }
                } lws_end_foreach_llp(ppmsg, msg_list);

                if (pmsg_del) {
                    // lwsl_warn("deleting msg:\n");
                    lws_ll_fwd_remove(struct msg, msg_list, pmsg_del, vhd->msg_list);
                    free(pmsg_del->payload);
                    while (pmsg_del->dest_list) {
                        struct dest_data__tth *pdest_del = pmsg_del->dest_list;
                        // lwsl_warn("dest: client_id: %i, sent: %i\n", pdest_del->client_id, pdest_del->sent);
                        lws_ll_fwd_remove(struct dest_data__tth, dest_list, pdest_del, pmsg_del->dest_list);
                        free(pdest_del);
                    }
                    free(pmsg_del);
                }

                break;
            }

        case LWS_CALLBACK_RECEIVE:
            {
                //lwsl_warn("%i %i\n", lws_is_first_fragment(wsi), lws_is_final_fragment(wsi));
                int first, final;
                first = lws_is_first_fragment(wsi);
                final = lws_is_final_fragment(wsi);
                /* lwsl_warn("msg: %*.*s, len: %i, is_first: %i, is_final %i\n", (int)len, (int)len, (char *)in, (int)len, first, final); */
                
                if (!(first && final)) {
                    struct in_msg *curr_msg = malloc(sizeof(struct in_msg));
                    if (!curr_msg) {
                        lwsl_user("OOM: dropping\n");
                        return 1;
                    }
                    curr_msg->in_list = NULL;
                    curr_msg->payload = malloc(len);
                    if (!curr_msg->payload) {
                        lwsl_user("OOM: dropping\n");
                        return 1;
                    }
                    memcpy(curr_msg->payload, in, len);
                    curr_msg->len = len;
                    ___ll_bck_insert(struct in_msg, curr_msg, in_list, vhd->in_list);
                }
                if (final) {
                    if (!first) {
                        int summ_len = 0;
                        lws_start_foreach_llp(struct in_msg **, ppim, vhd->in_list) {
                            summ_len += (*ppim)->len;
                        } lws_end_foreach_llp(ppim, in_list);
                        char *all_payload = malloc(summ_len);
                        if (!all_payload) {
                            lwsl_user("OOM: dropping\n");
                            return 1;
                        }
                        summ_len = 0;
                        lws_start_foreach_llp(struct in_msg **, ppim, vhd->in_list) {
                            memcpy(all_payload + summ_len, (*ppim)->payload, (*ppim)->len);
                            summ_len += (*ppim)->len;
                        } lws_end_foreach_llp(ppim, in_list);
                        in = all_payload;
                        len = summ_len;
                        struct in_msg *tmp = vhd->in_list;
                        while (tmp) {
                            free(tmp->payload);
                            void *_tmp = tmp;
                            tmp = tmp->in_list;
                            free(_tmp);
                        }
                    }

                    enum tth_code code;

                    code = tth_get_code(in, len);

                    if (code < 0) {
                        tth_callback_error(vhd, pss, in, len);
                        // lws_close_reason(wsi, LWS_CLOSE_STATUS_PROTOCOL_ERR, NULL, 0);
                        return 0;
                    } else if (code < TTH_CODE_ENUM_CLIENT_ADD) {
                        tth_callback_server(vhd, pss, code, in, len);
                        // lws_close_reason(wsi, LWS_CLOSE_STATUS_PROTOCOL_ERR, NULL, 0);
                        return 0;
                    }

                    int new_len = len - 2;
                    char *new_in = in + 2;

                    switch (code) {
                        case TTH_CODE_CLIENT_JOIN_ROOM:
                            tth_callback_client_join_room(vhd, pss, new_in, new_len);
                            break;
                        case TTH_CODE_CLIENT_LEAVE_ROOM:
                            tth_callback_client_leave_room(vhd, pss);
                            break;
                        case TTH_CODE_CLIENT_START_GAME:
                            tth_callback_client_start_game(vhd, pss);
                            break;
                        case TTH_CODE_CLIENT_SPEAKER_READY:
                            tth_callback_client_speaker_ready(vhd, pss);
                            break;
                        case TTH_CODE_CLIENT_LISTENER_READY:
                            tth_callback_client_listener_ready(vhd, pss);
                            break;
                        case TTH_CODE_CLIENT_END_WORD_EXPLANATION:
                            tth_callback_client_end_word_explanation(vhd, pss, new_in, new_len);
                            break;
                        case TTH_CODE_CLIENT_WORDS_EDITED:
                            tth_callback_client_words_edited(vhd, pss, new_in, new_len);
                            break;
                        case TTH_CODE_CLIENT_PING:
                            tth_callback_client_ping(vhd, pss);
                        default:
                            // lws_close_reason(wsi, LWS_CLOSE_STATUS_PROTOCOL_ERR, NULL, 0);
                            return 0;
                    }

                    break;
                }
            }
        default:
            break;
    }

    return 0;
}

#define LWS_PLUGIN_PROTOCOL_TTH \
    { \
        "lws-tth", \
        callback_tth, \
        sizeof(struct per_session_data__tth), \
        128, \
        0, NULL, 0 \
    }
