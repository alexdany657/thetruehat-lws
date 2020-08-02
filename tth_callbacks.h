#ifndef TTH_CALLBACKS_H
#define TTH_CALLBACKS_H

/* server callbacks */

int tth_callback_error(void *vhd, void *pss, char *msg, int len);

int tth_callback_server(void *vhd, void *pss, int code, char *msg, int len);

/* client callbacks */

int tth_callback_client_join_room(void *pss, void *vhd, char *msg, int len);

int tth_callback_client_leave_room(void *pss, void *vhd, char *msg, int len);

#endif
