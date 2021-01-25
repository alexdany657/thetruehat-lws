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
#include "config.h"

#define MAX_KEY_LENGTH 64
#define NUM_OF_CONN_MAX 1024

static int NUM_OF_CONN = 0;

static const char * const param_names[] = {
    "key",
    "port",
};

/* one of these is created for each key---port pair */

struct key__mas {
    struct key__mas *key_list; /* ptr to next key in ll */
    char *key; /* room key*/
    int port; /* port of room server (zero means that room server isn't ready yet) */
};

/* one of these is created for each message (or message fragment) we receive */

struct msg__mas {
    struct msg__mas *msg_list;
    char *payload;
    int len;
};

/* one of these is created for each http client connectiong to us */

struct per_session_data_http__mas {
    struct lws_spa *spa; /* spa for post */
};

/* one of these is created for each client connecting to us */

struct per_session_data__mas {
    int is_server; /* flag that shows that this pss is server pss */
    int should_close; /* flag that shows that connection should close, checked on WRITABLE callback */
    struct per_session_data__mas *pss_list; /* if server */
    struct lws *wsi; /* if server */
    struct lws_client_connect_info i; /* if server */
    char *key; /* key of the room, if server */
    struct lws *client_wsi; /* if client (NULL means that client connection isn't ready yet) */
    struct per_session_data__mas *parent_pss; /* if it's client, we should rely on server's pss */
    struct per_session_data__mas *child_pss; /* if server */
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

    struct per_session_data__mas *pss_list; /* linked-list of live pss */
};

/* storage for keys */

static struct key__mas *key_list = NULL;

void __close(void *_vhd, void *_pss) {
    struct per_vhost_data__mas *vhd = (struct per_vhost_data__mas *)_vhd;
    struct per_session_data__mas *pss = (struct per_session_data__mas *)_pss;

    pss->should_close = 1;
    if (pss->child_pss) {
        pss->child_pss->should_close = 1;
    }
    if (pss->parent_pss) {
        pss->parent_pss->should_close = 1;
    }
    lws_callback_on_writable_all_protocol_vhost(vhd->vhost, vhd->protocol);
}

static void connection_attempt_mas(void *_vhd, void *_pss, void *_key) {
    struct per_vhost_data__mas *vhd = (struct per_vhost_data__mas *)_vhd;
    struct per_session_data__mas *pss = (struct per_session_data__mas *)_pss;
    char *key = (char *)_key;

    int port = 0;
    struct key__mas *cur_key = NULL;
    lws_start_foreach_llp(struct key__mas **, ppkl, key_list) {
        if (!strncmp(key, (*ppkl)->key, MAX_KEY_LENGTH)) {
            port = (*ppkl)->port;
            cur_key = *ppkl;
        }
#ifndef NDEBUG
            lwsl_warn("strings %s and %s %s\n", key, (*ppkl)->key, (strncmp(key, (*ppkl)->key, MAX_KEY_LENGTH) ? "differ" : "are equal"));
#endif
    } lws_end_foreach_llp(ppkl, key_list);

    if (NUM_OF_CONN == NUM_OF_CONN_MAX) {
        lwsl_warn("Maximum amount of connections reached, dropping\n");
        __close(vhd, pss);
        return;
    }
    if (port == 0 && NUM_OF_CONN < NUM_OF_CONN_MAX) {
        struct key__mas *new_key = malloc(sizeof(struct key__mas));
        if (!new_key) {
            lwsl_user("OOM: dropping\n");
            return;
        }
        memset(new_key, 0, sizeof(struct key__mas));

        new_key->key = malloc(sizeof(char) * (strnlen(key, MAX_KEY_LENGTH) + 1));
        lwsl_warn("key len: %li\n", strnlen(key, MAX_KEY_LENGTH));
        if (!new_key->key) {
            lwsl_user("OOM: dropping\n");
            return;
        }
        memcpy(new_key->key, key, strnlen(key, MAX_KEY_LENGTH));
        *(new_key->key + strnlen(key, MAX_KEY_LENGTH)) = 0;

        char *mas_port = malloc(sizeof(char) * snprintf(NULL, 0, "%i", lws_get_vhost_listen_port(vhd->vhost)));
        if (!mas_port) {
            lwsl_user("OOM: dropping\n");
            return;
        }
        sprintf(mas_port, "%i", lws_get_vhost_listen_port(vhd->vhost));

        NUM_OF_CONN++;
        pid_t pid = fork();
        if (pid == 0) {
            /* execl("/usr/bin/ssh", "ssh", "-l", "thehat", "localhost", "/home/thehat/bin/start_tth", "0", NULL); */
            execl("/usr/bin/ssh", "ssh", "-l", USERNAME, "localhost", EXEC_PATH, "0", new_key->key, mas_port, NULL);
        }
        free(mas_port);

        lws_ll_fwd_insert(new_key, key_list, key_list);
        cur_key = new_key;
    }

    /* FIXME: used same ptr, verify that is't ok */
    pss->key = cur_key->key;
    lwsl_warn("key: %s, len: %li\n", pss->key, strlen(pss->key));

    if (!cur_key->port) {
        __close(vhd, pss);
        return;
    }

    pss->i.context = vhd->context;
    pss->i.port = cur_key->port;
    pss->i.address = "localhost";
    pss->i.path = "/";
    pss->i.host = pss->i.address;
    pss->i.origin = pss->i.address;
    pss->i.ssl_connection = 0;

    pss->i.protocol = "lws-internal";
    pss->i.local_protocol_name = "lws-tth";
    pss->i.pwsi = &pss->client_wsi;

    if (!lws_client_connect_via_info(&pss->i)) {
        /* lws_close_reason(pss->wsi, LWS_CLOSE_STATUS_NORMAL, NULL, 0); */
        lwsl_user("connection_attempt_mas: close: failed to connect\n");
        __close(vhd, pss);
    }
}

static int callback_http_mas(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len) {
    struct per_session_data_http__mas *pss = (struct per_session_data_http__mas *)user;

    switch (reason) {
        case LWS_CALLBACK_HTTP:
            lwsl_warn("HTTP: %p %s\n", wsi, (const char *)in);
            if (!strcmp((const char *)in, "/server_auth")) {
                return 0;
            }
            break;

        case LWS_CALLBACK_HTTP_BODY:
            if (!pss->spa) {
                pss->spa = lws_spa_create(wsi, param_names, LWS_ARRAY_SIZE(param_names), 1024, NULL, NULL);
                if (!pss->spa) {
                    lwsl_user("callback http body: failed to create spa\n");
                    return -1;
                }
            }

            if (lws_spa_process(pss->spa, in, (int)len)) {
                return -1;
            }
            break;

        case LWS_CALLBACK_CLOSED_CLIENT_HTTP:
            if (pss->spa && lws_spa_destroy(pss->spa)) {
                return -1;
            }
            break;

        case LWS_CALLBACK_HTTP_BODY_COMPLETION:
            lwsl_user("LWS_CALLBACK_HTTP_BODY_COMPLETION\n");
            lws_spa_finalize(pss->spa);

            if (pss->spa) {
                if (lws_spa_get_string(pss->spa, 0)) {
                    int _len = lws_spa_get_length(pss->spa, 0);
                    const char *_payload = lws_spa_get_string(pss->spa, 0);
                    struct key__mas *cur_key = NULL;
                    lwsl_user("key: %.*s\n", _len, _payload);
                    lws_start_foreach_llp(struct key__mas **, ppkl, key_list) {
                        if (!strncmp((*ppkl)->key, _payload, _len)) {
                            cur_key = *ppkl;
                            break;
                        }
                    } lws_end_foreach_llp(ppkl, key_list);
                    if (!cur_key) {
                        lwsl_user("HTTP_BODY_COMLETION: no key\n");
                        lws_spa_destroy(pss->spa);
                        return -1;
                    }
                    if (lws_spa_get_string(pss->spa, 1)) {
                        const char *__port_s = lws_spa_get_string(pss->spa, 1);
                        int _port_l = lws_spa_get_length(pss->spa, 1);
                        char *_port_s = malloc(_port_l + 1);
                        if (!_port_s) {
                            lwsl_user("OOM: dropping\n");
                            return 1;
                        }
                        memcpy(_port_s, __port_s, _port_l);
                        *(_port_s + _port_l) = 0;
                        cur_key->port = atoi(_port_s);
                        lwsl_user("HTTP_BODY_COMLETION: successful auth for key %s, port %i\n", cur_key->key, cur_key->port);
                        /* TODO: prohibit reauth for key */

                        /* TODO: notify everyone (server only) on vhost to connect (if not and have the same key) */

                        lws_return_http_status(wsi, 202, NULL);
                    }
                }
                return -1;
            }

            if (pss->spa && lws_spa_destroy(pss->spa)) {
                return -1;
            }
            break;

        default:
            break;
    }
    return lws_callback_http_dummy(wsi, reason, user, in, len);
}

static int callback_mas(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len) {
    struct per_session_data__mas *pss = (struct per_session_data__mas *)user;
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
            pss->pss_list = NULL;
            pss->is_server = 1;
            pss->should_close = 0;
            pss->wsi = wsi;
            pss->from_msg_frag_list = NULL;
            pss->to_msg_frag_list = NULL;
            pss->from_msg_list = NULL;
            pss->to_msg_list = NULL;
            char *key_buff = malloc(sizeof(char) * (4 + MAX_KEY_LENGTH));
            if (!key_buff) {
                lwsl_user("OOM: dropping\n");
                return 1;
            }
            const char *key = lws_get_urlarg_by_name(wsi, "key", key_buff, 4 + MAX_KEY_LENGTH) + 1;
            lwsl_warn("key: %s, len: %li\n", key, strlen(key));
            if (strlen(key) == 0) {
                lwsl_warn("Key can't be empty\n");
                __close(vhd, pss);
            }
            /* open connection to backend and bind it */
            connection_attempt_mas(vhd, pss, (void *)key);
            free(key_buff);
            lwsl_user("initial wsi: %p, backend wsi: %p\n", wsi, pss->client_wsi);
            break;

        case LWS_CALLBACK_CLOSED:
            lwsl_user("pss: close: wsi: %p\n", pss->wsi);
            /* lws_close_reason(pss->client_wsi, LWS_CLOSE_STATUS_NORMAL, NULL, 0); */
            __close(vhd, pss);
            /* remove our closing pss from the list of live pss */
            lws_ll_fwd_remove(struct per_session_data__mas, pss_list, pss, vhd->pss_list);
            break;

        case LWS_CALLBACK_SERVER_WRITEABLE:
            {
                if (pss->should_close) {
                    lws_close_reason(pss->wsi, LWS_CLOSE_STATUS_NORMAL, NULL, 0);
                    return -1;
                }
                /* lwsl_warn("LWS_CALLBACK_SERVER_WRITEABLE\n"); */
                if (!pss->to_msg_list) {
                    return 0;
                }
                /* lwsl_warn("LWS_CALLBACK_SERVER_WRITEABLE\n"); */
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
                /* lwsl_warn("LWS_CALLBACK_RECEIVE\n"); */
                /* lwsl_warn("%i %i\n", lws_is_first_fragment(wsi), lws_is_final_fragment(wsi)); */
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
                        pss->from_msg_frag_list = NULL;
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
                    /* lwsl_warn("%p\n", pss->from_msg_list); */
                    if (pss->client_wsi) {
                        lws_callback_on_writable(pss->client_wsi);
                    }
                }
            }
            break;
        
        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
            lwsl_err("CLIENT_CONNECTION_ERROR: %s\n", in ? (char *)in : "(null)");
            lwsl_user("pss: %p, client: %p\n", pss ? pss->parent_pss : NULL, pss);
            break;

        case LWS_CALLBACK_CLIENT_ESTABLISHED:
            lwsl_user("%s: established\n", __func__);
            pss->is_server = 0;
            pss->wsi = wsi;
            lws_start_foreach_llp (struct per_session_data__mas **, ppss, vhd->pss_list) {
                /* lwsl_warn("%i, %p == %p, %i\n", (*ppss)->is_server, (*ppss)->client_wsi, wsi, (*ppss)->client_wsi == wsi); */
                if ((*ppss)->is_server && (*ppss)->client_wsi == wsi) {
                    pss->parent_pss = *ppss;
                    (*ppss)->child_pss = pss;
                    break;
                }
            } lws_end_foreach_llp (ppss, pss_list);
            /* lwsl_warn("%p -> %p\n", pss, pss->parent_pss); */
            break;

        case LWS_CALLBACK_CLIENT_WRITEABLE:
            {
                if (pss->should_close) {
                    lws_close_reason(pss->wsi, LWS_CLOSE_STATUS_NORMAL, NULL, 0);
                    return -1;
                }
                pss = pss->parent_pss;
                /* lwsl_warn("LWS_CALLBACK_CLIENT_WRITEABLE\n"); */
                /* lwsl_warn("%p\n", pss->from_msg_list); */
                if (!pss->from_msg_list) {
                    return 0;
                }
                /* lwsl_warn("LWS_CALLBACK_CLIENT_WRITEABLE\n"); */
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
                pss = pss->parent_pss;
                /* lwsl_warn("LWS_CALLBACK_CLIENT_RECEIVE\n"); */
                /* lwsl_warn("%i %i\n", lws_is_first_fragment(wsi), lws_is_final_fragment(wsi)); */
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
                        /* __print_ll(struct msg__mas, pss->to_msg_frag_list, msg_list); */
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
                        pss->to_msg_frag_list = NULL;
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
                    /* __print_ll(struct msg__mas, pss->to_msg_list, msg_list); */
                    ___ll_bck_insert(struct msg__mas, cur_msg, msg_list, pss->to_msg_list);
                    lws_callback_on_writable(pss->wsi);
                }
            }
            break;

        case LWS_CALLBACK_CLIENT_CLOSED:
            pss = pss->parent_pss;
            pss->client_wsi = NULL;
            lwsl_user("callback: client close\n");
            __close(vhd, pss);
            break;

        case LWS_CALLBACK_WSI_DESTROY:
            /* free everything allocated by this connection */
            {
                if (!pss) {
                    break;
                }
                struct msg__mas *tmp = pss->to_msg_frag_list;
                while (tmp) {
                    free(tmp->payload);
                    void *_tmp = tmp;
                    tmp = tmp->msg_list;
                    free(_tmp);
                }
                pss->to_msg_frag_list = NULL;
                tmp = pss->from_msg_frag_list;
                while (tmp) {
                    free(tmp->payload);
                    void *_tmp = tmp;
                    tmp = tmp->msg_list;
                    free(_tmp);
                }
                pss->from_msg_frag_list = NULL;
                tmp = pss->to_msg_list;
                while (tmp) {
                    free(tmp->payload);
                    void *_tmp = tmp;
                    tmp = tmp->msg_list;
                    free(_tmp);
                }
                pss->to_msg_list = NULL;
                tmp = pss->from_msg_list;
                while (tmp) {
                    free(tmp->payload);
                    void *_tmp = tmp;
                    tmp = tmp->msg_list;
                    free(_tmp);
                }
                pss->from_msg_list = NULL;
            }
            break;

        default:
            break;
    }

    return 0;
}

#define LWS_PLUGIN_PROTOCOL_MASTER_TTH \
    { \
        "http", \
        callback_http_mas, \
        sizeof(struct per_session_data_http__mas), \
        0, \
        0, NULL, 0 \
    }, \
    { \
        "lws-tth", \
        callback_mas, \
        sizeof(struct per_session_data__mas), \
        128, \
        0, NULL, 0 \
    }
