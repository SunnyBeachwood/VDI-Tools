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
#include <commctrl.h>
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
#include "task.h"
#include "details.h"
#include "ids.h"

// fixed strings, these don't get localized
static PSTR szINIFileName = "VDI-Tools.ini"; // must not be longer than 31 chars.
static PSTR szSrcFileName = "SrcFile";
static PSTR szDstFileName = "DstFile";
static PSTR szLanguage    = "Language";
static PSTR szBatchThreads = "BatchThreads";
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
#define IDD_BTN_BATCH         303
#define IDD_BTN_PROCEED       1
#define IDD_BTN_EXIT          2

static HINSTANCE hInstApp;
static s_CLONEPARMS parm;
static FNCHAR tmpfn[1024];
static BOOL bCompactOptionDefault = FALSE;
static ENC_REPORT encryptionReport;

#define WM_BATCH_UPDATE (WM_APP+42)
#define BATCH_MAX_JOBS 128
#define IDB_LIST       500
#define IDB_ADD        501
#define IDB_REMOVE     502
#define IDB_START      503
#define IDB_CANCELALL  504
#define IDB_THREADS    505
#define IDB_CLEAR       506
#define IDB_DETAILS     507
#define IDB_CANCEL      508
#define IDM_THEME_LIGHT 610
#define IDM_THEME_DARK  611
#define IDM_THEME_TEAL  612
#define IDM_ABOUT       613

typedef struct {
   s_CLONEPARMS base;
   VDI_JOB jobs[BATCH_MAX_JOBS];
   UINT count;
   VDI_TASK_BATCH task;
   BOOL started;
   HWND hDlg;
} BATCH_UI;

static BATCH_UI batchUI;
static PSTR szTheme = "Theme";
static PSTR szWindowWidth = "WindowWidth";
static PSTR szWindowHeight = "WindowHeight";
static int themeIndex = 0;
static HBRUSH themeBrush;
static HBRUSH themePanelBrush;

typedef struct { COLORREF background, panel, text, accent; } APP_THEME;
static const APP_THEME themes[] = {
   { RGB(242,247,252), RGB(255,255,255), RGB(32,48,64), RGB(36,112,191) },
   { RGB(37,42,48), RGB(50,57,65), RGB(240,244,248), RGB(79,156,224) },
   { RGB(239,250,248), RGB(255,255,255), RGB(23,55,52), RGB(0,137,123) }
};

static void RestoreMainWindowSize(HWND hDlg)
{
   RECT work;
   int width=Profile_GetOptionDefault(szWindowWidth,1120);
   int height=Profile_GetOptionDefault(szWindowHeight,720);
   SystemParametersInfo(SPI_GETWORKAREA,0,&work,0);
   if (width<800) width=800;
   if (height<600) height=600;
   if (width>work.right-work.left) width=work.right-work.left;
   if (height>work.bottom-work.top) height=work.bottom-work.top;
   SetWindowPos(hDlg,NULL,0,0,width,height,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
}

static void SaveMainWindowSize(HWND hDlg)
{
   WINDOWPLACEMENT placement;
   char value[16];
   ZeroMemory(&placement,sizeof(placement)); placement.length=sizeof(placement);
   if (!GetWindowPlacement(hDlg,&placement)) return;
   wsprintf(value,"%ld",placement.rcNormalPosition.right-placement.rcNormalPosition.left);
   Profile_SetString(szWindowWidth,value);
   wsprintf(value,"%ld",placement.rcNormalPosition.bottom-placement.rcNormalPosition.top);
   Profile_SetString(szWindowHeight,value);
}

static void UpdateModeControls(HWND hDlg);

static const WCHAR *BatchStateNameW(LONG state)
{
   BOOL chinese=(Localization_ResolveLanguage(languagePreference)==VDI_LANGUAGE_ZH_CN);
   switch (state) {
      case VDI_JOB_WAITING: return chinese ? L"\x7B49\x5F85\x4E2D" : L"Waiting";
      case VDI_JOB_RUNNING: return chinese ? L"\x8FDB\x884C\x4E2D" : L"Running";
      case VDI_JOB_SUCCEEDED: return chinese ? L"\x5DF2\x5B8C\x6210" : L"Completed";
      case VDI_JOB_FAILED: return chinese ? L"\x5931\x8D25" : L"Failed";
      case VDI_JOB_SKIPPED: return chinese ? L"\x5DF2\x8DF3\x8FC7" : L"Skipped";
      case VDI_JOB_CANCELLED: return chinese ? L"\x5DF2\x53D6\x6D88" : L"Cancelled";
      case VDI_JOB_PAUSED: return chinese ? L"\x5DF2\x6682\x505C" : L"Paused - resume available";
   }
   return chinese ? L"\x672A\x77E5" : L"Unknown";
}

static void DrawProgressColumn(HDC hDC, HWND hWndList, int itemIndex, int subItemIndex, const VDI_JOB *job)
{
   RECT rcSubItem, rcBar, rcFill, rcClip;
   LONG permille;
   int percent, barWidth, fillWidth;
   char szPercent[32];
   HFONT hFont, hOldFont = NULL;
   COLORREF bgTrackColor, barFillColor, borderColor, textColorOnFill, textColorOnTrack;
   HBRUSH hBrushBg, hBrushFill, hOldBrush;
   HPEN hPenBorder, hOldPen;
   SIZE szText;
   int textX, textY, textLen;
   BOOL isSelected;

   ListView_GetSubItemRect(hWndList, itemIndex, subItemIndex, LVIR_BOUNDS, &rcSubItem);
   if (rcSubItem.right <= rcSubItem.left || rcSubItem.bottom <= rcSubItem.top) return;

   isSelected = (ListView_GetItemState(hWndList, itemIndex, LVIS_SELECTED) & LVIS_SELECTED) != 0;
   if (isSelected) {
      COLORREF selColor = (themeIndex == 1) ? RGB(60, 75, 95) : RGB(215, 232, 250);
      HBRUSH hSelBrush = CreateSolidBrush(selColor);
      FillRect(hDC, &rcSubItem, hSelBrush);
      DeleteObject(hSelBrush);
   } else {
      FillRect(hDC, &rcSubItem, themePanelBrush ? themePanelBrush : (HBRUSH)GetStockObject(WHITE_BRUSH));
   }

   permille = InterlockedCompareExchange((volatile LONG*)&job->progressPermille, 0, 0);
   if (job->state == VDI_JOB_SUCCEEDED) permille = 1000;
   else if (job->state == VDI_JOB_WAITING) permille = 0;
   if (permille < 0) permille = 0;
   if (permille > 1000) permille = 1000;
   percent = (int)(permille / 10);

   wsprintf(szPercent, "%d%%", percent);
   textLen = lstrlen(szPercent);

   rcBar = rcSubItem;
   rcBar.left += 4;
   rcBar.right -= 4;
   rcBar.top += 3;
   rcBar.bottom -= 3;
   if (rcBar.right <= rcBar.left || rcBar.bottom <= rcBar.top) return;

   barWidth = rcBar.right - rcBar.left;
   fillWidth = (barWidth * permille) / 1000;

   if (themeIndex == 1) {
      bgTrackColor = RGB(38, 44, 52);
      borderColor = RGB(65, 75, 88);
      textColorOnTrack = RGB(190, 200, 210);
      textColorOnFill = RGB(255, 255, 255);
      if (job->state == VDI_JOB_FAILED) barFillColor = RGB(220, 60, 60);
      else if (job->state == VDI_JOB_SUCCEEDED) barFillColor = RGB(46, 175, 80);
      else if (job->state == VDI_JOB_CANCELLED || job->state == VDI_JOB_PAUSED) barFillColor = RGB(120, 130, 140);
      else barFillColor = RGB(79, 156, 224);
   } else if (themeIndex == 2) {
      bgTrackColor = RGB(228, 243, 240);
      borderColor = RGB(170, 210, 205);
      textColorOnTrack = RGB(23, 55, 52);
      textColorOnFill = RGB(255, 255, 255);
      if (job->state == VDI_JOB_FAILED) barFillColor = RGB(220, 60, 60);
      else if (job->state == VDI_JOB_SUCCEEDED) barFillColor = RGB(46, 175, 80);
      else if (job->state == VDI_JOB_CANCELLED || job->state == VDI_JOB_PAUSED) barFillColor = RGB(140, 160, 158);
      else barFillColor = RGB(0, 137, 123);
   } else {
      bgTrackColor = RGB(232, 238, 245);
      borderColor = RGB(195, 208, 222);
      textColorOnTrack = RGB(40, 60, 80);
      textColorOnFill = RGB(255, 255, 255);
      if (job->state == VDI_JOB_FAILED) barFillColor = RGB(220, 60, 60);
      else if (job->state == VDI_JOB_SUCCEEDED) barFillColor = RGB(46, 175, 80);
      else if (job->state == VDI_JOB_CANCELLED || job->state == VDI_JOB_PAUSED) barFillColor = RGB(140, 150, 160);
      else barFillColor = RGB(36, 112, 191);
   }

   hBrushBg = CreateSolidBrush(bgTrackColor);
   hPenBorder = CreatePen(PS_SOLID, 1, borderColor);
   hOldBrush = (HBRUSH)SelectObject(hDC, hBrushBg);
   hOldPen = (HPEN)SelectObject(hDC, hPenBorder);
   RoundRect(hDC, rcBar.left, rcBar.top, rcBar.right, rcBar.bottom, 4, 4);
   SelectObject(hDC, hOldPen);
   SelectObject(hDC, hOldBrush);
   DeleteObject(hBrushBg);
   DeleteObject(hPenBorder);

   if (fillWidth > 0) {
      rcFill = rcBar;
      rcFill.left += 1;
      rcFill.top += 1;
      rcFill.right = rcBar.left + fillWidth;
      rcFill.bottom -= 1;
      if (rcFill.right > rcBar.right - 1) rcFill.right = rcBar.right - 1;

      hBrushFill = CreateSolidBrush(barFillColor);
      hPenBorder = CreatePen(PS_SOLID, 1, barFillColor);
      hOldBrush = (HBRUSH)SelectObject(hDC, hBrushFill);
      hOldPen = (HPEN)SelectObject(hDC, hPenBorder);
      RoundRect(hDC, rcFill.left, rcFill.top, rcFill.right + (rcFill.right < rcBar.right - 1 ? 2 : 0), rcFill.bottom + 1, 3, 3);
      SelectObject(hDC, hOldPen);
      SelectObject(hDC, hOldBrush);
      DeleteObject(hBrushFill);
      DeleteObject(hPenBorder);
   }

   hFont = (HFONT)SendMessage(hWndList, WM_GETFONT, 0, 0);
   if (!hFont) hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
   hOldFont = (HFONT)SelectObject(hDC, hFont);

   GetTextExtentPoint32(hDC, szPercent, textLen, &szText);
   textX = rcBar.left + (barWidth - szText.cx) / 2;
   textY = rcBar.top + (rcBar.bottom - rcBar.top - szText.cy) / 2;

   SetBkMode(hDC, TRANSPARENT);

   if (fillWidth > 0) {
      rcClip = rcBar;
      rcClip.right = rcBar.left + fillWidth;
      SetTextColor(hDC, textColorOnFill);
      ExtTextOut(hDC, textX, textY, ETO_CLIPPED, &rcClip, szPercent, textLen, NULL);
   }

   if (fillWidth < barWidth) {
      rcClip = rcBar;
      rcClip.left = rcBar.left + fillWidth;
      SetTextColor(hDC, textColorOnTrack);
      ExtTextOut(hDC, textX, textY, ETO_CLIPPED, &rcClip, szPercent, textLen, NULL);
   }

   if (hOldFont) SelectObject(hDC, hOldFont);
}

static void RefreshBatchList(HWND hDlg)
{
   UINT i;
   HWND list=GetDlgItem(hDlg,IDB_LIST);
   int currentCount;
   BOOL chinese;
   if (!list) return;

   chinese = (Localization_ResolveLanguage(languagePreference) == VDI_LANGUAGE_ZH_CN);
   currentCount = ListView_GetItemCount(list);

   while (currentCount > (int)batchUI.count) {
      currentCount--;
      ListView_DeleteItem(list, currentCount);
   }

   for (i = (UINT)currentCount; i < batchUI.count; i++) {
      LVITEM item;
      ZeroMemory(&item, sizeof(item));
      item.mask = LVIF_TEXT;
      item.iItem = (int)i;
      item.pszText = batchUI.jobs[i].parm.srcfn;
      ListView_InsertItem(list, &item);
   }

   for (i = 0; i < batchUI.count; i++) {
      VDI_JOB *job = &batchUI.jobs[i];
      char text[1400], progress[32], capacity[64];
      WCHAR wtext[1400], wcapacity[64];
      const WCHAR *opText = (job->parm.flags & PARM_FLAG_INPLACE) ?
                            (chinese ? L"\x539F\x5730\x4FEE\x6539" : L"In-place") :
                            (chinese ? L"\x514B\x9686" : L"Copy");
      LVITEMW itemW;

      ListView_SetItemText(list, (int)i, 0, job->parm.srcfn);
      ListView_SetItemText(list, (int)i, 1, job->parm.dstfn);

      ZeroMemory(&itemW, sizeof(itemW));
      itemW.mask = LVIF_TEXT;
      itemW.iItem = (int)i;

      itemW.iSubItem = 2;
      itemW.pszText = (LPWSTR)opText;
      SendMessageW(list, LVM_SETITEMTEXTW, (WPARAM)i, (LPARAM)&itemW);

      if (job->virtualBytes >= (1024LL * 1024LL * 1024LL)) {
         Env_DoubleToString(capacity, (double)job->virtualBytes / (1024.0 * 1024.0 * 1024.0), 2);
         lstrcat(capacity, " GB");
         MultiByteToWideChar(CP_ACP, 0, capacity, -1, wcapacity, 64);
      } else if (job->virtualBytes >= 0) {
         Env_DoubleToString(capacity, (double)job->virtualBytes / (1024.0 * 1024.0), 2);
         lstrcat(capacity, " MB");
         MultiByteToWideChar(CP_ACP, 0, capacity, -1, wcapacity, 64);
      } else {
         lstrcpyW(wcapacity, chinese ? L"\x4E0D\x53EF\x7528" : L"Unavailable");
      }
      itemW.iSubItem = 3;
      itemW.pszText = wcapacity;
      SendMessageW(list, LVM_SETITEMTEXTW, (WPARAM)i, (LPARAM)&itemW);

      itemW.iSubItem = 4;
      itemW.pszText = (LPWSTR)BatchStateNameW(job->state);
      SendMessageW(list, LVM_SETITEMTEXTW, (WPARAM)i, (LPARAM)&itemW);

      wsprintf(progress, "%ld%%", (long)(InterlockedCompareExchange((volatile LONG*)&job->progressPermille, 0, 0) / 10));
      ListView_SetItemText(list, (int)i, 5, progress);

      text[0] = 0;
      if (job->error[0]) lstrcpyn(text, job->error, sizeof(text));
      MultiByteToWideChar(CP_ACP, 0, text, -1, wtext, 1400);
      itemW.iSubItem = 6;
      itemW.pszText = wtext;
      SendMessageW(list, LVM_SETITEMTEXTW, (WPARAM)i, (LPARAM)&itemW);
   }

   InvalidateRect(list, NULL, FALSE);
}

static void RefreshLegacyBatchList(HWND hDlg)
{
   HWND list=GetDlgItem(hDlg,IDB_LIST);
   UINT i;
   if (!list) return;
   SendMessageA(list,LB_RESETCONTENT,0,0);
   for (i=0;i<batchUI.count;i++) SendMessageA(list,LB_ADDSTRING,0,(LPARAM)batchUI.jobs[i].parm.srcfn);
}

static void RefreshQueueList(HWND hDlg)
{
   HWND list=GetDlgItem(hDlg,IDB_LIST);
   char className[32];
   if (!list) return;
   GetClassNameA(list,className,sizeof(className));
   if (lstrcmpiA(className,"ListBox")==0) RefreshLegacyBatchList(hDlg);
   else RefreshBatchList(hDlg);
}

static void ApplyTheme(HWND hDlg, BOOL persist)
{
   const APP_THEME *theme=&themes[themeIndex];
   HWND list=GetDlgItem(hDlg,IDB_LIST);
   if (themeBrush) DeleteObject(themeBrush);
   if (themePanelBrush) DeleteObject(themePanelBrush);
   themeBrush=CreateSolidBrush(theme->background);
   themePanelBrush=CreateSolidBrush(theme->panel);
   if (list) {
      ListView_SetBkColor(list,theme->panel);
      ListView_SetTextBkColor(list,theme->panel);
      ListView_SetTextColor(list,theme->text);
      InvalidateRect(list,NULL,TRUE);
   }
   InvalidateRect(hDlg,NULL,TRUE);
   if (persist) { char value[8]; wsprintf(value,"%d",themeIndex); Profile_SetString(szTheme,value); }
}

static void InitQueueColumns(HWND hDlg)
{
   static const char *english[]={"Source file","Destination","Operation","Virtual size","Status","Progress","Details"};
   static const WCHAR *chinese[]={L"\x6E90\x6587\x4EF6",L"\x76EE\x6807\x6587\x4EF6",L"\x64CD\x4F5C",L"\x865A\x62DF\x5BB9\x91CF",L"\x72B6\x6001",L"\x8FDB\x5EA6",L"\x8BE6\x60C5"};
   static const int widths[]={220,220,70,85,90,105,180};
   LVCOLUMN column;
   int i;
   HWND list=GetDlgItem(hDlg,IDB_LIST);
   ListView_SetExtendedListViewStyle(list,LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER|LVS_EX_GRIDLINES);
   ZeroMemory(&column,sizeof(column)); column.mask=LVCF_TEXT|LVCF_WIDTH|LVCF_SUBITEM;
   for (i=0;i<7;i++) { column.pszText=(LPSTR)english[i]; column.cx=widths[i]; column.iSubItem=i; ListView_InsertColumn(list,i,&column); }
   if (Localization_ResolveLanguage(languagePreference)==VDI_LANGUAGE_ZH_CN) {
      LVCOLUMNW wide; ZeroMemory(&wide,sizeof(wide)); wide.mask=LVCF_TEXT;
      for (i=0;i<7;i++) { wide.pszText=(LPWSTR)chinese[i]; SendMessageW(list,LVM_SETCOLUMNW,i,(LPARAM)&wide); }
   }
}

static void ApplyModernLocalization(HWND hDlg)
{
   BOOL chinese=(Localization_ResolveLanguage(languagePreference)==VDI_LANGUAGE_ZH_CN);
   HWND list=GetDlgItem(hDlg,IDB_LIST);
   HMENU menu=GetMenu(hDlg);
   static const WCHAR *columnNamesZh[]={L"\x6E90\x6587\x4EF6",L"\x76EE\x6807\x6587\x4EF6",L"\x64CD\x4F5C",L"\x865A\x62DF\x5BB9\x91CF",L"\x72B6\x6001",L"\x8FDB\x5EA6",L"\x8BE6\x60C5"};
   static const char *columnNamesEn[]={"Source file","Destination","Operation","Virtual size","Status","Progress","Details"};
   int i;
   if (chinese) {
      SetWindowTextW(hDlg,L"VDI-Tools v6.00");
      SetDlgItemTextW(hDlg,400,L" \x4EFB\x52A1\x961F\x5217 "); SetDlgItemTextW(hDlg,IDB_ADD,L"\x6DFB\x52A0\x6587\x4EF6..."); SetDlgItemTextW(hDlg,IDB_REMOVE,L"\x79FB\x9664\x6240\x9009"); SetDlgItemTextW(hDlg,IDB_CLEAR,L"\x6E05\x7A7A"); SetDlgItemTextW(hDlg,IDB_DETAILS,L"\x67E5\x770B\x8BE6\x60C5");
      SetDlgItemTextW(hDlg,408,L" \x64CD\x4F5C\x8BBE\x7F6E "); SetDlgItemTextW(hDlg,214,L"\x521B\x5EFA\x65B0\x7684 VDI"); SetDlgItemTextW(hDlg,215,L"\x539F\x5730\x4FEE\x6539\x6E90 VDI"); SetDlgItemTextW(hDlg,207,L"\x751F\x6210\x65B0\x7684 UUID"); SetDlgItemTextW(hDlg,208,L"\x4FDD\x7559\x539F UUID"); SetDlgItemTextW(hDlg,209,L"\x5C06\x865A\x62DF\x78C1\x76D8\x5BB9\x91CF\x589E\x5927\x81F3"); SetDlgItemTextW(hDlg,211,L"\x6269\x5C55\x5206\x533A\x5BB9\x91CF"); SetDlgItemTextW(hDlg,212,L"\x538B\x7F29\x672A\x4F7F\x7528\x5757"); SetDlgItemTextW(hDlg,411,L"\x8BED\x8A00:");
      SetDlgItemTextW(hDlg,409,L" \x6267\x884C "); SetDlgItemTextW(hDlg,204,L"\x6240\x9009\x4EFB\x52A1\x7EDF\x4E00\x4F7F\x7528\x5DE6\x4FA7\x8BBE\x7F6E\x3002"); SetDlgItemTextW(hDlg,410,L"\x7EBF\x7A0B\x6570:"); SetDlgItemTextW(hDlg,IDB_START,L"\x5F00\x59CB\x961F\x5217"); SetDlgItemTextW(hDlg,IDB_CANCEL,L"\x53D6\x6D88\x6240\x9009"); SetDlgItemTextW(hDlg,IDB_CANCELALL,L"\x53D6\x6D88\x5168\x90E8"); SetDlgItemTextW(hDlg,2,L"\x9000\x51FA"); SetDlgItemTextW(hDlg,206,L"\x6DFB\x52A0\x4E00\x4E2A\x6216\x591A\x4E2A\x865A\x62DF\x78C1\x76D8\x540E\x5F00\x59CB\x3002");
      if (menu) {
         ModifyMenuW(menu,0,MF_BYPOSITION|MF_POPUP|MF_STRING,(UINT_PTR)GetSubMenu(menu,0),L"\x6587\x4EF6"); ModifyMenuW(menu,1,MF_BYPOSITION|MF_POPUP|MF_STRING,(UINT_PTR)GetSubMenu(menu,1),L"\x67E5\x770B"); ModifyMenuW(menu,2,MF_BYPOSITION|MF_POPUP|MF_STRING,(UINT_PTR)GetSubMenu(menu,2),L"\x4E3B\x9898"); ModifyMenuW(menu,3,MF_BYPOSITION|MF_POPUP|MF_STRING,(UINT_PTR)GetSubMenu(menu,3),L"\x5E2E\x52A9");
         ModifyMenuW(GetSubMenu(menu,0),0,MF_BYPOSITION|MF_STRING,IDB_ADD,L"\x6DFB\x52A0\x6587\x4EF6..."); ModifyMenuW(GetSubMenu(menu,0),1,MF_BYPOSITION|MF_STRING,IDB_REMOVE,L"\x79FB\x9664\x6240\x9009"); ModifyMenuW(GetSubMenu(menu,0),2,MF_BYPOSITION|MF_STRING,IDB_CLEAR,L"\x6E05\x7A7A"); ModifyMenuW(GetSubMenu(menu,0),4,MF_BYPOSITION|MF_STRING,2,L"\x9000\x51FA");
         ModifyMenuW(GetSubMenu(menu,1),0,MF_BYPOSITION|MF_STRING,IDB_DETAILS,L"\x6240\x9009\x6587\x4EF6\x8BE6\x60C5"); ModifyMenuW(GetSubMenu(menu,2),0,MF_BYPOSITION|MF_STRING,IDM_THEME_LIGHT,L"\x6D45\x8272\x84DD\x7070"); ModifyMenuW(GetSubMenu(menu,2),1,MF_BYPOSITION|MF_STRING,IDM_THEME_DARK,L"\x6DF1\x8272\x77F3\x58A8"); ModifyMenuW(GetSubMenu(menu,2),2,MF_BYPOSITION|MF_STRING,IDM_THEME_TEAL,L"\x9752\x7EFF\x8272"); ModifyMenuW(GetSubMenu(menu,3),0,MF_BYPOSITION|MF_STRING,IDM_ABOUT,L"\x5173\x4E8E VDI-Tools"); DrawMenuBar(hDlg);
      }
      for (i=0;i<7;i++) { LVCOLUMNW c; ZeroMemory(&c,sizeof(c)); c.mask=LVCF_TEXT; c.pszText=(LPWSTR)columnNamesZh[i]; SendMessageW(list,LVM_SETCOLUMNW,i,(LPARAM)&c); }
   } else {
      SetWindowTextA(hDlg,"VDI-Tools v6.00");
      SetDlgItemTextA(hDlg,400," Task queue "); SetDlgItemTextA(hDlg,IDB_ADD,"Add files..."); SetDlgItemTextA(hDlg,IDB_REMOVE,"Remove selected"); SetDlgItemTextA(hDlg,IDB_CLEAR,"Clear"); SetDlgItemTextA(hDlg,IDB_DETAILS,"View details");
      SetDlgItemTextA(hDlg,408," Operation settings "); SetDlgItemTextA(hDlg,214,"Create a new VDI"); SetDlgItemTextA(hDlg,215,"Modify source VDI in place"); SetDlgItemTextA(hDlg,207,"Generate new UUID"); SetDlgItemTextA(hDlg,208,"Keep old UUID"); SetDlgItemTextA(hDlg,209,"Increase virtual drive size to"); SetDlgItemTextA(hDlg,211,"Increase partition size"); SetDlgItemTextA(hDlg,212,"Compact unused blocks"); SetDlgItemTextA(hDlg,411,"Language:");
      SetDlgItemTextA(hDlg,409," Execution "); SetDlgItemTextA(hDlg,204,"Selected tasks use the settings on the left."); SetDlgItemTextA(hDlg,410,"Threads:"); SetDlgItemTextA(hDlg,IDB_START,"Start queue"); SetDlgItemTextA(hDlg,IDB_CANCEL,"Cancel selected"); SetDlgItemTextA(hDlg,IDB_CANCELALL,"Cancel all"); SetDlgItemTextA(hDlg,2,"Exit"); SetDlgItemTextA(hDlg,206,"Add one or more virtual disks to begin.");
      if (menu) {
         ModifyMenuA(menu,0,MF_BYPOSITION|MF_POPUP|MF_STRING,(UINT_PTR)GetSubMenu(menu,0),"File"); ModifyMenuA(menu,1,MF_BYPOSITION|MF_POPUP|MF_STRING,(UINT_PTR)GetSubMenu(menu,1),"View"); ModifyMenuA(menu,2,MF_BYPOSITION|MF_POPUP|MF_STRING,(UINT_PTR)GetSubMenu(menu,2),"Theme"); ModifyMenuA(menu,3,MF_BYPOSITION|MF_POPUP|MF_STRING,(UINT_PTR)GetSubMenu(menu,3),"Help");
         ModifyMenuA(GetSubMenu(menu,0),0,MF_BYPOSITION|MF_STRING,IDB_ADD,"Add files..."); ModifyMenuA(GetSubMenu(menu,0),1,MF_BYPOSITION|MF_STRING,IDB_REMOVE,"Remove selected"); ModifyMenuA(GetSubMenu(menu,0),2,MF_BYPOSITION|MF_STRING,IDB_CLEAR,"Clear"); ModifyMenuA(GetSubMenu(menu,0),4,MF_BYPOSITION|MF_STRING,2,"Exit");
         ModifyMenuA(GetSubMenu(menu,1),0,MF_BYPOSITION|MF_STRING,IDB_DETAILS,"Details for selected file"); ModifyMenuA(GetSubMenu(menu,2),0,MF_BYPOSITION|MF_STRING,IDM_THEME_LIGHT,"Light blue-gray"); ModifyMenuA(GetSubMenu(menu,2),1,MF_BYPOSITION|MF_STRING,IDM_THEME_DARK,"Dark graphite"); ModifyMenuA(GetSubMenu(menu,2),2,MF_BYPOSITION|MF_STRING,IDM_THEME_TEAL,"Teal"); ModifyMenuA(GetSubMenu(menu,3),0,MF_BYPOSITION|MF_STRING,IDM_ABOUT,"About VDI-Tools"); DrawMenuBar(hDlg);
      }
      for (i=0;i<7;i++) { LVCOLUMNA c; ZeroMemory(&c,sizeof(c)); c.mask=LVCF_TEXT; c.pszText=(LPSTR)columnNamesEn[i]; SendMessageA(list,LVM_SETCOLUMNA,i,(LPARAM)&c); }
   }
}

static void PopulateJobSize(VDI_JOB *job)
{
   HVDDR disk;
   HUGE size=0;
   job->virtualBytes=-1;
   VDDR_OpenMediaRegistry(job->parm.srcfn);
   disk=VDDR_Open(job->parm.srcfn,0);
   if (disk && disk->GetDriveSize(disk,&size)) job->virtualBytes=(LONGLONG)size;
   if (disk) disk->Close(disk);
}

static void LayoutMain(HWND hDlg)
{
   RECT r; int w,h,queueBottom,settingsTop,leftW,rightX,rightW,bottomH;
   GetClientRect(hDlg,&r); w=r.right; h=r.bottom;
   queueBottom=(h*45)/100;
   settingsTop=queueBottom+38;
   leftW=(w*62)/100-16; rightX=(w*62)/100; rightW=w-rightX-8; bottomH=h-settingsTop-12;
   SetWindowPos(GetDlgItem(hDlg,400),NULL,8,8,w-16,queueBottom-8,SWP_NOZORDER);
   SetWindowPos(GetDlgItem(hDlg,IDB_LIST),NULL,16,31,w-32,queueBottom-75,SWP_NOZORDER);
   SetWindowPos(GetDlgItem(hDlg,IDB_ADD),NULL,16,queueBottom-36,80,24,SWP_NOZORDER);
   SetWindowPos(GetDlgItem(hDlg,IDB_REMOVE),NULL,104,queueBottom-36,105,24,SWP_NOZORDER);
   SetWindowPos(GetDlgItem(hDlg,IDB_CLEAR),NULL,217,queueBottom-36,65,24,SWP_NOZORDER);
   SetWindowPos(GetDlgItem(hDlg,IDB_DETAILS),NULL,w-130,queueBottom-36,114,24,SWP_NOZORDER);
   SetWindowPos(GetDlgItem(hDlg,408),NULL,8,settingsTop,leftW,bottomH,SWP_NOZORDER);
   SetWindowPos(GetDlgItem(hDlg,409),NULL,rightX,settingsTop,rightW,bottomH,SWP_NOZORDER);
   SetWindowPos(GetDlgItem(hDlg,IDD_MODE_CLONE),NULL,20,settingsTop+25,145,22,SWP_NOZORDER); SetWindowPos(GetDlgItem(hDlg,IDD_MODE_INPLACE),NULL,175,settingsTop+25,leftW-185,22,SWP_NOZORDER);
   SetWindowPos(GetDlgItem(hDlg,IDD_UUID_CHANGE),NULL,20,settingsTop+58,150,22,SWP_NOZORDER); SetWindowPos(GetDlgItem(hDlg,IDD_UUID_KEEP),NULL,20,settingsTop+84,150,22,SWP_NOZORDER);
   SetWindowPos(GetDlgItem(hDlg,IDD_INCREASE_SIZE),NULL,20,settingsTop+110,190,22,SWP_NOZORDER); SetWindowPos(GetDlgItem(hDlg,IDD_NEW_SIZE),NULL,220,settingsTop+108,90,22,SWP_NOZORDER);
   SetWindowPos(GetDlgItem(hDlg,IDD_INCREASE_PARTSIZE),NULL,20,settingsTop+136,180,22,SWP_NOZORDER); SetWindowPos(GetDlgItem(hDlg,IDD_COMPACT),NULL,20,settingsTop+162,180,22,SWP_NOZORDER);
   SetWindowPos(GetDlgItem(hDlg,411),NULL,20,settingsTop+bottomH-34,55,20,SWP_NOZORDER); SetWindowPos(GetDlgItem(hDlg,IDD_LANGUAGE_COMBO),NULL,78,settingsTop+bottomH-36,leftW-90,22,SWP_NOZORDER);
   SetWindowPos(GetDlgItem(hDlg,IDD_VALID_RESULT),NULL,rightX+16,settingsTop+25,rightW-32,22,SWP_NOZORDER); SetWindowPos(GetDlgItem(hDlg,410),NULL,rightX+16,settingsTop+58,55,20,SWP_NOZORDER); SetWindowPos(GetDlgItem(hDlg,IDB_THREADS),NULL,rightX+75,settingsTop+56,50,22,SWP_NOZORDER);
   SetWindowPos(GetDlgItem(hDlg,IDB_START),NULL,rightX+16,settingsTop+92,(rightW-48)/2,30,SWP_NOZORDER); SetWindowPos(GetDlgItem(hDlg,IDB_CANCEL),NULL,rightX+32+(rightW-48)/2,settingsTop+92,(rightW-48)/2,30,SWP_NOZORDER);
   SetWindowPos(GetDlgItem(hDlg,IDB_CANCELALL),NULL,rightX+16,settingsTop+132,(rightW-48)/2,30,SWP_NOZORDER); SetWindowPos(GetDlgItem(hDlg,2),NULL,rightX+32+(rightW-48)/2,settingsTop+132,(rightW-48)/2,30,SWP_NOZORDER);
   SetWindowPos(GetDlgItem(hDlg,IDD_FILESYSTEM),NULL,rightX+16,settingsTop+bottomH-40,rightW-32,24,SWP_NOZORDER);
}

static void BatchNotify(void *context, VDI_JOB *job)
{
   HWND hDlg=(HWND)context;
   (void)job;
   if (IsWindow(hDlg)) PostMessage(hDlg,WM_BATCH_UPDATE,0,0);
}

static void AddBatchFiles(HWND hDlg, const s_CLONEPARMS *base)
{
   OPENFILENAME ofn;
   static char names[65536];
   char *p;
   ZeroMemory(&ofn,sizeof(ofn)); ZeroMemory(names,sizeof(names));
   ofn.lStructSize=sizeof(ofn); ofn.hwndOwner=hDlg; ofn.lpstrFile=names; ofn.nMaxFile=sizeof(names);
   ofn.lpstrFilter="Virtual drive files\0*.vdi;*.vhd;*.vmdk;*.raw;*.img;*.hdd\0\0";
   ofn.Flags=OFN_EXPLORER|OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_ALLOWMULTISELECT;
   if (!GetOpenFileName(&ofn)) return;
   p=names+lstrlen(names)+1;
   if (!*p) p=names;
   while (*p && batchUI.count<BATCH_MAX_JOBS) {
      VDI_JOB *job=&batchUI.jobs[batchUI.count];
      char full[1024];
      ZeroMemory(job,sizeof(*job)); job->parm=*base; job->parm.hVDIsrc=NULL;
      if (p==names) lstrcpyn(full,p,sizeof(full));
      else { lstrcpyn(full,names,sizeof(full)); lstrcat(full,"\\"); lstrcat(full,p); }
      GetFullPathName(full,1024,job->parm.srcfn,0);
      if (job->parm.flags&PARM_FLAG_INPLACE) String_Copy(job->parm.dstfn,job->parm.srcfn,1024);
      else Env_GenerateCloneName(job->parm.dstfn,job->parm.srcfn);
      PopulateJobSize(job);
      batchUI.count++;
      if (p==names) break;
      p+=lstrlen(p)+1;
   }
   RefreshQueueList(hDlg);
}

static BOOL BatchCanStart(HWND hDlg)
{
   UINT i,conflicts=0;
   if (!batchUI.count) return FALSE;
   if (batchUI.jobs[0].parm.flags&PARM_FLAG_INPLACE) {
      if (MessageBox(hDlg,"Each selected VDI will be modified in place. Confirm that every source is backed up and no VM is using it.","Batch in-place modification",MB_ICONWARNING|MB_OKCANCEL)!=IDOK) return FALSE;
   }
   for (i=0;i<batchUI.count;i++) {
      UINT j;
      for (j=i+1;j<batchUI.count;j++) {
         if (Filename_Compare(batchUI.jobs[i].parm.srcfn,batchUI.jobs[j].parm.srcfn)==0 ||
             (!(batchUI.jobs[i].parm.flags&PARM_FLAG_INPLACE) && Filename_Compare(batchUI.jobs[i].parm.dstfn,batchUI.jobs[j].parm.dstfn)==0)) {
            MessageBox(hDlg,"The batch contains duplicate source files or generated destination names.",RSTR(ERROR),MB_ICONEXCLAMATION|MB_OK);
            return FALSE;
         }
      }
   }
   for (i=0;i<batchUI.count;i++) if (!(batchUI.jobs[i].parm.flags&PARM_FLAG_INPLACE) && GetFileAttributes(batchUI.jobs[i].parm.dstfn)!=INVALID_FILE_ATTRIBUTES) conflicts++;
   if (conflicts && MessageBox(hDlg,"One or more generated destination files already exist. Overwrite every conflicting destination only after its temporary output completes?",RSTR(ERROR),MB_ICONWARNING|MB_YESNO)!=IDYES) return FALSE;
   if (conflicts) for (i=0;i<batchUI.count;i++) batchUI.jobs[i].overwriteExisting=TRUE;
   return TRUE;
}

static BOOL CALLBACK BatchDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
   switch (message) {
      case WM_INITDIALOG: {
         s_CLONEPARMS *base=(s_CLONEPARMS*)lParam;
         UINT saved;
         ZeroMemory(&batchUI,sizeof(batchUI)); batchUI.hDlg=hDlg; batchUI.base=*base;
         saved=(UINT)Profile_GetOptionDefault(szBatchThreads,(int)Task_DefaultThreadLimit());
         if (saved<1 || saved>32) saved=Task_DefaultThreadLimit();
         SetDlgItemInt(hDlg,IDB_THREADS,saved,FALSE);
         AddBatchFiles(hDlg,&batchUI.base);
         return TRUE;
      }
      case WM_COMMAND:
         switch (LOWORD(wParam)) {
            case IDB_ADD: {
               AddBatchFiles(hDlg,&batchUI.base); return TRUE;
            }
            case IDB_REMOVE: {
               LRESULT index=SendMessage(GetDlgItem(hDlg,IDB_LIST),LB_GETCURSEL,0,0);
               if (!batchUI.started && index!=LB_ERR) {
                  UINT i=(UINT)index;
                  for (;i+1<batchUI.count;i++) batchUI.jobs[i]=batchUI.jobs[i+1];
                  batchUI.count--; RefreshQueueList(hDlg);
               }
               return TRUE;
            }
            case IDB_START: {
               BOOL translated=FALSE;
               UINT threads=GetDlgItemInt(hDlg,IDB_THREADS,&translated,FALSE);
               if (!translated || threads<1 || threads>32) { MessageBox(hDlg,"Thread limit must be between 1 and 32.",RSTR(ERROR),MB_ICONEXCLAMATION|MB_OK); return TRUE; }
               { char value[12]; wsprintf(value,"%lu",threads); Profile_SetString(szBatchThreads,value); }
               if (!BatchCanStart(hDlg)) return TRUE;
               if (!TaskBatch_Start(&batchUI.task,batchUI.jobs,batchUI.count,threads,BatchNotify,hDlg)) { MessageBox(hDlg,"Could not start the batch worker threads.",RSTR(ERROR),MB_ICONEXCLAMATION|MB_OK); return TRUE; }
               batchUI.started=TRUE; EnableWindow(GetDlgItem(hDlg,IDB_ADD),FALSE); EnableWindow(GetDlgItem(hDlg,IDB_REMOVE),FALSE); EnableWindow(GetDlgItem(hDlg,IDB_START),FALSE);
               SetTimer(hDlg,1,250,NULL); RefreshQueueList(hDlg); return TRUE;
            }
            case IDB_CANCELALL:
               if (batchUI.started) TaskBatch_RequestCancelAll(&batchUI.task);
               return TRUE;
            case IDCANCEL:
               if (batchUI.started) { TaskBatch_RequestCancelAll(&batchUI.task); return TRUE; }
               EndDialog(hDlg,0); return TRUE;
         }
         break;
      case WM_BATCH_UPDATE:
         RefreshQueueList(hDlg); return TRUE;
      case WM_TIMER:
         RefreshQueueList(hDlg);
         if (batchUI.started && TaskBatch_IsComplete(&batchUI.task)) {
            KillTimer(hDlg,1); TaskBatch_Destroy(&batchUI.task); batchUI.started=FALSE;
            EnableWindow(GetDlgItem(hDlg,IDB_CANCELALL),FALSE);
         }
         return TRUE;
      case WM_CLOSE:
         if (batchUI.started) { TaskBatch_RequestCancelAll(&batchUI.task); return TRUE; }
         EndDialog(hDlg,0); return TRUE;
      case WM_DESTROY:
         if (batchUI.started) { TaskBatch_RequestCancelAll(&batchUI.task); TaskBatch_Wait(&batchUI.task); TaskBatch_Destroy(&batchUI.task); }
         return TRUE;
   }
   return FALSE;
}

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
      parm.flags |= PARM_FLAG_KEEPUUID;
      parm.flags &= ~PARM_FLAG_REPART;
      CheckDlgButton(hDlg,IDD_INCREASE_PARTSIZE,FALSE);
      CheckRadioButton(hDlg,IDD_UUID_CHANGE,IDD_UUID_KEEP,IDD_UUID_KEEP);
   } else {
      CheckRadioButton(hDlg,IDD_UUID_CHANGE,IDD_UUID_KEEP,(parm.flags&PARM_FLAG_KEEPUUID) ? IDD_UUID_KEEP : IDD_UUID_CHANGE);
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

static int
RunBatchCommandLine(s_CLONEPARMS *base)
{
   UINT i,count=CmdLine_BatchSourceCount(),failed=0;
   CPFN outputDir=CmdLine_BatchOutputDir();
   VDI_JOB *jobs;
   VDI_TASK_BATCH batch;
   FILE err=(FILE)GetStdHandle(STD_ERROR_HANDLE);
   if (count<2) {
      if (count==1 && outputDir) {
         FNCHAR tail[1024];
         Filename_SplitTail(base->dstfn,tail);
         Filename_MakePath(base->dstfn,outputDir,tail);
      }
      return DoItForHeavensSake(NULL) ? 0 : 1;
   }
   jobs=(VDI_JOB*)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(VDI_JOB)*count);
   if (!jobs) return 1;
   for (i=0;i<count;i++) {
      FNCHAR tail[1024];
      jobs[i].parm=*base;
      String_Copy(jobs[i].parm.srcfn,CmdLine_BatchSource(i),1024);
      if (jobs[i].parm.flags&PARM_FLAG_INPLACE) {
         String_Copy(jobs[i].parm.dstfn,jobs[i].parm.srcfn,1024);
      } else {
         Env_GenerateCloneName(jobs[i].parm.dstfn,jobs[i].parm.srcfn);
         if (outputDir) {
            Filename_SplitTail(jobs[i].parm.dstfn,tail);
            Filename_MakePath(jobs[i].parm.dstfn,outputDir,tail);
         }
      }
   }
   if (!TaskBatch_Start(&batch,jobs,count,CmdLine_BatchThreadLimit(),NULL,NULL)) {
      HeapFree(GetProcessHeap(),0,jobs); return 1;
   }
   TaskBatch_Wait(&batch);
   for (i=0;i<count;i++) {
      if (jobs[i].state!=VDI_JOB_SUCCEEDED) {
         failed++;
         if (err && err!=NULLFILE) {
            char msg[1400];
            wsprintf(msg,"%s: %s\r\n",jobs[i].parm.srcfn,jobs[i].error[0] ? jobs[i].error : "not completed");
            File_WrBin(err,msg,lstrlen(msg));
         }
      }
   }
   TaskBatch_Destroy(&batch);
   HeapFree(GetProcessHeap(),0,jobs);
   return failed ? 1 : 0;
}

/*....................................................*/

BOOL CALLBACK
LegacyDialogProc(HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam)
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
               case IDD_BTN_BATCH: {
                  s_CLONEPARMS base=parm;
                  base.hVDIsrc=NULL;
                  GetDlgItemText(hDlg,IDD_NEW_SIZE,base.szDestSize,32);
                  if (IsDlgButtonChecked(hDlg,IDD_INCREASE_SIZE)) base.flags|=PARM_FLAG_ENLARGE;
                  else base.flags&=~PARM_FLAG_ENLARGE;
                  if (IsDlgButtonChecked(hDlg,IDD_COMPACT)) base.flags|=PARM_FLAG_COMPACT;
                  else base.flags&=~PARM_FLAG_COMPACT;
                  if (IsDlgButtonChecked(hDlg,IDD_MODE_INPLACE)) base.flags|=PARM_FLAG_INPLACE;
                  else base.flags&=~PARM_FLAG_INPLACE;
                  DialogBoxParam(hInstApp,"DLG_BATCH",hDlg,BatchDialogProc,(LPARAM)&base);
                  break;
               }
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

static void ApplyQueueOptions(HWND hDlg)
{
   UINT i;
   s_CLONEPARMS base=parm;
   base.hVDIsrc=NULL;
   if (IsDlgButtonChecked(hDlg,IDD_MODE_INPLACE)) {
      base.flags|=PARM_FLAG_INPLACE;
      base.flags|=PARM_FLAG_KEEPUUID;
      base.flags&=~PARM_FLAG_REPART;
   } else {
      base.flags&=~(PARM_FLAG_INPLACE|PARM_FLAG_RESUME|PARM_FLAG_FORCE_ENCRYPTED);
      if (IsDlgButtonChecked(hDlg,IDD_UUID_KEEP)) base.flags|=PARM_FLAG_KEEPUUID; else base.flags&=~PARM_FLAG_KEEPUUID;
      if (IsDlgButtonChecked(hDlg,IDD_INCREASE_PARTSIZE)) base.flags|=PARM_FLAG_REPART; else base.flags&=~PARM_FLAG_REPART;
   }
   if (IsDlgButtonChecked(hDlg,IDD_INCREASE_SIZE)) base.flags|=PARM_FLAG_ENLARGE; else base.flags&=~PARM_FLAG_ENLARGE;
   if (IsDlgButtonChecked(hDlg,IDD_COMPACT)) base.flags|=PARM_FLAG_COMPACT; else base.flags&=~PARM_FLAG_COMPACT;
   GetDlgItemText(hDlg,IDD_NEW_SIZE,base.szDestSize,sizeof(base.szDestSize));
   for (i=0;i<batchUI.count;i++) {
      FNCHAR source[1024],destination[1024];
      String_Copy(source,batchUI.jobs[i].parm.srcfn,1024);
      String_Copy(destination,batchUI.jobs[i].parm.dstfn,1024);
      batchUI.jobs[i].parm=base;
      String_Copy(batchUI.jobs[i].parm.srcfn,source,1024);
      if (base.flags&PARM_FLAG_INPLACE) String_Copy(batchUI.jobs[i].parm.dstfn,source,1024);
      else if (destination[0]) String_Copy(batchUI.jobs[i].parm.dstfn,destination,1024);
      else Env_GenerateCloneName(batchUI.jobs[i].parm.dstfn,source);
   }
}

static void RemoveSelectedJobs(HWND hDlg)
{
   HWND list=GetDlgItem(hDlg,IDB_LIST);
   int i, writeIdx=0;
   if (batchUI.started || !list) return;
   for (i=0; i<(int)batchUI.count; i++) {
      if (ListView_GetItemState(list, i, LVIS_SELECTED) & LVIS_SELECTED) {
         // Selected, remove
      } else {
         if (writeIdx != i) {
            batchUI.jobs[writeIdx] = batchUI.jobs[i];
         }
         writeIdx++;
      }
   }
   batchUI.count = (UINT)writeIdx;
   RefreshBatchList(hDlg);
}

static void StartQueue(HWND hDlg)
{
   BOOL valid=FALSE;
   UINT threads=GetDlgItemInt(hDlg,IDB_THREADS,&valid,FALSE);
   if (!valid || threads<1 || threads>32) { MessageBox(hDlg,"Thread limit must be between 1 and 32.",RSTR(ERROR),MB_ICONEXCLAMATION|MB_OK); return; }
   ApplyQueueOptions(hDlg);
   if (!BatchCanStart(hDlg)) return;
   { char value[12]; wsprintf(value,"%lu",threads); Profile_SetString(szBatchThreads,value); }
   if (!TaskBatch_Start(&batchUI.task,batchUI.jobs,batchUI.count,threads,BatchNotify,hDlg)) { MessageBox(hDlg,"Could not start the task workers.",RSTR(ERROR),MB_ICONEXCLAMATION|MB_OK); return; }
   batchUI.started=TRUE;
   EnableWindow(GetDlgItem(hDlg,IDB_ADD),FALSE); EnableWindow(GetDlgItem(hDlg,IDB_REMOVE),FALSE); EnableWindow(GetDlgItem(hDlg,IDB_CLEAR),FALSE); EnableWindow(GetDlgItem(hDlg,IDB_START),FALSE);
   SetTimer(hDlg,1,200,NULL); RefreshBatchList(hDlg);
}

BOOL CALLBACK
DialogProc(HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
   switch (iMsg) {
      case WM_INITDIALOG: {
         char title[256],fmt[256],value[16]; UINT saved;
         GetWindowText(hDlg,fmt,sizeof(fmt)); wsprintf(title,fmt,SOFTWARE_VERSION>>8,SOFTWARE_VERSION&0xff); SetWindowText(hDlg,title);
         SetMenu(hDlg,LoadMenu(hInstApp,"MAIN_MENU")); DrawMenuBar(hDlg);
         ZeroMemory(&batchUI,sizeof(batchUI)); batchUI.hDlg=hDlg; batchUI.base=parm;
         InitQueueColumns(hDlg);
         saved=(UINT)Profile_GetOptionDefault(szBatchThreads,(int)Task_DefaultThreadLimit()); if (saved<1 || saved>32) saved=Task_DefaultThreadLimit(); SetDlgItemInt(hDlg,IDB_THREADS,saved,FALSE);
         Profile_GetString(szTheme,value,sizeof(value),"0"); themeIndex=atoi(value); if (themeIndex<0 || themeIndex>2) themeIndex=0;
         Localization_PopulateLanguageCombo(GetDlgItem(hDlg,IDD_LANGUAGE_COMBO),languagePreference);
         CheckRadioButton(hDlg,IDD_UUID_CHANGE,IDD_UUID_KEEP,(parm.flags&PARM_FLAG_KEEPUUID) ? IDD_UUID_KEEP : IDD_UUID_CHANGE);
         if (bCompactOptionDefault) parm.flags|=PARM_FLAG_COMPACT; CheckDlgButton(hDlg,IDD_COMPACT,(parm.flags&PARM_FLAG_COMPACT)!=0);
         UpdateModeControls(hDlg); RestoreMainWindowSize(hDlg); LayoutMain(hDlg); ApplyTheme(hDlg,FALSE); ApplyModernLocalization(hDlg); RefreshBatchList(hDlg);
         return TRUE;
      }
      case WM_GETMINMAXINFO: { MINMAXINFO *info=(MINMAXINFO*)lParam; info->ptMinTrackSize.x=800; info->ptMinTrackSize.y=600; return TRUE; }
      case WM_SIZE: LayoutMain(hDlg); return TRUE;
      case WM_COMMAND:
         switch (LOWORD(wParam)) {
            case IDB_ADD: AddBatchFiles(hDlg,&batchUI.base); return TRUE;
            case IDB_REMOVE: RemoveSelectedJobs(hDlg); return TRUE;
            case IDB_CLEAR: if (!batchUI.started) { batchUI.count=0; RefreshBatchList(hDlg); } return TRUE;
            case IDB_DETAILS: { int row=ListView_GetNextItem(GetDlgItem(hDlg,IDB_LIST),-1,LVNI_SELECTED); if (row>=0) Details_Show(hInstApp,hDlg,batchUI.jobs[row].parm.srcfn); return TRUE; }
            case IDB_START: StartQueue(hDlg); return TRUE;
            case IDB_CANCEL: { int row=ListView_GetNextItem(GetDlgItem(hDlg,IDB_LIST),-1,LVNI_SELECTED); if (row>=0) TaskBatch_RequestCancel(&batchUI.jobs[row]); return TRUE; }
            case IDB_CANCELALL: if (batchUI.started) TaskBatch_RequestCancelAll(&batchUI.task); return TRUE;
            case IDM_THEME_LIGHT: themeIndex=0; ApplyTheme(hDlg,TRUE); return TRUE;
            case IDM_THEME_DARK: themeIndex=1; ApplyTheme(hDlg,TRUE); return TRUE;
            case IDM_THEME_TEAL: themeIndex=2; ApplyTheme(hDlg,TRUE); return TRUE;
            case IDM_ABOUT: MessageBox(hDlg,"VDI-Tools\r\nVirtual disk copy and optimization tool.\r\nBased on CloneVDI by Don Milne.","About VDI-Tools",MB_OK|MB_ICONINFORMATION); return TRUE;
            case IDD_MODE_CLONE: parm.flags&=~PARM_FLAG_INPLACE; UpdateModeControls(hDlg); return TRUE;
            case IDD_MODE_INPLACE: parm.flags|=PARM_FLAG_INPLACE; UpdateModeControls(hDlg); return TRUE;
            case IDD_UUID_CHANGE: parm.flags&=~PARM_FLAG_KEEPUUID; CheckRadioButton(hDlg,IDD_UUID_CHANGE,IDD_UUID_KEEP,IDD_UUID_CHANGE); return TRUE;
            case IDD_UUID_KEEP: parm.flags|=PARM_FLAG_KEEPUUID; CheckRadioButton(hDlg,IDD_UUID_CHANGE,IDD_UUID_KEEP,IDD_UUID_KEEP); return TRUE;
            case IDD_INCREASE_SIZE: EnableWindow(GetDlgItem(hDlg,IDD_NEW_SIZE),IsDlgButtonChecked(hDlg,IDD_INCREASE_SIZE)); EnableWindow(GetDlgItem(hDlg,IDD_INCREASE_PARTSIZE),IsDlgButtonChecked(hDlg,IDD_INCREASE_SIZE) && !IsDlgButtonChecked(hDlg,IDD_MODE_INPLACE)); return TRUE;
            case IDD_LANGUAGE_COMBO: if (HIWORD(wParam)==CBN_SELCHANGE) { char v[16]; languagePreference=Localization_GetLanguageComboValue(GetDlgItem(hDlg,IDD_LANGUAGE_COMBO)); wsprintf(v,"%d",languagePreference); Profile_SetString(szLanguage,v); ApplyModernLocalization(hDlg); RefreshBatchList(hDlg); } return TRUE;
            case IDCANCEL: if (batchUI.started) { TaskBatch_RequestCancelAll(&batchUI.task); return TRUE; } DestroyWindow(hDlg); return TRUE;
         }
         break;
      case WM_NOTIFY: {
         NMHDR *pnm = (NMHDR*)lParam;
         if (pnm->idFrom == IDB_LIST) {
            if (pnm->code == NM_DBLCLK) {
               int row = ListView_GetNextItem(GetDlgItem(hDlg,IDB_LIST), -1, LVNI_SELECTED);
               if (row >= 0 && row < (int)batchUI.count) Details_Show(hInstApp, hDlg, batchUI.jobs[row].parm.srcfn);
               return TRUE;
            }
            if (pnm->code == NM_CUSTOMDRAW) {
               LPNMLVCUSTOMDRAW lpnmlv = (LPNMLVCUSTOMDRAW)lParam;
               switch (lpnmlv->nmcd.dwDrawStage) {
                  case CDDS_PREPAINT:
                     SetWindowLongPtr(hDlg, DWLP_MSGRESULT, CDRF_NOTIFYITEMDRAW);
                     return TRUE;
                  case CDDS_ITEMPREPAINT:
                     SetWindowLongPtr(hDlg, DWLP_MSGRESULT, CDRF_NOTIFYSUBITEMDRAW);
                     return TRUE;
                  case (CDDS_ITEMPREPAINT | CDDS_SUBITEM): {
                     if (lpnmlv->iSubItem == 5) {
                        int row = (int)lpnmlv->nmcd.dwItemSpec;
                        if (row >= 0 && row < (int)batchUI.count) {
                           DrawProgressColumn(lpnmlv->nmcd.hdc, lpnmlv->nmcd.hdr.hwndFrom, row, 5, &batchUI.jobs[row]);
                           SetWindowLongPtr(hDlg, DWLP_MSGRESULT, CDRF_SKIPDEFAULT);
                           return TRUE;
                        }
                     }
                     SetWindowLongPtr(hDlg, DWLP_MSGRESULT, CDRF_DODEFAULT);
                     return TRUE;
                  }
               }
            }
         }
         break;
      }
      case WM_BATCH_UPDATE: RefreshBatchList(hDlg); return TRUE;
      case WM_TIMER:
         RefreshBatchList(hDlg);
         if (batchUI.started && TaskBatch_IsComplete(&batchUI.task)) { KillTimer(hDlg,1); TaskBatch_Destroy(&batchUI.task); batchUI.started=FALSE; EnableWindow(GetDlgItem(hDlg,IDB_ADD),TRUE); EnableWindow(GetDlgItem(hDlg,IDB_REMOVE),TRUE); EnableWindow(GetDlgItem(hDlg,IDB_CLEAR),TRUE); EnableWindow(GetDlgItem(hDlg,IDB_START),TRUE); }
         return TRUE;
      case WM_CTLCOLORDLG: return (INT_PTR)themeBrush;
      case WM_CTLCOLORSTATIC: { HDC dc=(HDC)wParam; SetBkMode(dc,TRANSPARENT); SetTextColor(dc,themes[themeIndex].text); return (INT_PTR)themeBrush; }
      case WM_CLOSE: if (batchUI.started) { TaskBatch_RequestCancelAll(&batchUI.task); return TRUE; } DestroyWindow(hDlg); return TRUE;
      case WM_DESTROY:
         if (batchUI.started) { TaskBatch_RequestCancelAll(&batchUI.task); TaskBatch_Wait(&batchUI.task); TaskBatch_Destroy(&batchUI.task); }
         SaveMainWindowSize(hDlg);
         if (themeBrush) { DeleteObject(themeBrush); themeBrush=0; } if (themePanelBrush) { DeleteObject(themePanelBrush); themePanelBrush=0; }
         PostQuitMessage(0); return TRUE;
   }
   return FALSE;
}

int WINAPI
WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR szCmdLine, int nCmdShow)
{
   int  rslt,argc=Env_ProcessCmdLine(hInstance, szCmdLine);
   char languageValue[16];

   hInstApp = hInstance;
   { INITCOMMONCONTROLSEX controls; controls.dwSize=sizeof(controls); controls.dwICC=ICC_LISTVIEW_CLASSES|ICC_TAB_CLASSES|ICC_PROGRESS_CLASS; InitCommonControlsEx(&controls); }
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
         rslt = RunBatchCommandLine(&parm);
      }
   }

   Env_InitComAPI(FALSE);

   return rslt;
}

/*.......................................................................*/

/* end of clonevdi.c */
