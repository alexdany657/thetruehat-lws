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

#define ___ll_insert_when(___type, ___new_object, ___m_list, ___list_head, ___ppss, ___cond) { \
    if (!(___list_head)) { \
        ___list_head = ___new_object; \
    } else { \
        lws_start_foreach_llp(___type **, ___ppss, ___list_head) { \
            if (!((*___ppss)->___m_list)) { \
                (*___ppss)->___m_list = ___new_object; \
                break; \
            } \
            if (___cond) { \
                ___type *___nxt = (*___ppss)->___m_list; \
                (*___ppss)->___m_list = ___new_object; \
                ___new_object->___m_list = ___nxt; \
                break; \
            } \
        } lws_end_foreach_llp(___ppss, ___m_list); \
    } \
}

#endif
