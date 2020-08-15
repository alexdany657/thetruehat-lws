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
        case TTH_CODE_SERVER_YOU_JOINED:
            return TTH_CODE_SERVER_YOU_JOINED;
        case TTH_CODE_SERVER_GAME_STARTED:
            return TTH_CODE_SERVER_GAME_STARTED;
        case TTH_CODE_SERVER_EXPLANATION_STARTED:
            return TTH_CODE_SERVER_EXPLANATION_STARTED;
        case TTH_CODE_SERVER_NEW_WORD:
            return TTH_CODE_SERVER_NEW_WORD;
        case TTH_CODE_SERVER_WORD_EXPLANATION_ENDED:
            return TTH_CODE_SERVER_WORD_EXPLANATION_ENDED;
        case TTH_CODE_SERVER_EXPLANATION_ENDED:
            return TTH_CODE_SERVER_EXPLANATION_ENDED;
        case TTH_CODE_SERVER_WORDS_TO_EDIT:
            return TTH_CODE_SERVER_WORDS_TO_EDIT;
        case TTH_CODE_SERVER_NEXT_TURN:
            return TTH_CODE_SERVER_NEXT_TURN;
        case TTH_CODE_SERVER_GAME_ENDED:
            return TTH_CODE_SERVER_GAME_ENDED;
        case TTH_CODE_SERVER_PONG:
            return TTH_CODE_SERVER_PONG;

        case TTH_CODE_CLIENT_JOIN_ROOM:
            return TTH_CODE_CLIENT_JOIN_ROOM;
        case TTH_CODE_CLIENT_LEAVE_ROOM:
            return TTH_CODE_CLIENT_LEAVE_ROOM;
        case TTH_CODE_CLIENT_START_GAME:
            return TTH_CODE_CLIENT_START_GAME;
        case TTH_CODE_CLIENT_SPEAKER_READY:
            return TTH_CODE_CLIENT_SPEAKER_READY;
        case TTH_CODE_CLIENT_LISTENER_READY:
            return TTH_CODE_CLIENT_LISTENER_READY;
        case TTH_CODE_CLIENT_END_WORD_EXPLANATION:
            return TTH_CODE_CLIENT_END_WORD_EXPLANATION;
        case TTH_CODE_CLIENT_WORDS_EDITED:
            return TTH_CODE_CLIENT_WORDS_EDITED;
        case TTH_CODE_CLIENT_PING:
            return TTH_CODE_CLIENT_PING;
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
