#ifndef TTH_SIGNALS_H
#define TTH_SIGNALS_H

#include "tth_structs.h"
#include "tth_codes.h"

int tth_sMessage(void *vhd, void *pss, void *_amsg, void *_aseverity, void *_asignal);

int tth_sPlayerJoined(void *vhd, void *pss, void *_ausername);

int tth_sPlayerLeft(void *vhd, void *pss, void *_ausername);

int tth_sYouJoined(void *_vhd, void *_pss);

int tth_sGameStarted(void *vhd, void *pss);

int tth_sExplanationStarted(void *vhd, void *pss);

int tth_sNewWord(void *vhd, void *pss);

int tth_sWordExplanationEnded(void *vhd, void *pss, enum tth_cause_code cause);

int tth_sExplanationEnded(void *vhd, void *pss);

int tth_sWordsToEdit(void *vhd, void *pss);

int tth_sNextTurn(void *vhd, void *pss);

int tth_sGameEnded(void *vhd, void *pss);

int tth_sPong(void *vhd, void *pss);

#endif
