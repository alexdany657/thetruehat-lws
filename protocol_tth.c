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

static char *key = NULL;
static char *port_str = NULL;

int __load_key(char *_key) {
    key = malloc(sizeof(char) * (strlen(_key)+1));
    if (!key) {
        lwsl_user("OOM: dropping\n");
        return 1;
    }
    strcpy(key, _key);
    lwsl_warn("key: %s, %s\n", key, _key);
    return 0;
}

int __destroy_key() {
    free(key);
    return 0;
}

int __load_port(struct lws_vhost *vhost) {
    int port = lws_get_vhost_listen_port(vhost);
    port_str = malloc(sizeof(char) * (snprintf(NULL, 0, "%i", port)+1));
    if (!port_str) {
        lwsl_user("OOM: dropping\n");
        return 1;
    }
    sprintf(port_str, "%i", port);
    return 0;
}

int __destroy_port() {
    free(port_str);
    return 0;
}

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
    free(line);

    vhd->dict_list = new_dict;
    vhd->info->dict = new_dict;

}

static int callback_http(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len) {
    struct per_session_data_http__tth *pss = (struct per_session_data_http__tth *)user;
    char buf[LWS_PRE + 1024], *start = &buf[LWS_PRE], *p = start, *end = &buf[sizeof(buf) - LWS_PRE - 1];
    int n;

    switch (reason) {
        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
            lwsl_err("CLIENT_CONNECTION_ERROR: %s\n", in ? (char *)in : "(null)");
            break;

        case LWS_CALLBACK_CLOSED_CLIENT_HTTP:
            break;

        case LWS_CALLBACK_ESTABLISHED_CLIENT_HTTP:
            pss->status = lws_http_client_http_response(wsi);
            lwsl_user("Connected with server response: %d\n", pss->status);
            break;

        case LWS_CALLBACK_RECEIVE_CLIENT_HTTP_READ:
            lwsl_user("RECEIVE_CLIENT_HTTP_READ: read %d\n", (int)len);
            lwsl_hexdump_notice(in, len);
            return 0;

        case LWS_CALLBACK_RECEIVE_CLIENT_HTTP:
            n = sizeof(buf) - LWS_PRE;
            if (lws_http_client_read(wsi, &p, &n) < 0) {
                return -1;
            }
            return 0;

        case LWS_CALLBACK_COMPLETED_CLIENT_HTTP:
            if (pss->status != 202) {
                lws_cancel_service(lws_get_context(wsi));
            }
            lwsl_user("Successfull auth\n");
            break;

        case LWS_CALLBACK_CLIENT_APPEND_HANDSHAKE_HEADER:
            if (!lws_http_is_redirected_to_get(wsi)) {
                lwsl_user("%s: doing POST flow\n", __func__);
                lws_client_http_body_pending(wsi, 1);
                lws_callback_on_writable(wsi);
            } else {
                lwsl_user("%s: doing GET flow\n", __func__);
            }
            break;

        case LWS_CALLBACK_CLIENT_HTTP_WRITEABLE:
            if (lws_http_is_redirected_to_get(wsi)) {
                        break;
            }
            lwsl_user("LWS_CALLBACK_CLIENT_HTTP_WRITEABLE\n");
            n = LWS_WRITE_HTTP;

            switch (pss->body_part++) {
                case 0:
                    if (lws_client_http_multipart(wsi, "key", NULL, NULL, &p, end)) {
                        return -1;
                    }
                    /* notice every usage of the boundary starts with -- */
                    p += lws_snprintf(p, end - p, key);
                    break;

                case 1:
                    if (lws_client_http_multipart(wsi, "port", NULL, NULL, &p, end)) {
                        return -1;
                    }
                    __load_port(lws_get_vhost(wsi));
                    p += lws_snprintf(p, end - p, port_str);
                    break;

                case 2:
                    if (lws_client_http_multipart(wsi, NULL, NULL, NULL, &p, end)) {
                        return -1;
                    }
                    lws_client_http_body_pending(wsi, 0);
                     /* necessary to support H2, it means we will write no
                      * more on this stream */
                    n = LWS_WRITE_HTTP_FINAL;
                    break;

                default:
                    return 0;
            }

            if (lws_write(wsi, (uint8_t *)start, lws_ptr_diff(p, start), n) != lws_ptr_diff(p, start)) {
                return 1;
            }

            if (n != LWS_WRITE_HTTP_FINAL) {
                lws_callback_on_writable(wsi);
            }

            return 0;

        default:
            break;
    }

    return lws_callback_http_dummy(wsi, reason, user, in, len);
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
            /* printf("%i\n", lws_get_vhost_listen_port(vhd->vhost)); */
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
            vhd->info->settings->dictionary_id = 0;
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
            lwsl_warn("Init finished\n");
            break;

        case LWS_CALLBACK_PROTOCOL_DESTROY:
            {
            int i;
            free(vhd->info->start_time);
            free(vhd->info->end_explanation_time);
            free(vhd->info->end_aftermath_time);
            for (i = 0; i < vhd->info->dict->len; ++i) {
                free(vhd->info->dict->words[i]);
            }
            free(vhd->info->dict->words);
            free(vhd->info->dict);
            free(vhd->info->settings);
            free(vhd->info);
            struct edit_words_data__tth *pedit = vhd->edit_words;
            while (pedit) {
                void *tmp = pedit;
                pedit = pedit->edit_list;
                free(tmp);
            }
            pedit = vhd->words;
            while (pedit) {
                void *tmp = pedit;
                pedit = pedit->edit_list;
                free(tmp);
            }
            struct word_data__tth *pwords = vhd->fresh_words;
            while (pwords) {
                void *tmp = pwords;
                pwords = pwords->word_list;
                free(tmp);
            }
            break;
            }

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
                    void *old_in = in;
                    char fl = 0;
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
                        fl = 1;
                        len = summ_len;
                        struct in_msg *tmp = vhd->in_list;
                        while (tmp) {
                            free(tmp->payload);
                            void *_tmp = tmp;
                            tmp = tmp->in_list;
                            free(_tmp);
                        }
                        vhd->in_list = NULL;
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
                            break;
                        default:
                            // lws_close_reason(wsi, LWS_CLOSE_STATUS_PROTOCOL_ERR, NULL, 0);
                            if (fl) {
                                free(in);
                                in = old_in;
                            }
                            return 0;
                    }

                    if (fl) {
                        free(in);
                        in = old_in;
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
        "http", \
        callback_http, \
        sizeof(struct per_session_data_http__tth), \
        0, \
        0, NULL, 0 \
    }, \
    { \
        "lws-internal", \
        callback_tth, \
        sizeof(struct per_session_data__tth), \
        128, \
        0, NULL, 0 \
    }
