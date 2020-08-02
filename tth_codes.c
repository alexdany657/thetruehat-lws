/* tth codes implementation */

#include "tth_codes.h"

#include <string.h>

enum tth_code tth_get_code(char *msg, int len) {
    if (len < 2) {
        return TTH_CODE_ERROR;
    }
    if (*msg < '0' || *msg > '9') {
        return TTH_CODE_ERROR;
    }
    if (*(msg + 1) < '0' || *(msg + 1) > '9') {
        return TTH_CODE_ERROR;
    }
    int code = (int)((*msg) - '0') * 10 + (int)((*(msg + 1)) - '0');
    switch (code) {
        case TTH_CODE_SERVER_MESSAGE:
            return TTH_CODE_SERVER_MESSAGE;
        case TTH_CODE_SERVER_PLAYER_JOINED:
            return TTH_CODE_SERVER_PLAYER_JOINED;
        case TTH_CODE_SERVER_PLAYER_LEFT:
            return TTH_CODE_SERVER_PLAYER_LEFT;
        case TTH_CODE_CLIENT_JOIN_ROOM:
            return TTH_CODE_CLIENT_JOIN_ROOM;
        case TTH_CODE_CLIENT_LEAVE_ROOM:
            return TTH_CODE_CLIENT_LEAVE_ROOM;
    }
    return TTH_CODE_ERROR;
}

int tth_add_code(char *dst, enum tth_code code) {
    if (code == TTH_CODE_ERROR) {
        return 1;
    }
    if (code >= TTH_CODE_ENUM_CLIENT_ADD) {
        return 2;
    }
    *dst = code / 10 + '0';
    *(dst + 1) = code % 10 + '0';
    return 0;
}
