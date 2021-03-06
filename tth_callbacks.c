#include "tth_structs.h"
#include "tth_misc.h"

#include "tth_callbacks.h"
#include "tth_signals.h"

#include <stdlib.h>
#include <string.h>
#include <libwebsockets.h>
#include "cJSON/cJSON.h"
#include "tth_timeout.h"

/* internal */

struct __stop_explanation_callback_args {
    struct per_session_data__tth *pss;
    struct per_vhost_data__tth *vhd;
    int number_of_turn;
};

struct __new_word_callback_args {
    struct per_session_data__tth *pss;
    struct per_vhost_data__tth *vhd;
};

int __incr_timeval(struct timeval *time, int64_t incr) {
    time->tv_sec += incr / 1000;
    time->tv_usec += incr % 1000 * 1000;
    if (time->tv_usec >= 1000000) {
        time->tv_usec -= 1000000;
        time->tv_usec++;
    }
    return 0;
}

void __stop_explanation(void *_vhd, void *_pss) {
    struct per_vhost_data__tth *vhd = (struct per_vhost_data__tth *)_vhd;
    struct per_session_data__tth *pss = (struct per_session_data__tth *)_pss;

    vhd->info->substate = TTH_SUBSTATE_EDIT;

    if (vhd->info->word) {
        struct edit_words_data__tth *word = malloc(sizeof(struct edit_words_data__tth));
        if (!word) {
            tth_sMessage(vhd, pss, "server error", "error", "cEndWordExplanation");
            goto end;
        }
        word->word = vhd->info->word;
        word->cause = TTH_CAUSE_CODE_NOT_EXPLAINED;
        word->edit_list = NULL;
        ___ll_bck_insert(struct edit_words_data__tth, word, edit_list, vhd->edit_words);
        vhd->info->word = NULL;

        tth_sWordExplanationEnded(vhd, pss, TTH_CAUSE_CODE_NOT_EXPLAINED);
    }

    tth_sExplanationEnded(vhd, pss);

end:
    tth_sWordsToEdit(vhd, pss);
}

void __stop_explanation_callback(void *_args) {
    struct __stop_explanation_callback_args *args = (struct __stop_explanation_callback_args *)_args;

    if (args->vhd->info->state != TTH_STATE_PLAY || args->vhd->info->substate != TTH_SUBSTATE_EXPLANATION) {
        return;
    }

    if (args->number_of_turn != args->vhd->info->number_of_turn) {
        return;
    }

    __stop_explanation(args->vhd, args->pss);
}

void __new_word_callback(void *_args) {
    struct __new_word_callback_args *args = (struct __new_word_callback_args *)_args;

    tth_sNewWord(args->vhd, args->pss);
}

void __stop_callback(void *args) {
    exit(0);
}

void __start_explanation(void *_vhd, void *_pss) {
    struct per_vhost_data__tth *vhd = (struct per_vhost_data__tth *)_vhd;
    struct per_session_data__tth *pss = (struct per_session_data__tth *)_pss;

    vhd->info->substate = TTH_SUBSTATE_EXPLANATION;
    vhd->info->word = vhd->fresh_words->word;
    void *tmp = vhd->fresh_words;
    vhd->fresh_words = vhd->fresh_words->word_list;
    struct edit_words_data__tth *pword = vhd->edit_words;
    while (pword) {
        void *tmp = pword;
        pword = pword->edit_list;
        free(tmp);
    }
    vhd->edit_words = NULL;

    free(tmp);

    gettimeofday(vhd->info->start_time, NULL);
    __incr_timeval(vhd->info->start_time, vhd->info->transport_delay + vhd->info->settings->delay_time);
    memcpy(vhd->info->end_explanation_time, vhd->info->start_time, sizeof(struct timeval));
    __incr_timeval(vhd->info->end_explanation_time, vhd->info->settings->explanation_time);
    memcpy(vhd->info->end_aftermath_time, vhd->info->end_explanation_time, sizeof(struct timeval));
    __incr_timeval(vhd->info->end_aftermath_time, vhd->info->settings->aftermath_time);

    if (vhd->info->settings->strict_mode) {
        struct timeval *time = malloc(sizeof(struct timeval));
        if (!time) {
            goto end;
        }
        int wtime = vhd->info->transport_delay + vhd->info->settings->delay_time + vhd->info->settings->explanation_time + vhd->info->settings->aftermath_time;
        time->tv_sec = wtime / 1000;
        time->tv_usec = wtime % 1000 * 1000;
        struct __stop_explanation_callback_args *args = malloc(sizeof(struct __stop_explanation_callback_args));
        if (!args) {
            goto end;
        }
        args->vhd = vhd;
        args->pss = pss;
        args->number_of_turn = vhd->info->number_of_turn;
        tth_set_timeout(time, &__stop_explanation_callback, args);
    }

    struct timeval *time = malloc(sizeof(struct timeval));
    if (!time) {
        goto end;
    }
    int wtime = vhd->info->transport_delay;
    time->tv_sec = wtime / 1000;
    time->tv_usec = wtime % 1000 * 1000;
    struct __new_word_callback_args *args = malloc(sizeof(struct __new_word_callback_args));
    if (!args) {
        goto end;
    }
    args->vhd = vhd;
    args->pss = pss;
    tth_set_timeout(time, &__new_word_callback, args);

end:
    tth_sExplanationStarted(vhd, pss);
}

int __randrange(int a, int b) {
    uint64_t rnd = (uint64_t)random();
    rnd *= (b - a);
    rnd /= RAND_MAX;
    rnd += a;
    if (rnd >= b) {
        if (b) {
            return b - 1;
        }
        return 0;
    }
    return rnd;
}

enum tth_cause_code __get_cause(char *_cause) {
    enum tth_cause_code cause;
    if (!strcmp(_cause, "explained")) {
        cause = TTH_CAUSE_CODE_EXPLAINED;
    } else if (!strcmp(_cause, "mistake")) {
        cause = TTH_CAUSE_CODE_MISTAKE;
    } else if (!strcmp(_cause, "notExplained")) {
        cause = TTH_CAUSE_CODE_NOT_EXPLAINED;
    } else {
        cause = TTH_CAUSE_CODE_INVALID;
    }
    return cause;
}

int __turn_prepare(void *_vhd) {
    struct per_vhost_data__tth *vhd = (struct per_vhost_data__tth *)_vhd;

    vhd->info->substate = TTH_SUBSTATE_WAIT;

    // getting players number
    int cnt = 0;
    lws_start_foreach_llp(struct user_data__tth **, ppud, vhd->user_list) {
        cnt++;
    } lws_end_foreach_llp(ppud, user_list);

    vhd->info->speaker_pos++;
    vhd->info->listener_pos++;
    if (vhd->info->speaker_pos == cnt) {
        vhd->info->listener_pos++;
        if (vhd->info->listener_pos == vhd->info->speaker_pos) {
            vhd->info->listener_pos++;
        }
    }
    vhd->info->speaker_pos %= cnt;
    vhd->info->listener_pos %= cnt;

    vhd->info->speaker_ready = 0;
    vhd->info->listener_ready = 0;

    vhd->info->number_of_turn++;

    return 0;
}

void __end_game(void *_vhd, void *_pss) {
    struct per_vhost_data__tth *vhd = (struct per_vhost_data__tth *)_vhd;
    struct per_session_data__tth *pss = (struct per_session_data__tth *)_pss;

    tth_sGameEnded(vhd, pss);

    struct timeval *time = malloc(sizeof(struct timeval));
    if (!time) {
        exit(0);
    }
    int wtime = vhd->info->transport_delay;
    time->tv_sec = wtime / 1000;
    time->tv_usec = wtime % 1000 * 1000000;
    tth_set_timeout(time, &__stop_callback, NULL);

    vhd->info->state = TTH_STATE_WAIT;
    vhd->info->substate = TTH_SUBSTATE_UNDEFINED;

    struct user_data__tth *puser = vhd->user_list;
    while (puser) {
        void *tmp = puser;
        free(puser->username);
        puser = puser->user_list;
        free(tmp);
    }
}

/* external */

/* server */

int tth_callback_error(void *vhd, void *pss, char *msg, int len) {
    char *_msg = malloc(len + 1);
    if (!_msg) {
        lwsl_user("OOM: dropping\n");
        return 1;
    }
    memcpy(_msg, msg, len);
    *(_msg + len) = 0;
    lwsl_err("tth: tth_callback_error on message: \"%s\"\n", _msg);
    int needed = snprintf(NULL, 0, "tth: tth_callback_error on message: \"%s\"\n", _msg);
    char *ret_msg = malloc(needed);
    if (!ret_msg) {
        lwsl_user("OOM: dropping\n");
        return 1;
    }
    sprintf(ret_msg, "tth: tth_callback_error on message: \"%s\"\n", _msg);
    tth_sMessage(vhd, pss, ret_msg, "error", "");
    free(ret_msg);
    free(_msg);
    return 1;
}

int tth_callback_server(void *vhd, void *pss, int code, char *msg, int len) {
    char *_msg = malloc(len + 1);
    if (!_msg) {
        lwsl_user("OOM: dropping\n");
        return 1;
    }
    memcpy(_msg, msg, len);
    *(_msg + len) = 0;
    lwsl_warn("tth: tth_callbacks: got message with code %i \"%s\" that can be only emitted by server, ignoring\n", code, _msg);
    int needed = snprintf(NULL, 0, "tth: tth_callbacks: got message with code %i \"%s\" that can be only emitted by server, ignoring\n", code, _msg);
    char *ret_msg = malloc(needed);
    if (!ret_msg) {
        lwsl_user("OOM: dropping\n");
        return 1;
    }
    sprintf(ret_msg, "tth: tth_callbacks: got message with code %i \"%s\" that can be only emitted by server, ignoring\n", code, _msg);
    tth_sMessage(vhd, pss, ret_msg, "error", "");
    free(ret_msg);
    free(_msg);
    return 1;
}

/* client */

int tth_callback_client_join_room(void *_vhd, void *_pss, char *msg, int len) {
    struct per_session_data__tth *pss = (struct per_session_data__tth *)_pss;
    struct per_vhost_data__tth *vhd = (struct per_vhost_data__tth *)_vhd;
    char *_msg = malloc(len + 1);
    if (!_msg) {
        lwsl_user("OOM: dropping\n");
        tth_sMessage(vhd, pss, "Server error", "error", "cJoinRoom");
        return 1;
    }
    memcpy(_msg, msg, len);
    *(_msg + len) = 0;
    lwsl_user("tth: tth_callback_client_join_room: \"%s\"\n", _msg);
    free(_msg);

    cJSON *_username;
    cJSON *_time_zone_offset;
    cJSON *_data = cJSON_ParseWithLength(msg, len);

    if (_data == NULL) {
        tth_sMessage(vhd, pss, "Server error", "error", "cJoinRoom");
        return 1;
    }

    _username = cJSON_GetObjectItemCaseSensitive(_data, "username");
    if (!cJSON_IsString(_username)) {
        tth_sMessage(vhd, pss, "Improper type of field `username`, not a string", "error", "cJoinRoom");
        return 1;
    }
    
    _time_zone_offset = cJSON_GetObjectItemCaseSensitive(_data, "time_zone_offset");
    if (!cJSON_IsNumber(_time_zone_offset)) {
        tth_sMessage(vhd, pss, "Improper type of field `time_zone_offset`, not a number", "error", "cJoinRoom");
        return 1;
    }

    if (*(_username->valuestring) == 0) {
        tth_sMessage(vhd, pss, "Username can't be emty", "error", "cJoinRoom");
        return 1;
    }

    char *username = malloc(strlen(_username->valuestring));
    if (!username) {
        lwsl_user("OOM: dropping\n");
        tth_sMessage(vhd, pss, "Server error", "error", "cJoinRoom");
        return 1;
    }
    strcpy(username, _username->valuestring);
    
    int time_zone_offset = (int)_time_zone_offset->valuedouble;
    cJSON_Delete(_data);

    // trying to find user with this pss
    struct user_data__tth *puser_pss = NULL;
    struct user_data__tth *puser = NULL;
    lws_start_foreach_llp(struct user_data__tth **, ppud, vhd->user_list) {
        if ((*ppud)->client_id == pss->client_id) {
            puser_pss = *ppud;
        }
        if (!strcmp((*ppud)->username, username)) {
            puser = *ppud;
        }
    } lws_end_foreach_llp(ppud, user_list);

    // if user is already in room
    if (puser_pss) {
        tth_sMessage(vhd, pss, "Client is already in room", "error", "cJoinRoom");
        return 1;
    }

    // creating user if not exists
    if (!puser) {
        if (vhd->info->state != TTH_STATE_WAIT) {
            tth_sMessage(vhd, pss, "State isn't 'play', only logging in can be perfomed", "error", "cJoinRoom");
            return 1;
        }
        puser = malloc(sizeof(struct user_data__tth));
        if (!puser) {
            lwsl_user("OOM: dropping\n");
            tth_sMessage(vhd, pss, "Server error", "error", "cJoinRoom");
            return 1;
        }
        memset(puser, 0, sizeof(struct user_data__tth));
        ___ll_bck_insert(struct user_data__tth, puser, user_list, vhd->user_list);
    }

    // check if data is valid
    if (puser->online) {
        tth_sMessage(vhd, pss, "Username is already in use", "error", "cJoinRoom");
        return 1;
    }

    // marking user online and adding info
    puser->username = username;
    puser->time_zone_offset = time_zone_offset;
    puser->online = 1;
    puser->pss = pss;
    puser->client_id = pss->client_id;

    tth_sPlayerJoined(vhd, pss, username);
    tth_sYouJoined(vhd, pss);

    return 0;
}

int tth_callback_client_leave_room(void *_vhd, void *_pss) {
    struct per_session_data__tth *pss = (struct per_session_data__tth *)_pss;
    struct per_vhost_data__tth *vhd = (struct per_vhost_data__tth *)_vhd;
    lwsl_user("tth: tth_callback_client_leave_room\n");

    // finding user with this pss
    struct user_data__tth *puser = NULL;
    lws_start_foreach_llp(struct user_data__tth **, ppud, vhd->user_list) {
        if ((*ppud)->client_id == pss->client_id) {
            puser = *ppud;
            break;
        }
    } lws_end_foreach_llp(ppud, user_list);

    // if no user
    if (!puser) {
        tth_sMessage(vhd, pss, "You are not in room", "error", "cLeaveRoom");
        return 1;
    }

    // check if data is valid
    if (!puser->online) {
        tth_sMessage(vhd, pss, "You are not in room", "error", "cLeaveRoom");
        return 1;
    }
 
    // marking user offline
    puser->online = 0;
    puser->client_id = 0;
    puser->pss = NULL;
    
    tth_sPlayerLeft(vhd, pss, puser->username);

    return 0;
}

int tth_callback_client_start_game(void *_vhd, void *_pss) {
    struct per_session_data__tth *pss = (struct per_session_data__tth *)_pss;
    struct per_vhost_data__tth *vhd = (struct per_vhost_data__tth *)_vhd;
    lwsl_user("tth: tth_callback_client_start_game\n");

    // finding user with this pss
    struct user_data__tth *puser = NULL;
    lws_start_foreach_llp(struct user_data__tth **, ppud, vhd->user_list) {
        if ((*ppud)->client_id == pss->client_id) {
            puser = *ppud;
            break;
        }
    } lws_end_foreach_llp(ppud, user_list);

    // if no user
    if (!puser) {
        tth_sMessage(vhd, pss, "You are not in room", "error", "cStartGame");
        return 1;
    }

    struct user_data__tth *puh = NULL;
    // checking whether it's the host
    lws_start_foreach_llp(struct user_data__tth **, ppud, vhd->user_list) {
        if ((*ppud)->online) {
            puh = *ppud;
            break;
        }
    } lws_end_foreach_llp(ppud, user_list);

    if (!puh) {
        tth_sMessage(vhd, pss, "Everyone is offline", "error", "cStartGame");
        return 1;
    }
    if (puh->client_id != puser->client_id) {
        tth_sMessage(vhd, pss, "Only host can start a game", "error", "cStartGame");
        return 1;
    }
    int cnt = 0;
    lws_start_foreach_llp(struct user_data__tth **, ppud, vhd->user_list) {
        if ((*ppud)->online) {
            cnt++;
        }
    } lws_end_foreach_llp(ppud, user_list);

    if (cnt < 2) {
        tth_sMessage(vhd, pss, "Not enough players to start the game (at least two required)", "error", "cStartGame");
        return 1;
    }
    if (vhd->info->state != TTH_STATE_WAIT) {
        tth_sMessage(vhd, pss, "State isn't wait", "error", "cStartGame");
        return 1;
    }

    // kicking off offline users
    struct user_data__tth *online_users = NULL;
    lws_start_foreach_llp(struct user_data__tth **, ppud, vhd->user_list) {
        if ((*ppud)->online) {
            struct user_data__tth *tmp = malloc(sizeof(struct user_data__tth));
            if (!tmp) {
                return 1;
            }
            memcpy(tmp, *ppud, sizeof(struct user_data__tth));
            tmp->user_list = NULL;
            ___ll_bck_insert(struct user_data__tth, tmp, user_list, online_users);
        }
    } lws_end_foreach_llp(ppud, user_list);
    while (vhd->user_list) {
        struct user_data__tth *tmp = vhd->user_list;
        vhd->user_list = vhd->user_list->user_list;
        if (!tmp->online) {
            free(tmp->username);
        }
        free(tmp);
    }
    vhd->user_list = online_users;

    vhd->info->state = TTH_STATE_PLAY;
    
    /* choosing words */
    // checking number of words
    if (vhd->info->settings->word_number > vhd->info->dict->len) {
        tth_sMessage(vhd, pss, "Not enough words in dictionary, decreasing word number to maximum avaliable", "warn", "cStartGame");
        vhd->info->settings->word_number = vhd->info->dict->len;
    }
    // collecting words
    int w_cnt = 0;
    int pos;
    while (w_cnt != vhd->info->settings->word_number) {
        pos = __randrange(0, vhd->info->dict->len);
        int8_t taken = 0;
        lws_start_foreach_llp(struct word_data__tth **, ppwd, vhd->fresh_words) {
            if (*(vhd->info->dict->words+pos) == (*ppwd)->word) {
                taken = 1;
                break;
            }
        } lws_end_foreach_llp(ppwd, word_list);

        if (!taken) {
            struct word_data__tth *new_word = malloc(sizeof(struct word_data__tth));
            if (!new_word) {
                lwsl_user("OOM: dropping\n");
                return 1;
            }
            new_word->word = *(vhd->info->dict->words+pos);
            new_word->word_list = NULL;
            lws_ll_fwd_insert(new_word, word_list, vhd->fresh_words);
            w_cnt++;
        }
    }

    vhd->info->speaker_pos = cnt - 1;
    vhd->info->listener_pos = cnt - 2;
    vhd->info->number_of_turn = 0;
    __turn_prepare(vhd);
    // May be something else

    tth_sGameStarted(vhd, pss);

    return 0;
}

int tth_callback_client_speaker_ready(void *_vhd, void *_pss) {
    struct per_session_data__tth *pss = (struct per_session_data__tth *)_pss;
    struct per_vhost_data__tth *vhd = (struct per_vhost_data__tth *)_vhd;
    
    lwsl_user("tth_callback_client_speaker_ready\n");

    // finding user with this pss and counting they pos
    struct user_data__tth *puser = NULL;
    int pos = 0;
    lws_start_foreach_llp(struct user_data__tth **, ppud, vhd->user_list) {
        if ((*ppud)->client_id == pss->client_id) {
            puser = *ppud;
            break;
        }
        pos++;
    } lws_end_foreach_llp(ppud, user_list);

    // if no user
    if (!puser) {
        tth_sMessage(vhd, pss, "You are not in room", "error", "cSpeakerReady");
        return 1;
    }

    // checking if state is play and substate is wait
    if (vhd->info->state != TTH_STATE_PLAY) {
        tth_sMessage(vhd, pss, "Game state isn't 'play'", "error", "cSpeakerReady");
        return 1;
    }
    if (vhd->info->substate != TTH_SUBSTATE_WAIT) {
        tth_sMessage(vhd, pss, "Game substate isn't 'wait'", "error", "cSpeakerReady");
        return 1;
    }

    // checking if user is speaker
    if (pos != vhd->info->speaker_pos) {
        tth_sMessage(vhd, pss, "You are not a speaker", "error", "cSpeakerReady");
        return 1;
    }

    // checking if user isn't already ready
    if (vhd->info->speaker_ready) {
        tth_sMessage(vhd, pss, "You are already ready", "error", "cSpeakerReady");
        return 1;
    }

    vhd->info->speaker_ready = 1;

    // if listener is also ready --- let's start
    if (vhd->info->listener_ready) {
        __start_explanation(vhd, pss);
    }

    return 0;
}

int tth_callback_client_listener_ready(void *_vhd, void *_pss) {
    struct per_session_data__tth *pss = (struct per_session_data__tth *)_pss;
    struct per_vhost_data__tth *vhd = (struct per_vhost_data__tth *)_vhd;
    
    lwsl_user("tth_callback_client_listener_ready\n");

    // finding user with this pss and counting they pos
    struct user_data__tth *puser = NULL;
    int pos = 0;
    lws_start_foreach_llp(struct user_data__tth **, ppud, vhd->user_list) {
        if ((*ppud)->client_id == pss->client_id) {
            puser = *ppud;
            break;
        }
        pos++;
    } lws_end_foreach_llp(ppud, user_list);

    // if no user
    if (!puser) {
        tth_sMessage(vhd, pss, "You are not in room", "error", "cListenerReady");
        return 1;
    }

    // checking if state is play and substate is wait
    if (vhd->info->state != TTH_STATE_PLAY) {
        tth_sMessage(vhd, pss, "Game state isn't 'play'", "error", "cListenerReady");
        return 1;
    }
    if (vhd->info->substate != TTH_SUBSTATE_WAIT) {
        tth_sMessage(vhd, pss, "Game substate isn't 'wait'", "error", "cListenerReady");
        return 1;
    }

    // checking if user is listener
    if (pos != vhd->info->listener_pos) {
        tth_sMessage(vhd, pss, "You are not a listener", "error", "cListenerReady");
        return 1;
    }

    // checking if user isn't already ready
    if (vhd->info->listener_ready) {
        tth_sMessage(vhd, pss, "You are already ready", "error", "cListenerReady");
        return 1;
    }

    vhd->info->listener_ready = 1;

    // if speaker is also ready --- let's start
    if (vhd->info->speaker_ready) {
        __start_explanation(vhd, pss);
    }

    return 0;
}

int tth_callback_client_end_word_explanation(void *_vhd, void *_pss, char *msg, int len) {
    struct per_session_data__tth *pss = (struct per_session_data__tth *)_pss;
    struct per_vhost_data__tth *vhd = (struct per_vhost_data__tth *)_vhd;

    lwsl_user("tth_callback_client_end_word_explanation\n");

    // finding user with this pss and counting their pos
    struct user_data__tth *puser = NULL;
    int pos = 0;
    lws_start_foreach_llp(struct user_data__tth **, ppud, vhd->user_list) {
        if ((*ppud)->client_id == pss->client_id) {
            puser = *ppud;
            break;
        }
        pos++;
    } lws_end_foreach_llp(ppud, user_list);

    // if no user
    if (!puser) {
        tth_sMessage(vhd, pss, "You are not in room", "error", "cEndWordExplanation");
        return 1;
    }

    // checking if state is play and substate is explanation
    if (vhd->info->state != TTH_STATE_PLAY) {
        tth_sMessage(vhd, pss, "Game state isn't 'play'", "error", "cEndWordExplanation");
        return 1;
    }
    if (vhd->info->substate != TTH_SUBSTATE_EXPLANATION) {
        tth_sMessage(vhd, pss, "Game substate isn't 'explanation'", "error", "cEndWordExplanation");
        return 1;
    }

    // checking if user is speaker
    if (pos != vhd->info->speaker_pos) {
        tth_sMessage(vhd, pss, "Only speaker can send this signal", "error", "cEndWordExplanation");
        return 1;
    }

    // checking time
    struct timeval *curr_time = malloc(sizeof(struct timeval));
    if (!curr_time) {
        tth_sMessage(vhd, pss, "server error", "error", "cEndWordExplanation");
        return 1;
    }
    gettimeofday(curr_time, NULL);

    if (__time_cmp(vhd->info->start_time, curr_time)) {
        tth_sMessage(vhd, pss, "Too early", "error", "cEndWordExplanation");
        return 1;
    }
    /*
     * no need
    if (__time_cmp(curr_time, vhd->info->end_aftermath_time)) {
        tth_sMessage(vhd, pss, "Too late", "error", "cEndWordExplanation");
        return 1;
    }
    */

    // processing input data
    cJSON *__cause;
    cJSON *_data = cJSON_ParseWithLength(msg, len);
    if (!_data) {
        tth_sMessage(vhd, pss, "Server error", "error", "cEndWordExplanation");
        return 1;
    }
    
    __cause = cJSON_GetObjectItemCaseSensitive(_data, "cause");
    if (!cJSON_IsString(__cause)) {
        tth_sMessage(vhd, pss, "Improper type of field `cause`, not a string", "error", "cEndWordExplanation");
        return 1;
    }

    char *_cause = __cause->valuestring;
    enum tth_cause_code cause = __get_cause(_cause);

    switch (cause) {
        case TTH_CAUSE_CODE_EXPLAINED:
            {
            struct edit_words_data__tth *word = malloc(sizeof(struct edit_words_data__tth));
            if (!word) {
                tth_sMessage(vhd, pss, "server error", "error", "cEndWordExplanation");
                return 1;
            }
            word->word = vhd->info->word;
            word->cause = TTH_CAUSE_CODE_EXPLAINED;
            word->edit_list = NULL;
            ___ll_bck_insert(struct edit_words_data__tth, word, edit_list, vhd->edit_words);
            vhd->info->word = NULL;
            if (vhd->fresh_words == NULL) {
                __stop_explanation(vhd, pss);
                break;
            }
            if (__time_cmp(curr_time, vhd->info->end_explanation_time)) {
                __stop_explanation(vhd, pss);
                break;
            }
            vhd->info->word = vhd->fresh_words->word;
            void *tmp = vhd->fresh_words;
            vhd->fresh_words = vhd->fresh_words->word_list;
            free(tmp);

            tth_sNewWord(vhd, pss);
            tth_sWordExplanationEnded(vhd, pss, TTH_CAUSE_CODE_EXPLAINED);
            break;
            }
        case TTH_CAUSE_CODE_MISTAKE:
            {
            struct edit_words_data__tth *word = malloc(sizeof(struct edit_words_data__tth));
            if (!word) {
                tth_sMessage(vhd, pss, "server error", "error", "cEndWordExplanation");
                return 1;
            }
            word->word = vhd->info->word;
            word->cause = TTH_CAUSE_CODE_MISTAKE;
            word->edit_list = NULL;
            ___ll_bck_insert(struct edit_words_data__tth, word, edit_list, vhd->edit_words);
            vhd->info->word = NULL;

            tth_sWordExplanationEnded(vhd, pss, TTH_CAUSE_CODE_MISTAKE);
            __stop_explanation(vhd, pss);
            break;
            }
        case TTH_CAUSE_CODE_NOT_EXPLAINED:
            tth_sWordExplanationEnded(vhd, pss, TTH_CAUSE_CODE_NOT_EXPLAINED);
            __stop_explanation(vhd, pss);
            break;
        case TTH_CAUSE_CODE_INVALID:
            tth_sMessage(vhd, pss, "Improper value of field `cause`", "error", "cEndWordExplanation");
            return 1;
    }

    return 0;
}

int tth_callback_client_words_edited(void *_vhd, void *_pss, char *msg, int len) {
    struct per_session_data__tth *pss = (struct per_session_data__tth *)_pss;
    struct per_vhost_data__tth *vhd = (struct per_vhost_data__tth *)_vhd;

    lwsl_user("tth_callback_client_words_edited\n");

    // finding user with this pss and counting their pos
    struct user_data__tth *puser = NULL;
    int pos = 0;
    lws_start_foreach_llp(struct user_data__tth **, ppud, vhd->user_list) {
        if ((*ppud)->client_id == pss->client_id) {
            puser = *ppud;
            break;
        }
        pos++;
    } lws_end_foreach_llp(ppud, user_list);
 
    // if no user
    if (!puser) {
        tth_sMessage(vhd, pss, "You are not in room", "error", "cWordsEdited");
        return 1;
    }

    // checking if state is play and substate is edit
    if (vhd->info->state != TTH_STATE_PLAY) {
        tth_sMessage(vhd, pss, "Game state isn't 'play'", "error", "cWordsEdited");
        return 1;
    }
    if (vhd->info->substate != TTH_SUBSTATE_EDIT) {
        tth_sMessage(vhd, pss, "Game substate isn't 'edit'", "error", "cWordsEdited");
        return 1;
    }

    // checking if user is speaker
    if (pos != vhd->info->speaker_pos) {
        tth_sMessage(vhd, pss, "Only speaker can send this signal", "error", "cWordsEdited");
        return 1;
    }

    // cleanup
    struct edit_words_data__tth *pword = vhd->words;
    while (pword) {
        void *tmp = pword;
        pword = pword->edit_list;
        free(tmp);
    }
    vhd->words = NULL;


    // process input and gen vhd->words
    cJSON *_words = NULL;
    cJSON *_data = cJSON_ParseWithLength(msg, len);
    if (!_data) {
        tth_sMessage(vhd, pss, "server error", "error", "cWordsEdited");
        return 1;
    }
    _words = cJSON_GetObjectItemCaseSensitive(_data, "editWords");
    if (!_words) {
        tth_sMessage(vhd, pss, "server error", "error", "cWordsEdited");
        return 1;
    }
    if (!cJSON_IsArray(_words)) {
        tth_sMessage(vhd, pss, "Incorrect type of field 'editWords'", "error", "cWordsEdited");
        return 1;
    }
    int cnt = 0;
    lws_start_foreach_llp(struct edit_words_data__tth **, pped, vhd->edit_words) {
        cnt++;
    } lws_end_foreach_llp(pped, edit_list);
    
    if (cJSON_GetArraySize(_words) != cnt) {
        tth_sMessage(vhd, pss, "Incorrect number of words", "error", "cWordsEdited");
        return 1;
    }

    // running transaction test
    cJSON *_word = NULL;
    struct edit_words_data__tth *curr_word = vhd->edit_words;
    cJSON_ArrayForEach(_word, _words) {
        if (!cJSON_IsObject(_word)) {
            tth_sMessage(vhd, pss, "Incorrect type of element field 'editWords'", "error", "cWordsEdited");
            return 1;
        }
        cJSON *_aword = cJSON_GetObjectItemCaseSensitive(_word, "word");
        if (!cJSON_IsString(_aword)) {
            tth_sMessage(vhd, pss, "Incorrect type of field 'word'", "error", "cWordsEdited");
            return 1;
        }
        if (strcmp(curr_word->word, _aword->valuestring)) {
            int needed = snprintf(NULL, 0, "Incorrect word on position %i", cnt);
            char *error_msg = malloc(needed);
            if (!error_msg) {
                tth_sMessage(vhd, pss, "server error", "error", "cWordsEdited");
                lwsl_user("OOM: dropping\n");
                return 1;
            }
            sprintf(error_msg, "Incorrect word on position %i", cnt);
            tth_sMessage(vhd, pss, error_msg, "error", "cWordsEdited");
            free(error_msg);
            return 1;
        }
        cJSON *_cause = cJSON_GetObjectItemCaseSensitive(_word, "wordState");
        if (!cJSON_IsString(_cause)) {
            tth_sMessage(vhd, pss, "Incorrect type of field 'wordState'", "error", "cWordsEdited");
            return 1;
        }
        enum tth_cause_code cause = __get_cause(_cause->valuestring);
        switch (cause) {
            case TTH_CAUSE_CODE_EXPLAINED:
            case TTH_CAUSE_CODE_MISTAKE:
            case TTH_CAUSE_CODE_NOT_EXPLAINED:
                break;
            case TTH_CAUSE_CODE_INVALID:
                tth_sMessage(vhd, pss, "Incorrect value of field 'wordState'", "error", "cWordsEdited");
                return 1;
        }
        curr_word = curr_word->edit_list;
    }

    // running transaction
    _word = NULL;
    curr_word = vhd->edit_words;
    cnt = 0;
    cJSON_ArrayForEach(_word, _words) {
        if (!cJSON_IsObject(_word)) {
            tth_sMessage(vhd, pss, "Incorrect value of field 'editWords'", "error", "cWordsEdited");
            return 1;
        }
        cJSON *_aword = cJSON_GetObjectItemCaseSensitive(_word, "word");
        if (!cJSON_IsString(_aword)) {
            tth_sMessage(vhd, pss, "Incorrect value of field 'word'", "error", "cWordsEdited");
            return 1;
        }
        if (strcmp(curr_word->word, _aword->valuestring)) {
            int needed = snprintf(NULL, 0, "Incorrect word on position %i", cnt);
            char *error_msg = malloc(needed);
            if (!error_msg) {
                tth_sMessage(vhd, pss, "server error", "error", "cWordsEdited");
                lwsl_user("OOM: dropping\n");
                return 1;
            }
            sprintf(error_msg, "Incorrect word on position %i", cnt);
            tth_sMessage(vhd, pss, error_msg, "error", "cWordsEdited");
            free(error_msg);
            return 1;
        }
        cJSON *_cause = cJSON_GetObjectItemCaseSensitive(_word, "wordState");
        if (!cJSON_IsString(_cause)) {
            tth_sMessage(vhd, pss, "Incorrect type of field 'wordState'", "error", "cWordsEdited");
            return 1;
        }
        enum tth_cause_code cause = __get_cause(_cause->valuestring);
        switch (cause) {
            case TTH_CAUSE_CODE_EXPLAINED:
                {
                    int cnt = 0;
                    struct user_data__tth *speaker = NULL;
                    struct user_data__tth *listener = NULL;
                    lws_start_foreach_llp(struct user_data__tth **, ppud, vhd->user_list) {
                        if (cnt == vhd->info->speaker_pos) {
                            speaker = *ppud;
                        }
                        if (cnt == vhd->info->listener_pos) {
                            listener = *ppud;
                        }
                        cnt++;
                    } lws_end_foreach_llp(ppud, user_list);
                    speaker->score_explained++;
                    listener->score_guessed++;
                }
            case TTH_CAUSE_CODE_MISTAKE:
                {
                struct edit_words_data__tth *__word = malloc(sizeof(struct edit_words_data__tth));
                if (!__word) {
                    tth_sMessage(vhd, pss, "server error", "error", "cWordsEdited");
                    lwsl_user("OOM: dropping\n");
                    return 1;
                }
                __word->word = curr_word->word;
                __word->cause = cause;
                __word->edit_list = NULL;
                ___ll_bck_insert(struct edit_words_data__tth, __word, edit_list, vhd->words);
                break;
                }
            case TTH_CAUSE_CODE_NOT_EXPLAINED:
                curr_word->cause = cause;
                int fresh_words_count = 0;
                lws_start_foreach_llp(struct word_data__tth **, ppwd, vhd->fresh_words) {
                    fresh_words_count++;
                } lws_end_foreach_llp(ppwd, word_list);
                int insert_pos = __randrange(0, fresh_words_count);
                // lwsl_warn("pos: %i\n", insert_pos);
                struct word_data__tth *__word = malloc(sizeof(struct word_data__tth));
                if (!__word) {
                    tth_sMessage(vhd, pss, "server error", "error", "cWordsEdited");
                    lwsl_user("OOM: dropping\n");
                    return 1;
                }
                __word->word_list = NULL;
                __word->word = curr_word->word;
                int cnt = 0;
                if (cnt == insert_pos) {
                    __word->word_list = vhd->fresh_words;
                    vhd->fresh_words = __word;
                } else {
                    lws_start_foreach_llp(struct word_data__tth **, ppwd, vhd->fresh_words) {
                        if (cnt == insert_pos) {
                            __word->word_list = (*ppwd)->word_list;
                            (*ppwd)->word_list = __word;
                            break;
                        }
                        cnt++;
                    } lws_end_foreach_llp(ppwd, word_list);
                }
                break;
            case TTH_CAUSE_CODE_INVALID:
                tth_sMessage(vhd, pss, "Incorrect value of field 'wordState'", "error", "cWordsEdited");
                return 1;
        }
        curr_word = curr_word->edit_list;
    }
    
    vhd->info->substate = TTH_SUBSTATE_WAIT;

    if (!vhd->fresh_words) {
        __end_game(vhd, pss);
        return 0;
    }

    __turn_prepare(vhd);

    tth_sNextTurn(vhd, pss);

    return 0;
}

int tth_callback_client_ping(void *_vhd, void *_pss) {
    tth_sPong(_vhd, _pss);
    return 0;
}
