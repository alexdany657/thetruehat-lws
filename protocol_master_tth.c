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

const timeval MAS_RECONNECTION_TIMEOUT = {10, 0};

struct per_session_data__mas {
    struct per_session_data__mas *pss_list;
    struct lws *wsi;
    struct lws_client_connect_info i;
    struct lws *client_wsi;
};

struct per_vhost_data__mas {
    struct lws_context *context;
    struct lws_vhost *vhost;
    const struct lws_protocols *protocol;

    struct per_session_data__mas *pss_list; /* linked-list of live pss*/
};

struct reconnection_handler_args_mas {
    void *vhd;
    void *pss;
};

void reconnection_handler_mas(void *_args);

static void connection_attempt_mas(void *_vhd, void *_pss) {
    struct per_vhost_data__mas *vhd = (struct per_vhost_data__mas *)_vhd;
    struct per_session_data__mas *pss = (struct per_session_data__mas *)_pss;

    pss->i.context = vhd->context;
    pss->i.port = 2005;
    pss->i.address = "localhost";
    pss->i.path = "/";
    pss->i.host = pss->i.address;
    pss->i.orogin = pss->i.address;
    pss->i.ssl_connection = 0;

    pss->i.protocol = "lws-internal";
    pss->i.local_protocol_name = "lws-tth";
    pss->i.pwsi = &pss->client_wsi;

    struct reconnection_handler_args_mas *args = malloc(sizeof(struct reconnection_handler_args_mas));
    args->pss = pss;
    args->vhd = vhd;

    if (!lws_client_connect_via_info(&pss->i)) {
        tth_set_timeout(&MAS_RECONNECTION_TIMEOUT, reconnection_handler_mas, args);
    }
}

void reconnection_handler_mas(void *_args) {
    struct reconnection_handler_args_mas *args = (struct reconnection_handler_args_mas *)_args;
    connection_attempt_mas(args->vhd, args->pss);
}

static int callback_tth(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len) {
    struct per_session_data__mas*pss = (struct per_session_data__mas *)user;
    struct per_vhost_data__mas *vhd = (struct per_vhost_data__mas *)lws_protocol_vh_priv_get(lws_get_vhost(wsi), lws_get_protocol(wsi));

    switch (reason) {
        case LWS_CALLBACK_PROTOCOL_INIT:
            vhd = lws_protocol_vh_priv_zalloc(lws_get_vhost(wsi), lws_get_protocol(wsi), sizeof(struct per_vhost_data__mas));
            vhd->context = lws_get_context(wsi);
            vhd->protocol = lws_get_protocol(wsi);
            vhd->vhost = lws_get_vhost(wsi);
            break;

        case LWS_CALLBACK_ESTABLISHED:
            /* add ourselves to the list of live pss held in the vhd */
            lws_ll_fwd_insert(pss, pss_list, vhd->pss_list);
            pss->wsi = wsi;
            /* open connection to backend and bind it */
            connection_attempt_mas(vhd, pss);
            lwsl_user("initial wsi: %p, backend wsi: %p\n", wsi, pss->client_wsi);
            break;

        case LWS_CALLBACK_CLOSED:
            lwsl_user("pss: close: wsi: %p\n", pss->wsi);
            /* remove our closing pss from the list of live pss */
            lws_close_reason(pss->client_wsi, LWS_CLOSE_STATUS_NORMAL, NULL, 0); /* TODO: check if really closes the connection */
            lws_ll_fwd_remove(struct per_session_data__tth, pss_list, pss, vhd->pss_list);
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
        
        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
            lwsl_err("CLIENT_CONNECTION_ERROR: %s\n", in ? (char *)in : "(null)");
            vhd->client_wsi = NULL;
            connection_attempt_mas(vhd, pss);
            break;


        case LWS_CALLBACK_CLIENT_ESTABLISHED:
            lwsl_user("%s: established\n", __func__);
            break;

        case LWS_CALLBACK_CLIENT_CLOSED:
            vhd->client_wsi = NULL;
            lws_sul_schedule(vhd->context, 0, &vhd->sul,
                     sul_connect_attempt, LWS_US_PER_SEC);
            break;
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
