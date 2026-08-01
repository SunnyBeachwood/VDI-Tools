/*================================================================================*/
/* Copyright (C) 2009, Don Milne.                                                 */
/* All rights reserved.                                                           */
/* See LICENSE.TXT for conditions on copying, distribution, modification and use. */
/*================================================================================*/

/* Module mostly responsible for animating the main dialog box */
#include "djwarning.h"
#define NOTOOLBAR
#include <stdarg.h>
#include <stdlib.h>
#include <windows.h>
#include <commdlg.h>
#include "thermo.h"
#include "hexview.h"
#include "version.h"
#include "filename.h"
#include "vddr.h"
#include "mem.h"
#include "showheader.h"
#include "random.h"
#include "clone.h"
#include "partinfo.h"
#include "sectorviewer.h"
#include "ntfs.h"
#include "fat.h"
#include "extx.h"
#include "djfile.h"
#include "memfile.h"
#include "profile.h"
#include "env.h"
#include "djstring.h"
#include "cmdline.h"
#include "encryption.h"
#include "vdiinplace.h"
#include "localization.h"
#include "ids.h"

// fixed strings, these don't get localized
static PSTR szINIFileName = "VDI-Tools.ini"; // must not be longer than 31 chars.
static PSTR szSrcFileName = "SrcFile";
static PSTR szDstFileName = "DstFile";
static PSTR szLanguage    = "Language";
static int languagePreference = VDI_LANGUAGE_AUTO;

// these strings get localized
static PSTR pszNONE          /* = "None" */ ;
static PSTR pszOK            /* = "Ok" */ ;
static PSTR pszERROR         /* = "Error" */ ;
static PSTR pszUNKNOWNFS     /* = "Unknown(%02lx)" */ ;
static PSTR pszPLSSELSRC     /* = "Please select a source virtual disk" */ ;
static PSTR pszNOSRC         /* = "Cannot proceed - the source file does not exist!" */ ;
static PSTR pszERRAPPWND     /* = "Could not create app window (error x%x)" */ ;
static PSTR pszDLG_CLONEVDI  /* = "DLG_CLONEVDI" */ ;
static PSTR pszENC_CLONE_WARN;
static PSTR pszENC_INPLACE_WARN;
static PSTR pszINPLACE_CONFIRM;
static PSTR pszRECOVER_CONFIRM;
static PSTR pszRISK_TITLE;
static PSTR pszINPLACE_FAILED;
static PSTR pszRECOVERY_FOUND;
static PSTR pszENC_CLI_WARN;
static PSTR pszENC_INCOMPLETE;

/*
// these declarations moved to parms.h
//
#define PARM_FLAG_KEEPUUID  1
#define PARM_FLAG_ENLARGE   2
#define PARM_FLAG_REPART    4
#define PARM_FLAG_COMPACT   8

typedef struct {
   UINT flags;
   FNCHAR srcfn[1024];
   FNCHAR dstfn[1024];
   BYTE MBR[512];
   HVDDR hVDIsrc;
   CHAR  szDestSize[32];
   UINT  DestSectors;
} s_CLONEPARMS;
*/

// main dialog field idents
#define IDD_SOURCE_FN         200
#define IDD_BTN_SBROWSE       201
#define IDD_DEST_FN           202
#define IDD_BTN_DBROWSE       203
#define IDD_VALID_RESULT      204
#define IDD_OLD_SIZE          205
#define IDD_FILESYSTEM        206
#define IDD_UUID_CHANGE       207
#define IDD_UUID_KEEP         208
#define IDD_INCREASE_SIZE     209
#define IDD_NEW_SIZE          210
#define IDD_INCREASE_PARTSIZE 211
#define IDD_COMPACT           212
#define IDD_ABOUT_TEXT        213
#define IDD_MODE_CLONE        214
#define IDD_MODE_INPLACE      215
#define IDD_ENCRYPTION        216
#define IDD_LANGUAGE_COMBO    217
#define IDD_BTN_PARTINFO      300
#define IDD_BTN_HDRINFO       301
#define IDD_BTN_SECTOR_VIEW   302
#define IDD_BTN_PROCEED       1
#define IDD_BTN_EXIT          2

static HINSTANCE hInstApp;
static s_CLONEPARMS parm;
static FNCHAR tmpfn[1024];
static BOOL bCompactOptionDefault = FALSE;
static ENC_REPORT encryptionReport;

static void UpdateModeControls(HWND hDlg);

/*.......................................................................*/

static BOOL
Error(PSTR pszMsg)
{
   FILE stderr = 0;

   if (parm.flags & PARM_FLAG_CLIMODE) stderr = GetStdHandle(STD_ERROR_HANDLE);
   if (stderr == NULLFILE) stderr = 0;
   if (stderr) {
      File_WrBin(stderr,pszMsg,lstrlen(pszMsg));
      File_WrBin(stderr,"\r\n",2);
   } else {
      MessageBox(GetFocus(), pszMsg, RSTR(ERROR), MB_ICONEXCLAMATION|MB_OK);
   }
   return FALSE;
}

/*..........................................................*/

static PSTR
FSName(HVDDR hVDIsrc, PPART pPart)
{
   WORD superblock[512];
   PSTR psz;
   static char szUnk[16];
   switch (pPart->PartType) {
      case 0x00: psz=RSTR(NONE);  break;
      case 0x01: psz="FAT12"; break;
      case 0x04:
      case 0x06:
      case 0x0E: psz="FAT16"; break;
      case 0x82:
      case 0x05: psz="Linux swap"; break;
      case 0x07: {
         HUGE startLBA;
         startLBA = (UINT)MAKELONG(pPart->loStartLBA,pPart->hiStartLBA);
         if (NTFS_IsNTFSVolume(hVDIsrc,startLBA)) psz="NTFS"; // if this returns true then it's definitely NTFS.
         else psz="HPFS";
         // could also be FAT64(exFAT) - need distinguishing code.
         break;
      }
      case 0x0B: // fall through
      case 0x0C: psz="FAT32"; break;
      case 0x42: {
         HUGE startLBA;
         startLBA = (UINT)MAKELONG(pPart->loStartLBA,pPart->hiStartLBA);
         if (NTFS_IsNTFSVolume(hVDIsrc,startLBA)) psz="NTFS"; // if this returns true then it's definitely NTFS.
         else psz="LDM";
         break;
      }
      case 0x83: {
         UINT startLBA = (UINT)MAKELONG(pPart->loStartLBA,pPart->hiStartLBA);
         hVDIsrc->ReadSectors(hVDIsrc, (BYTE*)superblock, startLBA+2, 2);
         if (superblock[28]==0xEF53) {
            if (superblock[48] & 0x40) /* incompat: supports extents? */ psz = "ext4";
			else if (superblock[46] & 4) /* compat: journalling support? */ psz = "ext3";
            else psz = "ext2";
         } else {
            psz="ext1";
         }
         break;
      }
      default:
         wsprintf(szUnk,RSTR(UNKNOWNFS),pPart->PartType);
         psz=szUnk;
   }
   return psz;
}

/*....................................................*/

static UINT
ReadLE32(CPBYTE p)
{
   return ((UINT)p[0]) | (((UINT)p[1])<<8) | (((UINT)p[2])<<16) | (((UINT)p[3])<<24);
}

/*....................................................*/

static HUGE
ReadLE64(CPBYTE p)
{
   return MAKEHUGE(ReadLE32(p),ReadLE32(p+4));
}

/*....................................................*/

static BOOL
NullGUID(CPBYTE p)
{
   UINT i;
   for (i=0; i<16; i++) if (p[i]) return FALSE;
   return TRUE;
}

/*....................................................*/

static PSTR
FSNameAtLBA(HVDDR hVDIsrc, HUGE startLBA)
{
   BYTE boot[512];

   if (NTFS_IsNTFSVolume(hVDIsrc,startLBA)) return "NTFS";

   if (hVDIsrc->ReadSectors(hVDIsrc,boot,startLBA,1)) {
      if (Mem_Compare(boot+3,"EXFAT   ",8)==0) return "exFAT";
   }

   if (FAT_IsFATVolume(hVDIsrc,startLBA)) {
      if (hVDIsrc->ReadSectors(hVDIsrc,boot,startLBA,1) && Mem_Compare(boot+82,"FAT32   ",8)==0) return "FAT32";
      return "FAT16";
   }

   if (Extx_IsLinuxVolume(hVDIsrc,startLBA)) {
      WORD superblock[512];
      if (hVDIsrc->ReadSectors(hVDIsrc,(BYTE*)superblock,startLBA+2,2)) {
         if (superblock[48] & 0x40) return "ext4";
         if (superblock[46] & 4) return "ext3";
      }
      return "ext2";
   }

   return NULL;
}

/*....................................................*/

static void
AppendFSName(PSTR sz, PSTR pszFS)
{
   UINT n = lstrlen(sz);
   UINT cch = lstrlen(pszFS);

   if (n+cch+(n ? 1 : 0) < 256) {
      if (n) sz[n++] = ',';
      lstrcpy(sz+n,pszFS);
   }
}

/*....................................................*/

static BOOL
GetGPTFileSystem(PSTR sz)
{
   BYTE hdr[512];
   BYTE sectors[1024];
   HUGE entriesLBA;
   UINT nEntries,cbEntry,i;

   if (!parm.hVDIsrc->ReadSectors(parm.hVDIsrc,hdr,1,1) || Mem_Compare(hdr,"EFI PART",8)!=0) return FALSE;

   entriesLBA = ReadLE64(hdr+72);
   nEntries   = ReadLE32(hdr+80);
   cbEntry    = ReadLE32(hdr+84);
   if (entriesLBA<=0 || nEntries==0 || cbEntry<128 || cbEntry>512) return FALSE;
   if (nEntries>128) nEntries=128;

   sz[0] = 0;
   for (i=0; i<nEntries; i++) {
      HUGE entryOffset = ((HUGE)i)*cbEntry;
      HUGE sectorLBA = entriesLBA+(entryOffset>>9);
      UINT sectorOffset = LO32(entryOffset) & 511;
      UINT nSectors = (sectorOffset+48+511)>>9;
      PBYTE pEntry;

      if (!parm.hVDIsrc->ReadSectors(parm.hVDIsrc,sectors,sectorLBA,nSectors)) break;
      pEntry = sectors+sectorOffset;
      if (!NullGUID(pEntry)) {
         HUGE startLBA = ReadLE64(pEntry+32);
         PSTR pszFS = (startLBA>0 ? FSNameAtLBA(parm.hVDIsrc,startLBA) : NULL);
         if (pszFS) AppendFSName(sz,pszFS);
      }
   }

   if (!sz[0]) lstrcpy(sz,"GPT");
   return TRUE;
}

/*....................................................*/

static void
GetFileSystem(PSTR sz)
{
   if (parm.MBR[510]==0x55 && parm.MBR[511]==0xAA) {
      PPART pPart;
      UINT  i;
      PSTR  psz = sz;

      psz[0] = 0;
      for (i=0; i<4; i++) {
         pPart = (PPART)(parm.MBR+(446+i*16));
         if (pPart->PartType==0xEE && GetGPTFileSystem(sz)) return;
      }
      for (i=0; i<4; i++) {
         pPart = (PPART)(parm.MBR+(446+i*16));
         if (pPart->PartType) {
            if (i) *psz++ = ',';
            psz += wsprintf(psz,"%s",FSName(parm.hVDIsrc,pPart));
         }
      }
      if (!sz[0]) lstrcpy(sz,RSTR(NONE));

   } else {
      lstrcpy(sz,RSTR(NONE));
   }
}

/*....................................................*/

static void
ShowDriveInfo(HWND hDlg)
{
   SetDlgItemText(hDlg,IDD_VALID_RESULT,VDDR_GetErrorString(0xFFFFFFFF));
   if (parm.hVDIsrc) {
      UINT GB,MB,R;
      CHAR sz[256],numstr[16];
      HUGE drivesize;
      parm.hVDIsrc->GetDriveSize(parm.hVDIsrc,&drivesize);

      GB = (UINT)(drivesize>>30); // yes, drivesize[1] can overflow, but only if size is (2^30)GB or more!
      MB = (UINT)((drivesize & 0x3FFFFFFF)>>20);
      R  = (UINT)(drivesize & 0xFFFFF);

      if (GB) { // if drive is greater than 1GB.
          Env_DoubleToString(numstr,GB+MB/1024.0,2);
          wsprintf(sz, "%s GB", numstr);
      } else {
          Env_DoubleToString(numstr,MB+R/1024.0,2);
          wsprintf(sz, "%s MB", numstr);
      }
      SetDlgItemText(hDlg,IDD_OLD_SIZE,sz);
      SetDlgItemText(hDlg,IDD_NEW_SIZE,sz);

      parm.hVDIsrc->ReadSectors(parm.hVDIsrc, parm.MBR, 0, 1); // read MBR sector.

      GetFileSystem(sz);
      SetDlgItemText(hDlg,IDD_FILESYSTEM,sz);
      Encryption_Scan(parm.hVDIsrc,&encryptionReport);
      Encryption_FormatReport(&encryptionReport,sz,sizeof(sz));
      SetDlgItemText(hDlg,IDD_ENCRYPTION,sz);
      EnableWindow(GetDlgItem(hDlg,IDD_BTN_HDRINFO),TRUE);

   } else {
      SetDlgItemText(hDlg,IDD_OLD_SIZE,"");
      SetDlgItemText(hDlg,IDD_FILESYSTEM,"");
      SetDlgItemText(hDlg,IDD_OLD_SIZE,"");
      SetDlgItemText(hDlg,IDD_FILESYSTEM,"");
      if (VDIIP_HasJournal(parm.srcfn)) {
         SetDlgItemText(hDlg,IDD_VALID_RESULT,RSTR(RECOVERY_FOUND));
         SetDlgItemText(hDlg,IDD_ENCRYPTION,RSTR(ENC_INCOMPLETE));
      } else SetDlgItemText(hDlg,IDD_ENCRYPTION,"");
      EnableWindow(GetDlgItem(hDlg,IDD_BTN_HDRINFO),FALSE);
   }
}

/*....................................................*/

static void
OpenNewSource(HWND hDlg, BOOL bRefreshDlg)
{
   UINT modeFlags=parm.flags&PARM_FLAG_INPLACE;
   if (parm.hVDIsrc) parm.hVDIsrc->Close(parm.hVDIsrc);
   VDDR_OpenMediaRegistry(parm.srcfn);
   parm.hVDIsrc = VDDR_Open(parm.srcfn,0);
   if (bRefreshDlg) ShowDriveInfo(hDlg);
   if (VDIIP_HasJournal(parm.srcfn)) modeFlags|=PARM_FLAG_INPLACE|PARM_FLAG_RESUME;
   EnableWindow(GetDlgItem(hDlg,IDD_BTN_PROCEED),parm.hVDIsrc!=NULL || VDIIP_HasJournal(parm.srcfn));
   EnableWindow(GetDlgItem(hDlg,IDD_BTN_PARTINFO),parm.hVDIsrc!=NULL);
   EnableWindow(GetDlgItem(hDlg,IDD_BTN_SECTOR_VIEW),parm.hVDIsrc!=NULL);

   parm.flags = modeFlags;
   if (IsDlgButtonChecked(hDlg,IDD_UUID_KEEP)) parm.flags |= PARM_FLAG_KEEPUUID;
   if (IsDlgButtonChecked(hDlg,IDD_INCREASE_SIZE)) parm.flags |= PARM_FLAG_ENLARGE;
   if (IsDlgButtonChecked(hDlg,IDD_COMPACT)) parm.flags |= PARM_FLAG_COMPACT;
   UpdateModeControls(hDlg);
}

/*....................................................*/

static void
UpdateModeControls(HWND hDlg)
{
   BOOL inPlace=(parm.flags&PARM_FLAG_INPLACE)!=0;
   CheckRadioButton(hDlg,IDD_MODE_CLONE,IDD_MODE_INPLACE,inPlace ? IDD_MODE_INPLACE : IDD_MODE_CLONE);
   EnableWindow(GetDlgItem(hDlg,IDD_DEST_FN),!inPlace);
   EnableWindow(GetDlgItem(hDlg,IDD_BTN_DBROWSE),!inPlace);
   EnableWindow(GetDlgItem(hDlg,IDD_UUID_CHANGE),!inPlace);
   EnableWindow(GetDlgItem(hDlg,IDD_UUID_KEEP),!inPlace);
   EnableWindow(GetDlgItem(hDlg,IDD_INCREASE_PARTSIZE),!inPlace && (parm.flags&PARM_FLAG_ENLARGE)!=0);
   if (inPlace) {
      parm.flags&=~(PARM_FLAG_KEEPUUID|PARM_FLAG_REPART);
      CheckDlgButton(hDlg,IDD_INCREASE_PARTSIZE,FALSE);
   }
}

/*....................................................*/

static BOOL
ScanSourceEncryption(ENC_REPORT *report)
{
   HVDDR disk;
   VDDR_OpenMediaRegistry(parm.srcfn);
   disk=VDDR_Open(parm.srcfn,0);
   if (!disk) {
      ZeroMemory(report,sizeof(*report));
      report->complete=FALSE;
      return FALSE;
   }
   Encryption_Scan(disk,report);
   disk->Close(disk);
   return TRUE;
}

static void
CliEncryptionWarning(const ENC_REPORT *report)
{
   FILE err=(FILE)GetStdHandle(STD_ERROR_HANDLE);
   char detail[512],msg[768];
   if (!err || err==NULLFILE) return;
   Encryption_FormatReport(report,detail,sizeof(detail));
   wsprintf(msg,RSTR(ENC_CLI_WARN),detail);
   File_WrBin(err,msg,lstrlen(msg)); File_WrBin(err,"\r\n",2);
}

/*....................................................*/

static BOOL
DoItForHeavensSake(HWND hWndParent)
{
   ENC_REPORT report;
   BOOL cli=(parm.flags&PARM_FLAG_CLIMODE)!=0;
   if (parm.flags&PARM_FLAG_INPLACE) {
      BOOL recovering=VDIIP_HasJournal(parm.srcfn);
      if (recovering) {
         ZeroMemory(&report,sizeof(report)); report.complete=TRUE;
         parm.flags|=PARM_FLAG_FORCE_ENCRYPTED|PARM_FLAG_RESUME;
         if (!cli && !Env_AskRiskYN(RSTR(RECOVER_CONFIRM),RSTR(RISK_TITLE))) return FALSE;
      } else {
         ScanSourceEncryption(&report);
         if (!cli && !Env_AskRiskYN(RSTR(INPLACE_CONFIRM),RSTR(RISK_TITLE))) return FALSE;
         if (Encryption_HasWarning(&report)) {
            char detail[512],msg[1024];
            Encryption_FormatReport(&report,detail,sizeof(detail));
            if (cli) {
               CliEncryptionWarning(&report);
            } else {
               wsprintf(msg,RSTR(ENC_INPLACE_WARN),detail);
               if (!Env_AskRiskYN(msg,RSTR(RISK_TITLE))) return FALSE;
               parm.flags|=PARM_FLAG_FORCE_ENCRYPTED;
            }
         }
      }
      if (!VDIIP_Proceed(hInstApp,hWndParent,&parm,&report)) {
         char msg[1024]; wsprintf(msg,RSTR(INPLACE_FAILED),VDIIP_GetErrorString());
         return Error(msg);
      }
      return TRUE;
   }

   ScanSourceEncryption(&report);
   if (Encryption_HasWarning(&report)) {
      if (cli) CliEncryptionWarning(&report);
      else {
         char detail[512],msg[1024];
         Encryption_FormatReport(&report,detail,sizeof(detail));
         wsprintf(msg,RSTR(ENC_CLONE_WARN),detail);
         if (!Env_AskRiskYN(msg,RSTR(RISK_TITLE))) return FALSE;
      }
   }
   if (!Filename_IsExtension(parm.dstfn,"vdi")) {
      if (Filename_IsExtension(parm.dstfn,"vhd") || Filename_IsExtension(parm.dstfn,"vmdk") ||
          Filename_IsExtension(parm.dstfn,"raw") || Filename_IsExtension(parm.dstfn,"img")  ||
          Filename_IsExtension(parm.dstfn,"hdd")) {
         Filename_ChangeExtension(parm.dstfn,"vdi");
      } else {
         Filename_AddExtension(parm.dstfn,"vdi");
      }
   }
   return Clone_Proceed(hInstApp,hWndParent,&parm);
// return Clone_CompareImages(hInstApp,hWndParent,&parm);
}

/*....................................................*/

BOOL CALLBACK
DialogProc(HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
   static int busy;

   switch (iMsg) {
      case WM_INITDIALOG: {
         char sz[256],szFmt[256];

         GetWindowText(hDlg,szFmt,256);
         wsprintf(sz,szFmt,SOFTWARE_VERSION>>8,SOFTWARE_VERSION & 0xFF);
         SetWindowText(hDlg,sz);

         GetDlgItemText(hDlg,IDD_ABOUT_TEXT,szFmt,256);
         wsprintf(sz,szFmt,SOFTWARE_VERSION>>8,SOFTWARE_VERSION & 0xFF,COPYRIGHT_YEAR);
         SetDlgItemText(hDlg,IDD_ABOUT_TEXT,sz);

         Localization_PopulateLanguageCombo(GetDlgItem(hDlg,IDD_LANGUAGE_COMBO),languagePreference);
         Localization_ApplyMainDialog(hDlg,languagePreference);

         SetDlgItemText(hDlg,IDD_SOURCE_FN,parm.srcfn);
         SetDlgItemText(hDlg,IDD_DEST_FN,parm.dstfn);

         EnableWindow(GetDlgItem(hDlg,IDD_NEW_SIZE),(parm.flags & PARM_FLAG_ENLARGE)!=0);
         EnableWindow(GetDlgItem(hDlg,IDD_INCREASE_PARTSIZE),(parm.flags & PARM_FLAG_ENLARGE)!=0);

         CheckDlgButton(hDlg,IDD_UUID_KEEP,(parm.flags & PARM_FLAG_KEEPUUID)!=0);
         CheckDlgButton(hDlg,IDD_UUID_CHANGE,(parm.flags & PARM_FLAG_KEEPUUID)==0);

         if (bCompactOptionDefault) parm.flags |= PARM_FLAG_COMPACT;
         else parm.flags &= (~PARM_FLAG_COMPACT);
         CheckDlgButton(hDlg,IDD_COMPACT,(parm.flags & PARM_FLAG_COMPACT)!=0);
         UpdateModeControls(hDlg);

         if (parm.srcfn[0]) {
            OpenNewSource(hDlg,TRUE);
         } else {
            ShowDriveInfo(hDlg);
            SetDlgItemText(hDlg,IDD_VALID_RESULT,RSTR(PLSSELSRC));
         }
         EnableWindow(GetDlgItem(hDlg,IDD_BTN_PROCEED),parm.hVDIsrc!=NULL || VDIIP_HasJournal(parm.srcfn));
         EnableWindow(GetDlgItem(hDlg,IDD_BTN_PARTINFO),parm.hVDIsrc!=NULL);
         EnableWindow(GetDlgItem(hDlg,IDD_BTN_SECTOR_VIEW),parm.hVDIsrc!=NULL);

         busy = FALSE;
         return TRUE;
      }
      case WM_COMMAND: {
         UINT idCtl = LOWORD(wParam);
         busy++;
         if (busy==1) {
            switch (idCtl) {
               case IDD_SOURCE_FN:
                  if (HIWORD(wParam)==EN_KILLFOCUS) {
                     GetDlgItemText(hDlg, IDD_SOURCE_FN, tmpfn, 1024);
                     if (String_Compare(tmpfn,parm.srcfn)) {
                        lstrcpy(parm.srcfn, tmpfn);
                        Env_GenerateCloneName(parm.dstfn,parm.srcfn);
                        SetDlgItemText(hDlg,IDD_DEST_FN,parm.dstfn);
                        OpenNewSource(hDlg,TRUE);
                     }
                  }
                  break;
               case IDD_BTN_SBROWSE:
                  lstrcpy(tmpfn, parm.srcfn);
                  if (Env_BrowseFiles(hDlg,tmpfn,TRUE,NULL)) {
                     lstrcpy(parm.srcfn, tmpfn);
                     SetDlgItemText(hDlg,IDD_SOURCE_FN,parm.srcfn);
                     Env_GenerateCloneName(parm.dstfn,parm.srcfn);
                     SetDlgItemText(hDlg,IDD_DEST_FN,parm.dstfn);
                     OpenNewSource(hDlg,TRUE);
                  }
                  break;
               case IDD_BTN_DBROWSE:
                  if (Env_BrowseFiles(hDlg,parm.dstfn,FALSE,NULL)) {
                     SetDlgItemText(hDlg,IDD_DEST_FN,parm.dstfn);
                  }
                  break;
               case IDD_UUID_CHANGE:
                  parm.flags = (parm.flags & ~PARM_FLAG_KEEPUUID);
                  if (!IsDlgButtonChecked(hDlg,idCtl)) parm.flags |= PARM_FLAG_KEEPUUID;
                  CheckDlgButton(hDlg,IDD_UUID_KEEP,(parm.flags & PARM_FLAG_KEEPUUID)!=0);
                  break;
               case IDD_MODE_CLONE:
                  parm.flags&=~(PARM_FLAG_INPLACE|PARM_FLAG_RESUME|PARM_FLAG_FORCE_ENCRYPTED);
                  UpdateModeControls(hDlg);
                  break;
               case IDD_MODE_INPLACE:
                  parm.flags|=PARM_FLAG_INPLACE;
                  UpdateModeControls(hDlg);
                  break;
               case IDD_UUID_KEEP:
                  parm.flags = (parm.flags & ~PARM_FLAG_KEEPUUID);
                  if (IsDlgButtonChecked(hDlg,idCtl)) parm.flags |= PARM_FLAG_KEEPUUID;
                  CheckDlgButton(hDlg,IDD_UUID_CHANGE,(parm.flags & PARM_FLAG_KEEPUUID)==0);
                  break;
               case IDD_INCREASE_SIZE:
                  parm.flags &= (~PARM_FLAG_ENLARGE);
                  if (IsDlgButtonChecked(hDlg,idCtl)) parm.flags |= PARM_FLAG_ENLARGE;
                  EnableWindow(GetDlgItem(hDlg,IDD_NEW_SIZE),(parm.flags & PARM_FLAG_ENLARGE)!=0);
                  EnableWindow(GetDlgItem(hDlg,IDD_INCREASE_PARTSIZE),(parm.flags & PARM_FLAG_ENLARGE)!=0);
                  UpdateModeControls(hDlg);
                  if (!(parm.flags & PARM_FLAG_ENLARGE)) CheckDlgButton(hDlg,IDD_INCREASE_PARTSIZE,FALSE);
                  break;
               case IDD_COMPACT:
                  parm.flags &= (~PARM_FLAG_COMPACT);
                  if (IsDlgButtonChecked(hDlg,idCtl)) parm.flags |= PARM_FLAG_COMPACT;
                  break;
               case IDD_LANGUAGE_COMBO:
                  if (HIWORD(wParam)==CBN_SELCHANGE) {
                     char value[16];
                     languagePreference = Localization_GetLanguageComboValue(GetDlgItem(hDlg,IDD_LANGUAGE_COMBO));
                     wsprintf(value,"%d",languagePreference);
                     Profile_SetString(szLanguage,value);
                     Env_SetLanguage(hInstApp,Localization_ResolveLanguage(languagePreference));
                     Localization_ApplyMainDialog(hDlg,languagePreference);
                  }
                  break;
               case IDD_BTN_PARTINFO:
                  if (parm.hVDIsrc) {
                     PartInfo_Show(hInstApp,hDlg,parm.MBR);
                  }
                  break;
               case IDD_BTN_SECTOR_VIEW:
                  if (parm.hVDIsrc) {
                     SectorViewer_Show(hInstApp,hDlg,parm.hVDIsrc);
                  }
                  break;
               case IDD_BTN_HDRINFO:
                  if (parm.hVDIsrc) {
                     ShowHeader_Show(hInstApp,hDlg,parm.hVDIsrc);
                  }
                  break;
               case IDD_BTN_PROCEED:
                  if (parm.hVDIsrc || VDIIP_HasJournal(parm.srcfn)) {
                     if (IsDlgButtonChecked(hDlg,IDD_INCREASE_SIZE) && IsDlgButtonChecked(hDlg,IDD_INCREASE_PARTSIZE)) parm.flags |= PARM_FLAG_REPART;
                     if (parm.hVDIsrc) parm.hVDIsrc = parm.hVDIsrc->Close(parm.hVDIsrc);
                     GetDlgItemText(hDlg, IDD_SOURCE_FN, parm.srcfn, 1024);
                     GetDlgItemText(hDlg, IDD_DEST_FN, parm.dstfn, 1024);
                     GetDlgItemText(hDlg, IDD_NEW_SIZE, parm.szDestSize, 32);
                     DoItForHeavensSake(hDlg);
                     OpenNewSource(hDlg,FALSE);
                  } else {
                     Error(RSTR(NOSRC));
                  }
                  break;
               case IDD_BTN_EXIT: {
                  Profile_SetString(szSrcFileName,parm.srcfn);
                  Profile_SetString(szDstFileName,parm.dstfn);
                  DestroyWindow(hDlg);
                  break;
               }
            }
         }
         busy--;
         return TRUE;
      }
      case WM_CTLCOLORSTATIC:
         if ((HWND)lParam == GetDlgItem(hDlg,IDD_VALID_RESULT)) {
            char sz[256];
            GetDlgItemText(hDlg,IDD_VALID_RESULT,sz,256);
            if (lstrcmpi(sz,RSTR(OK))!=0) {
               HDC hDC = (HDC)wParam;
               SetBkMode(hDC,TRANSPARENT);
               SetTextColor(hDC,RGB(255,0,0));
               return (BOOL)GetSysColorBrush(COLOR_BTNFACE);
            }
         }
         break;
      case WM_CLOSE:
         if (!busy) DestroyWindow(hDlg);
         return TRUE;
      case WM_DESTROY:
         PostQuitMessage(0);
         return TRUE;
   }
   return FALSE;
}

/*.......................................................................*/

int WINAPI
WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR szCmdLine, int nCmdShow)
{
   int  rslt,argc=Env_ProcessCmdLine(hInstance, szCmdLine);
   char languageValue[16];

   hInstApp = hInstance;
   Thermo.RegisterClass(hInstance);
   Random_Randomize(GetTickCount());

   Profile_Init(hInstApp,szINIFileName); // tell Profile module where the .INI file is.

   Profile_GetString(szLanguage,languageValue,sizeof(languageValue),"-1");
   languagePreference = atoi(languageValue);
   Env_SetLanguage(hInstApp,Localization_ResolveLanguage(languagePreference));

   if (argc == 0) {
      HWND hWnd;
      MSG  msg;
      int status;
      
      parm.flags = 0;
      parm.hVDIsrc = NULL;
      Profile_GetString(szSrcFileName,parm.srcfn,MAX_PATH,"");
      Profile_GetString(szDstFileName,parm.dstfn,MAX_PATH,"");
      bCompactOptionDefault = (Profile_GetOption("Compact")!=0);

      HexView_RegisterClass(hInstance);

      hWnd = CreateDialogParamW(hInstApp, L"DLG_CLONEVDI", NULL, DialogProc, 0);

      if (!hWnd) {
         char buf[256];
         wsprintf (buf, RSTR(ERRAPPWND), GetLastError());
         MessageBox (0, buf, RSTR(ERROR), MB_ICONEXCLAMATION | MB_OK);
         return 1;
      }

      while ((status = GetMessage (& msg, 0, 0, 0)) != 0) {
         if (status==(-1)) break;
         if (!IsDialogMessage (hWnd, & msg)) {
            TranslateMessage ( & msg );
            DispatchMessage ( & msg );
         }
      }

      if (parm.hVDIsrc) parm.hVDIsrc->Close(parm.hVDIsrc);
      rslt = msg.wParam;
   } else {
      // CLI mode.
      AttachConsole(ATTACH_PARENT_PROCESS);
      rslt = 1;
      if (CmdLine_Parse(&parm)) {
         if (DoItForHeavensSake(NULL)) rslt = 0;
      }
   }

   Env_InitComAPI(FALSE);

   return rslt;
}

/*.......................................................................*/

/* end of clonevdi.c */


