#ifndef TTH_SIGNALS_H
#define TTH_SIGNALS_H

#include "tth_structs.h"

#include "tth_codes.h"

int tth_sMessage(void *vhd, void *pss, void *_amsg, void *_aseverity, void *_asignal);

int tth_sPlayerJoined(void *vhd, void *pss, void *_ausername);

int tth_sPlayerLeft(void *vhd, void *pss, void *_ausername);

int tth_sYouJoined(void *vhd, void *pss, void *_puser);

#endif
