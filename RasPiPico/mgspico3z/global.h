#pragma once
#include <stdint.h>

#include "MgspicoSettings.h"
extern MgspicoSettings g_Setting;

extern const char *pFN_MGSDRV;
extern const char *pFN_KINROU5;
extern const char *pFN_NDP;
extern const char *pFN_MGSPICO_DAT;

inline bool IsMGS(MgspicoSettings::MUSICDATA x) { return (MgspicoSettings::MUSICDATA::MGS==x); }
inline bool IsKIN5(MgspicoSettings::MUSICDATA x) { return (MgspicoSettings::MUSICDATA::KIN5==x); }
// inline bool IsMGSorKIN5(MgspicoSettings::MUSICDATA x) { return ((MgspicoSettings::MUSICDATA::MGS==x)||(MgspicoSettings::MUSICDATA::KIN5==x)); }
// inline bool IsTGF(MgspicoSettings::MUSICDATA x) { return (MgspicoSettings::MUSICDATA::TGF==x); }
// inline bool IsVGM(MgspicoSettings::MUSICDATA x) { return (MgspicoSettings::MUSICDATA::VGM==x); }
// inline bool IsTGForVGM(MgspicoSettings::MUSICDATA x) { return ((MgspicoSettings::MUSICDATA::TGF==x)||(MgspicoSettings::MUSICDATA::VGM==x)); }
