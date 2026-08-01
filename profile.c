/*================================================================================*/
/* Copyright (C) 2009, Don Milne.                                                 */
/* All rights reserved.                                                           */
/* See LICENSE.TXT for conditions on copying, distribution, modification and use. */
/*================================================================================*/

#include "djwarning.h"
#include <stdarg.h>
#include <windef.h>
#include <winbase.h>
#include <wingdi.h>
#include <winuser.h>
#include "profile.h"
#include "filename.h"
#include "env.h"

#define PUBLIC

static PSTR szOptions     = "Options";

static char szfnProfile[1024];

/*.........................................................................*/

PUBLIC void
Profile_Init(HINSTANCE hInstance, PSTR Basename)
{
   DWORD len = GetModuleFileNameA(hInstance,szfnProfile,sizeof(szfnProfile));
   while (len && szfnProfile[len-1]!='\\' && szfnProfile[len-1]!='/') len--;
   lstrcpynA(szfnProfile+len,Basename,sizeof(szfnProfile)-len);
}

/*.........................................................................*/

PUBLIC void
Profile_GetString(PSTR szOptionName, PSTR szValue, DWORD cbMax, PSTR szDefault)
{
   GetPrivateProfileString(szOptions,szOptionName,szDefault,szValue,cbMax,szfnProfile);
}

/*.........................................................*/

PUBLIC void
Profile_SetString(PSTR szOptionName, PSTR szValue)
{
   WritePrivateProfileString(szOptions,szOptionName,szValue,szfnProfile);
}

/*.........................................................*/

PUBLIC int
Profile_GetOption(PSTR szOptionName)
{
   return GetPrivateProfileInt(szOptions,szOptionName,0,szfnProfile);
}

/*.........................................................*/

PUBLIC int
Profile_GetOptionDefault(PSTR szOptionName, int DefaultValue)
{
   return GetPrivateProfileInt(szOptions,szOptionName,DefaultValue,szfnProfile);
}

/*.........................................................*/

/* end of profile.c */

