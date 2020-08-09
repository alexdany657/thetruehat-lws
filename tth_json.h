#ifndef TTH_JSON_H
#define TTH_JSON_H

#include "tth_structs.h"

void *tth_get_playerList(void *_vhd);

void *tth_get_host(void *_vhd);

void *tth_get_state(void *_vhd);

void *tth_get_substate(void *_vhd);

void *tth_get_settings(void *_vhd);

void *tth_get_speaker(void *_vhd);

void *tth_get_listener(void *_vhd);

void *tth_get_words_count(void *_vhd);

void *tth_get_start_time(void *_vhd);

#endif
