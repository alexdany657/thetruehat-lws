/**
 * some code taken from official lws example "lws-minimal": https://libwebsockets.org/git/libwebsockets/tree/minimal-examples/ws-server/minimal-ws-server?h=v4.0-stable
 */

#if !defined (LWS_PLUGIN_STATIC)
#define LWS_DLL
#define LWS_INTERNAL
#include <libwebsockets.h>
#endif

#include <string.h>

#include "tth_structs.h"

#include "tth_callbacks.h"
#include "tth_codes.h"

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
            break;

        case LWS_CALLBACK_ESTABLISHED:
            /* add ourselves to the list of live pss held in the vhd */
            lws_ll_fwd_insert(pss, pss_list, vhd->pss_list);
            pss->wsi = wsi;
            lwsl_user("initial wsi: %p\n", wsi);
            pss->clientId = vhd->clientCnt++;
            break;

        case LWS_CALLBACK_CLOSED:
            lwsl_user("pss: close: clientId %i\n", pss->clientId);
            /* remove our closing pss from the list of live pss */
            lws_ll_fwd_remove(struct per_session_data__tth, pss_list, pss, vhd->pss_list);

            /* if this client has logged in user --- we need to make it offline */
            struct user_data__tth *puser = NULL;
            lws_start_foreach_llp(struct user_data__tth **, ppud, vhd->user_list) {
                if ((*ppud)->clientId == pss->clientId) {
                    puser = *ppud;
                    break;
                }
            } lws_end_foreach_llp(ppud, user_list);
            if (puser) {
                puser->online = 0;
                puser->pss = NULL;
            }
            break;

        case LWS_CALLBACK_SERVER_WRITEABLE:
            {
                int8_t writed = 0;
                struct msg *pmsg_del = NULL;
                lws_start_foreach_llp(struct msg **, ppmsg, vhd->msg_list) {
                    int8_t shouldDelete = 1;
                    lws_start_foreach_llp(struct dest_data__tth **, ppdd, (*ppmsg)->dest_list) {
                        if (!((*ppdd)->sent)) {
                            shouldDelete = 0;
                            if ((*ppdd)->clientId == pss->clientId) {
                                if (writed) {
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
                                shouldDelete = 1;
                            }
                        }
                    } lws_end_foreach_llp(ppdd, dest_list);
                    if (shouldDelete) {
                        pmsg_del = *ppmsg;
                    }
                    if (writed) {
                        lws_callback_on_writable(wsi);
                        break;
                    }
                } lws_end_foreach_llp(ppmsg, msg_list);

                if (pmsg_del) {
                    lws_ll_fwd_remove(struct msg, msg_list, pmsg_del, vhd->msg_list);
                    free(pmsg_del->payload);
                    while (pmsg_del->dest_list) {
                        struct dest_data__tth *pdest_del = pmsg_del->dest_list;
                        lws_ll_fwd_remove(struct dest_data__tth, dest_list, pdest_del, pmsg_del->dest_list);
                        free(pdest_del);
                    }
                    free(pmsg_del);
                }

                break;
            }

        case LWS_CALLBACK_RECEIVE:
            {
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
                        tth_callback_client_join_room(pss, vhd, new_in, new_len);
                        break;
                    case TTH_CODE_CLIENT_LEAVE_ROOM:
                        tth_callback_client_leave_room(pss, vhd, new_in, new_len);
                        break;
                    default:
                        // lws_close_reason(wsi, LWS_CLOSE_STATUS_PROTOCOL_ERR, NULL, 0);
                        return 0;
                }

                break;
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
