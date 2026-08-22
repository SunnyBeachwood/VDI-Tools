/*================================================================================*/
/* Copyright (C) 2009, Don Milne.                                                 */
/* All rights reserved.                                                           */
/* See LICENSE.TXT for conditions on copying, distribution, modification and use. */
/*================================================================================*/

#ifndef CMDLINE_H
#define CMDLINE_H

/*======================================================================*/
/*                Module parses command line arguments                  */
/*======================================================================*/

#include "djtypes.h"
#include "parms.h"

BOOL CmdLine_Parse(s_CLONEPARMS *parm);
UINT CmdLine_BatchSourceCount(void);
CPFN CmdLine_BatchSource(UINT index);
UINT CmdLine_BatchThreadLimit(void);
CPFN CmdLine_BatchOutputDir(void);

#endif
