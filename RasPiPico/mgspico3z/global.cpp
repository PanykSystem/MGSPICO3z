#include "global.h"

MgspicoSettings g_Setting;

const char *pFN_MGSDRV	= "MGSDRV.COM";
const char *pFN_KINROU5 = "KINROU5.DRV";
const char *pFN_NDP		= "NDP.BIN";

#ifdef MGSPICO_3RD_Z
const char *pFN_MGSPICO_DAT = "MGSPIC3Z.DAT";
#else
const char *pFN_MGSPICO_DAT = "MGSPICO.DAT";
#endif