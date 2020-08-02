#ifndef TTH_CODES_H
#define TTH_CODES_H

#define TTH_CODE_ENUM_CLIENT_ADD 50

enum tth_code {
    TTH_CODE_ERROR = -1,
    /* messages from server */
    TTH_CODE_SERVER_MESSAGE = 0,
    TTH_CODE_SERVER_PLAYER_JOINED = 1,
    TTH_CODE_SERVER_PLAYER_LEFT = 2,
    /* messages from client */
    TTH_CODE_CLIENT_JOIN_ROOM = TTH_CODE_ENUM_CLIENT_ADD + 0,
    TTH_CODE_CLIENT_LEAVE_ROOM = TTH_CODE_ENUM_CLIENT_ADD + 1
};

enum tth_dest_code {
    TTH_DEST_CODE_INVALID = -1,
    TTH_DEST_CODE_THIS_CLIENT = 0,
    TTH_DEST_CODE_ALL = 1,
    TTH_DEST_CODE_HOST = 2,
    TTH_DEST_CODE_SPEAKER = 3
};

enum tth_code tth_get_code(char *msg, int len);

int tth_add_code(char *dst, enum tth_code code);

#endif
