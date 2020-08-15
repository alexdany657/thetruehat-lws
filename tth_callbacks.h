#ifndef TTH_CALLBACKS_H
#define TTH_CALLBACKS_H

/* server callbacks */

int tth_callback_error(void *vhd, void *pss, char *msg, int len);

int tth_callback_server(void *vhd, void *pss, int code, char *msg, int len);

/* client callbacks */

int tth_callback_client_join_room(void *_vhd, void *_pss, char *msg, int len);

int tth_callback_client_leave_room(void *_vhd, void *_pss);

int tth_callback_client_start_game(void *_vhd, void *_pss);

int tth_callback_client_speaker_ready(void *_vhd, void *_pss);

int tth_callback_client_listener_ready(void *_vhd, void *_pss);

int tth_callback_client_end_word_explanation(void *_vhd, void *_pss, char *msg, int len);

int tth_callback_client_words_edited(void *_vhd, void *_pss, char *msg, int len);

#endif
