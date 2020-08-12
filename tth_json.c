#include "tth_json.h"

#include "tth_structs.h"
#include "tth_misc.h"

#include "cJSON/cJSON.h"
#include <libwebsockets.h>
#include <sys/time.h>

/* internal functions */


void *__get_user_bpos(void *_vhd, int pos) {
    struct per_vhost_data__tth *vhd = (struct per_vhost_data__tth *)_vhd;

    int cnt = 0;
    lws_start_foreach_llp(struct user_data__tth **, ppud, vhd->user_list) {
        if (cnt == pos) {
            return (*ppud);
        }
        cnt++;
    } lws_end_foreach_llp(ppud, user_list);

    return NULL;
}

void *__get_user(void *_vhd, int client_id) {
    struct per_vhost_data__tth *vhd = (struct per_vhost_data__tth *)_vhd;

    lws_start_foreach_llp(struct user_data__tth **, ppud, vhd->user_list) {
        if ((*ppud)->client_id == client_id) {
            cJSON *_username = cJSON_CreateString((*ppud)->username);
            if (!_username) {
                return NULL;
            }
            return _username;
        }
    } lws_end_foreach_llp(ppud, user_list);

    return NULL;
}

/* external funcitons */

void *tth_get_playerList(void *_vhd) {
    struct per_vhost_data__tth *vhd = (struct per_vhost_data__tth *)_vhd;

    cJSON *_users = cJSON_CreateArray();
    if (!_users) {
        return NULL;
    }

    lws_start_foreach_llp(struct user_data__tth **, ppud, vhd->user_list) {
        cJSON *__user = cJSON_CreateObject();
        if (!__user) {
            break;
        }

        cJSON *__username = cJSON_CreateString((*ppud)->username);
        if (!__username) {
            break;
        }
        cJSON_AddItemToObject(__user, "username", __username);

        cJSON *__online = cJSON_CreateBool((*ppud)->online);
        if (!__online) {
            break;
        }
        cJSON_AddItemToObject(__user, "online", __online);

        cJSON_AddItemToArray(_users, __user);
    } lws_end_foreach_llp(ppud, user_list);

    return _users;
}

void *tth_get_host(void *_vhd) {
    struct per_vhost_data__tth *vhd = (struct per_vhost_data__tth *)_vhd;

    char *ahost = NULL;
    lws_start_foreach_llp(struct user_data__tth **, ppud, vhd->user_list) {
        if ((*ppud)->online) { 
            ahost = (*ppud)->username;
            break;
        }
    } lws_end_foreach_llp(ppud, user_list);
    if (!ahost) {
        ahost = "";
    }
    cJSON *_host = cJSON_CreateString(ahost);
    if (!_host) {
        return NULL;
    }

    return _host;
}

void *tth_get_state(void *_vhd) {
    struct per_vhost_data__tth *vhd = (struct per_vhost_data__tth *)_vhd;

    char *state = NULL;
    switch (vhd->info->state) {
        case TTH_STATE_WAIT:
            state = "wait";
            break;
        case TTH_STATE_PLAY:
            state = "play";
            break;
        case TTH_STATE_END:
            state = "end";
            break;
        default:
            return NULL;
    }

    cJSON *_state = cJSON_CreateString(state);
    if (!_state) {
        return NULL;
    }
    
    return _state;
}

void *tth_get_substate(void *_vhd) {
    struct per_vhost_data__tth *vhd = (struct per_vhost_data__tth *)_vhd;

    cJSON *_substate = NULL;
    switch (vhd->info->substate) {
        case TTH_SUBSTATE_WAIT:
            _substate = cJSON_CreateString("wait");
            break;
        case TTH_SUBSTATE_EXPLANATION:
            _substate = cJSON_CreateString("explanation");
            break;
        case TTH_SUBSTATE_EDIT:
            _substate = cJSON_CreateString("edit");
            break;
        case TTH_SUBSTATE_UNDEFINED:
            _substate = cJSON_CreateNull();
            break;
        default:
            return NULL;
    }

    if (!_substate) {
        return NULL;
    }

    return _substate;
}

void *tth_get_settings(void *_vhd) {
    struct per_vhost_data__tth *vhd = (struct per_vhost_data__tth *)_vhd;

    cJSON *_settings = cJSON_CreateObject();
    if (!_settings) {
        return NULL;
    }

    cJSON *_word_number = cJSON_CreateNumber(vhd->info->settings->word_number);
    if (!_word_number) {
        return NULL;
    }
    cJSON_AddItemToObject(_settings, "wordNumber", _word_number);

    cJSON *_dictionary_id = cJSON_CreateNumber(vhd->info->settings->dictionary_id);
    if (!_dictionary_id) {
        return NULL;
    }
    cJSON_AddItemToObject(_settings, "dictionaryId", _dictionary_id);

    cJSON *_strict_mode = cJSON_CreateBool(vhd->info->settings->strict_mode);
    if (!_strict_mode) {
        return NULL;
    }
    cJSON_AddItemToObject(_settings, "strictMode", _strict_mode);
    
    cJSON *_delay_time = cJSON_CreateNumber(vhd->info->settings->delay_time);
    if (!_delay_time) {
        return NULL;
    }
    cJSON_AddItemToObject(_settings, "delayTime", _delay_time);

    cJSON *_explanation_time = cJSON_CreateNumber(vhd->info->settings->explanation_time);
    if (!_explanation_time) {
        return NULL;
    }
    cJSON_AddItemToObject(_settings, "explanationTime", _explanation_time);

    cJSON *_aftermath_time = cJSON_CreateNumber(vhd->info->settings->aftermath_time);
    if (!_aftermath_time) {
        return NULL;
    }
    cJSON_AddItemToObject(_settings, "aftermathTime", _aftermath_time);

    return _settings;
}

void *tth_get_speaker(void *_vhd) {
    struct per_vhost_data__tth *vhd = (struct per_vhost_data__tth *)_vhd;

    struct user_data__tth *speaker = (struct user_data__tth *)__get_user_bpos(vhd, vhd->info->speaker_pos);
    if (!speaker) {
        return NULL;
    }
    cJSON *_speaker = cJSON_CreateString(speaker->username);
    if (!_speaker) {
        return NULL;
    }

    return _speaker;
}

void *tth_get_listener(void *_vhd) {
    struct per_vhost_data__tth *vhd = (struct per_vhost_data__tth *)_vhd;

    struct user_data__tth *listener = (struct user_data__tth *)__get_user_bpos(vhd, vhd->info->listener_pos);
    if (!listener) {
        return NULL;
    }
    cJSON *_listener = cJSON_CreateString(listener->username);
    if (!_listener) {
        return NULL;
    }

    return _listener;
}

void *tth_get_words_count(void *_vhd) {
    struct per_vhost_data__tth *vhd = (struct per_vhost_data__tth *)_vhd;

    int cnt = 0;
    lws_start_foreach_llp(struct word_data__tth **, ppwd, vhd->fresh_words) {
        cnt++;
    } lws_end_foreach_llp(ppwd, word_list);

    cJSON *_words_count = cJSON_CreateNumber(cnt);
    if (!_words_count) {
        return NULL;
    }
    return _words_count;
}

void *tth_get_start_time(void *_vhd) {
    struct per_vhost_data__tth *vhd = (struct per_vhost_data__tth *)_vhd;
    
    cJSON *_start_time = cJSON_CreateNumber(vhd->info->start_time);
    if (!_start_time) {
        return NULL;
    }

    return _start_time;
}

void *tth_get_word(void *_vhd) {
    struct per_vhost_data__tth *vhd = (struct per_vhost_data__tth *)_vhd;
    
    cJSON *_word = cJSON_CreateString(vhd->info->word);
    if (!_word) {
        return NULL;
    }

    return _word;
}

void *tth_get_cause(enum tth_cause_code cause) {
    switch (cause) {
        case TTH_CAUSE_CODE_EXPLAINED:
            {
            cJSON *_cause = cJSON_CreateString("explained");
            if (!_cause) {
                return NULL;
            }
            return _cause;
            }
        case TTH_CAUSE_CODE_MISTAKE:
            {
            cJSON *_cause = cJSON_CreateString("mistake");
            if (!_cause) {
                return NULL;
            }
            return _cause;
            }
        case TTH_CAUSE_CODE_NOT_EXPLAINED:
            {
            cJSON *_cause = cJSON_CreateString("notExplained");
            if (!_cause) {
                return NULL;
            }
            return _cause;
            }
        case TTH_CAUSE_CODE_INVALID:
            return NULL;
    }
    return NULL;
}

void *tth_get_edit_words(void *_vhd) {
    struct per_vhost_data__tth *vhd = (struct per_vhost_data__tth *)_vhd;

    cJSON *_edit_words = cJSON_CreateArray();
    if (!_edit_words) {
        return NULL;
    }
    lws_start_foreach_llp(struct edit_words_data__tth **, pped, vhd->edit_words) {
        cJSON *_edit = cJSON_CreateObject();
        if (!_edit) {
            return NULL;
        }

        cJSON *_word = cJSON_CreateString((*pped)->word);
        if (!_word) {
            return NULL;
        }
        cJSON_AddItemToObject(_edit, "word", _word);

        cJSON *_cause = (cJSON *)tth_get_cause((*pped)->cause);
        if (!_cause) {
            return NULL;
        }
        cJSON_AddItemToObject(_edit, "cause", _cause);

        cJSON_AddItemToArray(_edit_words, _edit);

    } lws_end_foreach_llp(pped, edit_list);

    return _edit_words;
}

void *tth_get_words(void *_vhd) {
    struct per_vhost_data__tth *vhd = (struct per_vhost_data__tth *)_vhd;

    cJSON *_words = cJSON_CreateArray();
    if (!_words) {
        return NULL;
    }
    lws_start_foreach_llp(struct edit_words_data__tth **, pped, vhd->words) {
        cJSON *_wordc = cJSON_CreateObject();
        if (!_wordc) {
            return NULL;
        }

        cJSON *_word = cJSON_CreateString((*pped)->word);
        if (!_word) {
            return NULL;
        }
        cJSON_AddItemToObject(_wordc, "word", _word);

        cJSON *_cause = (cJSON *)tth_get_cause((*pped)->cause);
        if (!_cause) {
            return NULL;
        }
        cJSON_AddItemToObject(_wordc, "cause", _cause);

        cJSON_AddItemToArray(_words, _wordc);

    } lws_end_foreach_llp(pped, edit_list);

    return _words;
}
