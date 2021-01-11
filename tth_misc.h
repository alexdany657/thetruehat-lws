#ifndef TTH_MISC_H
#define TTH_MISC_H

#include <libwebsockets.h>

#define ___ll_bck_insert(___type, ___new_object, ___m_list, ___list_head) { \
    if (!(___list_head)) { \
        ___list_head = ___new_object; \
    } else { \
        lws_start_foreach_llp(___type **, ___ppss, ___list_head) { \
            if (!((*___ppss)->___m_list)) { \
                (*___ppss)->___m_list = ___new_object; \
                break; \
            } \
        } lws_end_foreach_llp(___ppss, ___m_list); \
    } \
}

#define __print_ll(___type, ___list_head, ___m_list) { \
    lws_start_foreach_llp(___type **, ___ptr, ___list_head) { \
        lwsl_err("%p->", *___ptr); \
    } lws_end_foreach_llp(___ptr, ___m_list); \
    lwsl_err("(null)\n"); \
}

#define __time_cmp(___a, ___b) \
    (___a->tv_sec > ___b->tv_sec || \
     (___a->tv_sec == ___b->tv_sec && \
      (___a->tv_usec > ___b->tv_usec)))

#endif
