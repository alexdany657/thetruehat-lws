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

#include "tth_misc.h"

/* one of these is created for each message fragment we receive */

struct msg__mas {
    struct msg__mas *msg_list;
    char *payload;
    int len;
};

/* one of these is created for each client connecting to us */

struct per_session_data__mas {
    struct per_session_data__mas *pss_list;
    struct lws *wsi;
    struct lws_client_connect_info i;
    struct lws *client_wsi;
    struct msg__mas *from_msg_frag_list; /* list of recieved fragments from real client */
    struct msg__mas *to_msg_frag_list; /* list of recieved fragments to real client */
    struct msg__mas *from_msg_list; /* list of messages recieved from real client (need to be sent to backend) */
    struct msg__mas *to_msg_list; /* list of messages recieved from backend (need to be sent to real client) */
};

/* one of these is created for each vhost our protocol is used with */

struct per_vhost_data__mas {
    struct lws_context *context;
    struct lws_vhost *vhost;
    const struct lws_protocols *protocol;

    struct per_session_data__mas *pss_list; /* linked-list of live pss*/
};

static void connection_attempt_mas(void *_vhd, void *_pss) {
    struct per_vhost_data__mas *vhd = (struct per_vhost_data__mas *)_vhd;
    struct per_session_data__mas *pss = (struct per_session_data__mas *)_pss;

    pss->i.context = vhd->context;
    pss->i.port = 2005;
    pss->i.address = "localhost";
    pss->i.path = "/";
    pss->i.host = pss->i.address;
    pss->i.origin = pss->i.address;
    pss->i.ssl_connection = 0;

    pss->i.protocol = "lws-internal";
    pss->i.local_protocol_name = "lws-tth";
    pss->i.pwsi = &pss->client_wsi;

    if (!lws_client_connect_via_info(&pss->i) && pss->wsi) {
        lws_close_reason(pss->wsi, LWS_CLOSE_STATUS_NORMAL, NULL, 0);
    }
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
            pss->from_msg_frag_list = NULL;
            pss->to_msg_frag_list = NULL;
            pss->from_msg_list = NULL;
            pss->to_msg_list = NULL;
            /* open connection to backend and bind it */
            connection_attempt_mas(vhd, pss);
            lwsl_user("initial wsi: %p, backend wsi: %p\n", wsi, pss->client_wsi);
            break;

        case LWS_CALLBACK_CLOSED:
            lwsl_user("pss: close: wsi: %p\n", pss->wsi);
            if (pss->client_wsi) {
                lws_close_reason(pss->client_wsi, LWS_CLOSE_STATUS_NORMAL, NULL, 0); /* TODO: check if really closes the connection */
            }
            /* remove our closing pss from the list of live pss */
            lws_ll_fwd_remove(struct per_session_data__mas, pss_list, pss, vhd->pss_list);
            break;

        case LWS_CALLBACK_SERVER_WRITEABLE:
            {
                lwsl_warn("LWS_CALLBACK_SERVER_WRITEABLE\n");
                if (!pss->to_msg_list) {
                    return 0;
                }
                struct msg__mas *tmp = pss->to_msg_list;
                int m;
                if (!tmp->msg_list) {
                    m = lws_write(pss->wsi, ((unsigned char *)tmp->payload) + LWS_PRE, tmp->len, LWS_WRITE_TEXT);
                    if (m < (int)tmp->len) {
                        lwsl_err("ERROR %d writing to ws socket\n", m);
                        return -1;
                    }
                    free(tmp);
                    pss->to_msg_list = NULL;
                    return 0;
                }
                struct msg__mas *last = tmp->msg_list;
                while (last->msg_list) {
                    tmp = tmp->msg_list;
                    last = tmp->msg_list;
                }
                m = lws_write(pss->wsi, ((unsigned char *)last->payload) + LWS_PRE, last->len, LWS_WRITE_TEXT);
                if (m < (int)last->len) {
                    lwsl_err("ERROR %d writing to ws socket\n", m);
                    return -1;
                }
                free(last);
                tmp->msg_list = NULL;
                lws_callback_on_writable(pss->wsi);
            }
            break;

        case LWS_CALLBACK_RECEIVE:
            {
                lwsl_warn("LWS_CALLBACK_RECEIVE\n");
                //lwsl_warn("%i %i\n", lws_is_first_fragment(wsi), lws_is_final_fragment(wsi));
                int first, final;
                first = lws_is_first_fragment(wsi);
                final = lws_is_final_fragment(wsi);
                /* lwsl_warn("msg: %*.*s, len: %i, is_first: %i, is_final %i\n", (int)len, (int)len, (char *)in, (int)len, first, final); */
                
                if (!(first && final)) {
                    struct msg__mas *curr_msg = malloc(sizeof(struct msg__mas)); /* Not really a message, it's a fragment*/
                    if (!curr_msg) {
                        lwsl_user("OOM: dropping\n");
                        return 1;
                    }
                    curr_msg->msg_list = NULL;
                    curr_msg->payload = malloc(len);
                    if (!curr_msg->payload) {
                        lwsl_user("OOM: dropping\n");
                        return 1;
                    }
                    memcpy(curr_msg->payload, in, len);
                    curr_msg->len = len;
                    ___ll_bck_insert(struct msg__mas, curr_msg, msg_list, pss->from_msg_frag_list);
                }
                if (final) {
                    if (!first) {
                        int summ_len = 0;
                        lws_start_foreach_llp(struct msg__mas **, ppmf, pss->from_msg_frag_list) {
                            summ_len += (*ppmf)->len;
                        } lws_end_foreach_llp(ppmf, msg_list);
                        char *all_payload = malloc(summ_len);
                        if (!all_payload) {
                            lwsl_user("OOM: dropping\n");
                            return 1;
                        }
                        summ_len = 0;
                        lws_start_foreach_llp(struct msg__mas **, ppmf, pss->from_msg_frag_list) {
                            memcpy(all_payload + summ_len, (*ppmf)->payload, (*ppmf)->len);
                            summ_len += (*ppmf)->len;
                        } lws_end_foreach_llp(ppmf, msg_list);
                        in = all_payload;
                        len = summ_len;
                        struct msg__mas *tmp = pss->from_msg_frag_list;
                        while (tmp) {
                            free(tmp->payload);
                            void *_tmp = tmp;
                            tmp = tmp->msg_list;
                            free(_tmp);
                        }
                    }
                    struct msg__mas *cur_msg = malloc(sizeof(struct msg__mas));
                    if (!cur_msg) {
                        lwsl_user("OOM: dropping\n");
                        return 1;
                    }
                    memset(cur_msg, 0, sizeof(struct msg__mas));
                    cur_msg->payload = malloc(len + LWS_PRE);
                    if (!cur_msg->payload) {
                        lwsl_user("OOM: dropping\n");
                        return 1;
                    }
                    memcpy(cur_msg->payload + LWS_PRE, in, len);
                    cur_msg->len = len;
                    ___ll_bck_insert(struct msg__mas, cur_msg, msg_list, pss->from_msg_list);
                    lws_callback_on_writable(pss->client_wsi);
                }
            }
            break;
        
        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
            lwsl_err("CLIENT_CONNECTION_ERROR: %s\n", in ? (char *)in : "(null)");
            pss->client_wsi = NULL;
            if (pss->wsi) {
                lws_close_reason(pss->wsi, LWS_CLOSE_STATUS_NORMAL, NULL, 0);
            }
            break;

        case LWS_CALLBACK_CLIENT_ESTABLISHED:
            lwsl_user("%s: established\n", __func__);
            break;

        case LWS_CALLBACK_CLIENT_WRITEABLE:
            {
                lwsl_warn("LWS_CALLBACK_CLIENT_WRITEABLE\n");
                if (!pss->from_msg_list) {
                    return 0;
                }
                struct msg__mas *tmp = pss->from_msg_list;
                int m;
                if (!tmp->msg_list) {
                    m = lws_write(pss->client_wsi, ((unsigned char *)tmp->payload) + LWS_PRE, tmp->len, LWS_WRITE_TEXT);
                    if (m < (int)tmp->len) {
                        lwsl_err("ERROR %d writing to ws socket\n", m);
                        return -1;
                    }
                    free(tmp);
                    pss->from_msg_list = NULL;
                    return 0;
                }
                struct msg__mas *last = tmp->msg_list;
                while (last->msg_list) {
                    tmp = tmp->msg_list;
                    last = tmp->msg_list;
                }
                m = lws_write(pss->client_wsi, ((unsigned char *)last->payload) + LWS_PRE, last->len, LWS_WRITE_TEXT);
                if (m < (int)last->len) {
                    lwsl_err("ERROR %d writing to ws socket\n", m);
                    return -1;
                }
                free(last);
                tmp->msg_list = NULL;
                lws_callback_on_writable(pss->client_wsi);
            }
            break;

        case LWS_CALLBACK_CLIENT_RECEIVE:
            {
                lwsl_warn("LWS_CALLBACK_CLIENT_RECEIVE\n");
                //lwsl_warn("%i %i\n", lws_is_first_fragment(wsi), lws_is_final_fragment(wsi));
                int first, final;
                first = lws_is_first_fragment(wsi);
                final = lws_is_final_fragment(wsi);
                /* lwsl_warn("msg: %*.*s, len: %i, is_first: %i, is_final %i\n", (int)len, (int)len, (char *)in, (int)len, first, final); */
                
                if (!(first && final)) {
                    struct msg__mas *curr_msg = malloc(sizeof(struct msg__mas)); /* Not really a message, it's a fragment*/
                    if (!curr_msg) {
                        lwsl_user("OOM: dropping\n");
                        return 1;
                    }
                    curr_msg->msg_list = NULL;
                    curr_msg->payload = malloc(len);
                    if (!curr_msg->payload) {
                        lwsl_user("OOM: dropping\n");
                        return 1;
                    }
                    memcpy(curr_msg->payload, in, len);
                    curr_msg->len = len;
                    ___ll_bck_insert(struct msg__mas, curr_msg, msg_list, pss->to_msg_frag_list);
                }
                if (final) {
                    if (!first) {
                        int summ_len = 0;
                        lws_start_foreach_llp(struct msg__mas **, ppmf, pss->to_msg_frag_list) {
                            summ_len += (*ppmf)->len;
                        } lws_end_foreach_llp(ppmf, msg_list);
                        char *all_payload = malloc(summ_len);
                        if (!all_payload) {
                            lwsl_user("OOM: dropping\n");
                            return 1;
                        }
                        summ_len = 0;
                        lws_start_foreach_llp(struct msg__mas **, ppmf, pss->to_msg_frag_list) {
                            memcpy(all_payload + summ_len, (*ppmf)->payload, (*ppmf)->len);
                            summ_len += (*ppmf)->len;
                        } lws_end_foreach_llp(ppmf, msg_list);
                        in = all_payload;
                        len = summ_len;
                        struct msg__mas *tmp = pss->to_msg_frag_list;
                        while (tmp) {
                            free(tmp->payload);
                            void *_tmp = tmp;
                            tmp = tmp->msg_list;
                            free(_tmp);
                        }
                    }
                    struct msg__mas *cur_msg = malloc(sizeof(struct msg__mas));
                    if (!cur_msg) {
                        lwsl_user("OOM: dropping\n");
                        return 1;
                    }
                    memset(cur_msg, 0, sizeof(struct msg__mas));
                    cur_msg->payload = malloc(len + LWS_PRE);
                    if (!cur_msg->payload) {
                        lwsl_user("OOM: dropping\n");
                        return 1;
                    }
                    memcpy(cur_msg->payload + LWS_PRE, in, len);
                    cur_msg->len = len;
                    ___ll_bck_insert(struct msg__mas, cur_msg, msg_list, pss->to_msg_list);
                    lws_callback_on_writable(pss->client_wsi);
                }
            }
            break;

        case LWS_CALLBACK_CLIENT_CLOSED:
            pss->client_wsi = NULL;
            if (pss->wsi) {
                lws_close_reason(pss->wsi, LWS_CLOSE_STATUS_NORMAL, NULL, 0);
            }
            break;

        default:
            break;
    }

    return 0;
}

#define LWS_PLUGIN_PROTOCOL_MASTER_TTH \
    { \
        "lws-tth", \
        callback_tth, \
        sizeof(struct per_session_data__mas), \
        128, \
        0, NULL, 0 \
    }
