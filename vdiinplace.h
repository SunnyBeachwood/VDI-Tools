/* Safe, journaled in-place optimization of standalone dynamic VDI files. */
#ifndef VDIINPLACE_H
#define VDIINPLACE_H

#include <windef.h>
#include "parms.h"
#include "encryption.h"

BOOL VDIIP_HasJournal(CPFN sourceName);
BOOL VDIIP_Proceed(HINSTANCE hInstRes, HWND hWndParent, s_CLONEPARMS *parm, const ENC_REPORT *encryption);
PSTR VDIIP_GetErrorString(void);

#endif
