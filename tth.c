/**
 * some code taken from official lws example "lws-minimal": https://libwebsockets.org/git/libwebsockets/tree/minimal-examples/ws-server/minimal-ws-server?h=v4.0-stable
 */

#include <libwebsockets.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <time.h>

#define LWS_PLUGIN_STATIC
#include "protocol_tth.c"
#include "tth_timeout.h"

static struct lws_protocols protocols[] = {
    LWS_PLUGIN_PROTOCOL_TTH,
    { NULL, NULL, 0, 0 } /* terminator */
};

static int interrupted;

static const struct lws_http_mount mount = {
    /* .mount_next */               NULL,           /* linked-list "next" */
    /* .mountpoint */               "/",            /* mountpoint URL */
    /* .origin */                   "./static",     /* serve from dir */
    /* .def */                      "index.html",   /* default filename */
    /* .protocol */                 NULL,
    /* .cgienv */                   NULL,
    /* .extra_mimetypes */          NULL,
    /* .interpret */                NULL,
    /* .cgi_timeout */              0,
    /* .cache_max_age */            0,
    /* .auth_mask */                0,
    /* .cache_reusable */           0,
    /* .cache_revalidate */         0,
    /* .cache_intermediaries */     0,
    /* .origin_protocol */          LWSMPRO_FILE,   /* files in a dir */
    /* .mountpoint_len */           1,              /* char count */
    /* .basic_auth_login_file */    NULL,
};

void sigint_handler(int sig) {
    interrupted = 1;
}

int main(int argc, char **argv) {
    int port;
    if (argc != 4) {
        lwsl_warn("usage: tth <port> <key> <auth port>\n");
        return 1;
    }
    port = atoi(*(argv+1));
    __load_key(*(argv+2));

    struct lws_context_creation_info info;
    struct lws_client_connect_info i;
    struct lws_context *context;
    int n = 0, logs = LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE
        /* for LLL_ verbosity above NOTICE to be built into lws,
         * lws must have been configured and built with
         * -DCMAKE_BUILD_TYPE=DEBUG instead of =RELEASE */
#ifndef NDEBUG
        | LLL_INFO | LLL_PARSER | LLL_HEADER
        | LLL_EXT | LLL_CLIENT | LLL_LATENCY
        | LLL_DEBUG
#endif
        ;

    signal(SIGINT, sigint_handler);
    tth_timer_init();
    srandom((int)time(NULL));

    lws_set_log_level(logs, NULL);

    memset(&info, 0, sizeof(struct lws_context_creation_info)); /* otherwise uninitialized garbage */
    info.port = port;
    info.mounts = &mount;
    info.protocols = protocols;
    info.vhost_name = "localhost";
    info.options = LWS_SERVER_OPTION_HTTP_HEADERS_SECURITY_BEST_PRACTICES_ENFORCE;

    context = lws_create_context(&info);
    if (!context) {
        lwsl_err("lws init failed\n");
        return 1;
    }

    memset(&i, 0, sizeof(struct lws_client_connect_info)); /* otherwise uninitialized garbage */
    i.context = context;
    i.port = atoi(*(argv+3));
    i.address = "localhost";
    i.path = "/server_auth";
    i.host = i.address;
    i.origin = i.address;
    i.method = "POST";
    i.protocol = protocols[0].name;
    i.ssl_connection = LCCSCF_HTTP_MULTIPART_MIME;

    if (!lws_client_connect_via_info(&i)) {
        lwsl_err("Failed to create connection to master server, dropping\n");
        return 1;
    }

    while (n >= 0 && !interrupted)
        n = lws_service(context, 0);

    lws_context_destroy(context);

    return 0;
}
