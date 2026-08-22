#include "djwarning.h"
#include <stdarg.h>
#include <windows.h>
#include <commctrl.h>
#include "details.h"
#include "sectorviewer.h"
#include "vdir.h"
#include "vhdr.h"
#include "vmdkr.h"
#include "profile.h"
#include "localization.h"

#define IDD_TABS 700
#define IDD_TEXT 701
#define IDD_SECTORS 702

typedef struct {
   HVDDR disk;
   char filename[1024];
   BYTE mbr[512];
   BOOL chinese;
} DETAILS_CONTEXT;

static void ToWide(WCHAR *out, UINT count, const char *in)
{
   MultiByteToWideChar(CP_ACP,0,in,-1,out,count);
   out[count-1]=0;
}

static const char *DriveTypeName(int type)
{
   switch (type) {
      case VDD_TYPE_VDI: return "VDI";
      case VDD_TYPE_VHD: return "VHD";
      case VDD_TYPE_VMDK: return "VMDK";
      case VDD_TYPE_RAW: return "RAW";
      case VDD_TYPE_PART_RAW: return "Partition image";
      case VDD_TYPE_PARALLELS: return "Parallels HDD";
   }
   return "Unknown";
}

static unsigned long long Read64(const BYTE *p)
{
   unsigned long long lo=(unsigned long long)p[0] | ((unsigned long long)p[1]<<8) |
      ((unsigned long long)p[2]<<16) | ((unsigned long long)p[3]<<24);
   unsigned long long hi=(unsigned long long)p[4] | ((unsigned long long)p[5]<<8) |
      ((unsigned long long)p[6]<<16) | ((unsigned long long)p[7]<<24);
   return lo | (hi<<32);
}

static unsigned long Read32(const BYTE *p)
{
   return (unsigned long)p[0] | ((unsigned long)p[1]<<8) | ((unsigned long)p[2]<<16) | ((unsigned long)p[3]<<24);
}

static void SetOverview(HWND dlg, DETAILS_CONTEXT *c)
{
   char text[4096],size[128];
   HUGE bytes=0;
   LARGE_INTEGER physical;
   HANDLE file;
   c->disk->GetDriveSize(c->disk,&bytes);
   file=CreateFile(c->filename,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,0,NULL);
   if (file!=INVALID_HANDLE_VALUE && GetFileSizeEx(file,&physical)) {
      wsprintf(size,"%I64u bytes",physical.QuadPart);
   } else lstrcpy(size,"Unavailable");
   if (file!=INVALID_HANDLE_VALUE) CloseHandle(file);
   if (c->chinese) {
      WCHAR output[4096],fileW[1024],typeW[64],sizeW[128];
      ToWide(fileW,sizeof(fileW)/sizeof(fileW[0]),c->filename); ToWide(typeW,sizeof(typeW)/sizeof(typeW[0]),DriveTypeName(c->disk->GetDriveType(c->disk))); ToWide(sizeW,sizeof(sizeW)/sizeof(sizeW[0]),size);
      wsprintfW(output,L"\x6982\x89C8\r\n\r\n\x6587\x4EF6: %s\r\n\x683C\x5F0F: %s\r\n\x865A\x62DF\x5BB9\x91CF: %I64u bytes\r\n\x5B9E\x9645\x6587\x4EF6\x5927\x5C0F: %s\r\n\r\n\x6B64\x7A97\x53E3\x4EE5\x53EA\x8BFB\x65B9\x5F0F\x6253\x5F00\x78C1\x76D8\x3002\x53EF\x5728\x5176\x4ED6\x6807\x7B7E\x9875\x67E5\x770B\x5E03\x5C40\x548C\x5143\x6570\x636E\x3002",fileW,typeW,(unsigned long long)bytes,sizeW);
      SetDlgItemTextW(dlg,IDD_TEXT,output); return;
   }
   wsprintf(text,
      "OVERVIEW\r\n\r\n"
      "File: %s\r\n"
      "Format: %s\r\n"
      "Virtual capacity: %I64u bytes\r\n"
      "Physical file size: %s\r\n\r\n"
      "This window opens the disk read-only. Use the other tabs to inspect layout and metadata.",
      c->filename,DriveTypeName(c->disk->GetDriveType(c->disk)),(unsigned long long)bytes,size);
   SetDlgItemText(dlg,IDD_TEXT,text);
}

static void SetPartitions(HWND dlg, DETAILS_CONTEXT *c)
{
   char text[4096],line[512];
   UINT i;
   BOOL isGPT = FALSE;
   BYTE gptHdr[512];

   if (c->mbr[510]==0x55 && c->mbr[511]==0xAA && c->mbr[446+4]==0xEE) {
      if (c->disk->ReadSectors(c->disk,gptHdr,1,1)!=VDDR_RSLT_FAIL && memcmp(gptHdr,"EFI PART",8)==0) {
         isGPT = TRUE;
      }
   }

   if (c->chinese) {
      WCHAR output[4096],lineW[512];
      lstrcpyW(output,L"\x5206\x533A\x4FE1\x606F\r\n\r\n");
      if (c->mbr[510]!=0x55 || c->mbr[511]!=0xAA) {
         lstrcatW(output,L"\x672A\x627E\x5230\x6709\x6548\x7684 MBR \x6807\x8BB0\x3002");
      } else if (isGPT) {
         unsigned long long entriesLBA = Read64(gptHdr+72);
         unsigned long nEntries = Read32(gptHdr+80);
         unsigned long cbEntry  = Read32(gptHdr+84);
         lstrcatW(output,L"\x68C0\x6D4B\x5230 GPT \x5206\x533A\x8868 (GUID Partition Table)\x3002\r\n\r\n");
         lstrcatW(output,L"\x7F16\x53F7  \x540D\x79F0  \x8D77\x59CB LBA  \x7ED3\x675F LBA  \x6247\x533A\x6570\r\n");
         if (entriesLBA>0 && nEntries>0 && cbEntry>=128 && cbEntry<=512) {
            BYTE secBuffer[512];
            UINT perSec = 512 / cbEntry;
            UINT count = 0;
            if (nEntries > 128) nEntries = 128;
            for (i=0; i<nEntries; i++) {
               BYTE *pEntry;
               unsigned long long firstLBA, lastLBA, sectors;
               UINT k;
               BOOL nonZero = FALSE;
               if ((i % perSec) == 0) {
                  if (c->disk->ReadSectors(c->disk,secBuffer,(HUGE)(entriesLBA+(i/perSec)),1)==VDDR_RSLT_FAIL) break;
               }
               pEntry = secBuffer + (i % perSec) * cbEntry;
               for (k=0; k<16; k++) if (pEntry[k]) { nonZero = TRUE; break; }
               if (nonZero) {
                  WCHAR nameW[37];
                  firstLBA = Read64(pEntry+32);
                  lastLBA  = Read64(pEntry+40);
                  sectors  = (lastLBA >= firstLBA) ? (lastLBA - firstLBA + 1) : 0;
                  memcpy(nameW, pEntry+56, 72);
                  nameW[36] = 0;
                  if (!nameW[0]) lstrcpyW(nameW, L"-");
                  wsprintfW(lineW, L"%lu  %s  %I64u  %I64u  %I64u\r\n", count+1, nameW, firstLBA, lastLBA, sectors);
                  lstrcatW(output, lineW);
                  count++;
               }
            }
            if (count==0) lstrcatW(output, L"\x65E0\x6D3B\x52A8 GPT \x5206\x533A\x3002");
         }
      } else {
         lstrcatW(output,L"\x7F16\x53F7  \x5F15\x5BFC  \x7C7B\x578B  \x8D77\x59CB LBA  \x7ED3\x675F LBA  \x6247\x533A\x6570\r\n");
         for (i=0;i<4;i++) { BYTE *p=c->mbr+446+i*16; unsigned long start=Read32(p+8),count=Read32(p+12); if (!p[4] || !count) continue; wsprintfW(lineW,L"%lu  %s  0x%02X  %lu  %lu  %lu\r\n",i+1,p[0]==0x80?L"\x662F":L"\x5426",p[4],start,start+count-1,count); lstrcatW(output,lineW); }
      }
      SetDlgItemTextW(dlg,IDD_TEXT,output); return;
   }
   lstrcpy(text,"PARTITIONS\r\n\r\n");
   if (c->mbr[510]!=0x55 || c->mbr[511]!=0xAA) {
      lstrcat(text,"No valid MBR signature was found.");
   } else if (isGPT) {
      unsigned long long entriesLBA = Read64(gptHdr+72);
      unsigned long nEntries = Read32(gptHdr+80);
      unsigned long cbEntry  = Read32(gptHdr+84);
      lstrcat(text,"GPT Partition Table (GUID Partition Table) detected.\r\n\r\n");
      lstrcat(text,"#  Name  Start LBA  End LBA  Sectors\r\n");
      if (entriesLBA>0 && nEntries>0 && cbEntry>=128 && cbEntry<=512) {
         BYTE secBuffer[512];
         UINT perSec = 512 / cbEntry;
         UINT count = 0;
         if (nEntries > 128) nEntries = 128;
         for (i=0; i<nEntries; i++) {
            BYTE *pEntry;
            unsigned long long firstLBA, lastLBA, sectors;
            UINT k;
            BOOL nonZero = FALSE;
            if ((i % perSec) == 0) {
               if (c->disk->ReadSectors(c->disk,secBuffer,(HUGE)(entriesLBA+(i/perSec)),1)==VDDR_RSLT_FAIL) break;
            }
            pEntry = secBuffer + (i % perSec) * cbEntry;
            for (k=0; k<16; k++) if (pEntry[k]) { nonZero = TRUE; break; }
            if (nonZero) {
               char nameA[37];
               WCHAR nameW[37];
               firstLBA = Read64(pEntry+32);
               lastLBA  = Read64(pEntry+40);
               sectors  = (lastLBA >= firstLBA) ? (lastLBA - firstLBA + 1) : 0;
               memcpy(nameW, pEntry+56, 72);
               nameW[36] = 0;
               WideCharToMultiByte(CP_ACP, 0, nameW, -1, nameA, 37, NULL, NULL);
               if (!nameA[0]) lstrcpy(nameA, "-");
               wsprintf(line, "%lu  %s  %I64u  %I64u  %I64u\r\n", count+1, nameA, firstLBA, lastLBA, sectors);
               lstrcat(text, line);
               count++;
            }
         }
         if (count==0) lstrcat(text, "No active GPT partitions.");
      }
   } else {
      lstrcat(text,"#  Boot  Type  Start LBA  End LBA  Sectors\r\n");
      for (i=0;i<4;i++) {
         BYTE *p=c->mbr+446+i*16;
         unsigned long start=Read32(p+8),count=Read32(p+12);
         if (!p[4] || !count) continue;
         wsprintf(line,"%lu  %s  0x%02X  %lu  %lu  %lu\r\n",i+1,p[0]==0x80?"Yes":"No",p[4],start,start+count-1,count);
         lstrcat(text,line);
      }
      if (lstrlen(text)==26) lstrcat(text,"No active MBR partitions.");
   }
   SetDlgItemText(dlg,IDD_TEXT,text);
}

static void SetHeader(HWND dlg, DETAILS_CONTEXT *c)
{
   char text[4096];
   VDI_PREHEADER pre;
   VDI_HEADER vdi;
   VHD_FOOTER vhd;
   VHD_DYN_HEADER dyn;
   if (c->chinese) {
      WCHAR output[4096];
      if (c->disk->GetDriveType(c->disk)==VDD_TYPE_VDI && VDIR_GetHeader(c->disk,&pre,&vdi)) {
         wsprintfW(output,L"\x6807\x5934\x4FE1\x606F\r\n\r\n\x57FA\x672C\x4FE1\x606F\r\n  \x683C\x5F0F: VDI %lu.%02lu\r\n  \x7C7B\x578B: %lu\r\n  \x865A\x62DF\x5927\x5C0F: %I64u bytes\r\n\r\n\x5E03\x5C40\x4E0E\x5757\r\n  \x5757\x5927\x5C0F: %lu bytes\r\n  \x603B\x5757\x6570: %lu\r\n  \x5DF2\x5206\x914D\x5757\x6570: %lu",pre.u32Version>>16,pre.u32Version&0xffff,vdi.vdi_type,(unsigned long long)vdi.DiskSize,vdi.BlockSize,vdi.nBlocks,vdi.nBlocksAllocated);
      } else {
         WCHAR typeW[64];
         ToWide(typeW,sizeof(typeW)/sizeof(typeW[0]),DriveTypeName(c->disk->GetDriveType(c->disk)));
         wsprintfW(output,L"\x6807\x5934\x4FE1\x606F\r\n\r\n\x683C\x5F0F: %s\r\n\r\n\x6B64\x683C\x5F0F\x672A\x63D0\x4F9B\x7ED3\x6784\x5316\x6807\x5934\x4FE1\x606F\x3002",typeW);
      }
      SetDlgItemTextW(dlg,IDD_TEXT,output); return;
   }
   lstrcpy(text,"HEADER INFORMATION\r\n\r\n");
   if (c->disk->GetDriveType(c->disk)==VDD_TYPE_VDI && VDIR_GetHeader(c->disk,&pre,&vdi)) {
      wsprintf(text+lstrlen(text),"Basic\r\n  Format: VDI %lu.%02lu\r\n  Type: %lu\r\n  Virtual size: %I64u bytes\r\n\r\nLayout and blocks\r\n  Block size: %lu bytes\r\n  Total blocks: %lu\r\n  Allocated blocks: %lu\r\n\r\nUUID information is available from the legacy header dialog.",pre.u32Version>>16,pre.u32Version&0xffff,vdi.vdi_type,(unsigned long long)vdi.DiskSize,vdi.BlockSize,vdi.nBlocks,vdi.nBlocksAllocated);
   } else if (c->disk->GetDriveType(c->disk)==VDD_TYPE_VHD && VHDR_GetHeader(c->disk,&vhd,&dyn)) {
      wsprintf(text+lstrlen(text),"Basic\r\n  Format: VHD\r\n  Disk type: %lu\r\n  Virtual size: %I64u bytes\r\n\r\nLayout and blocks\r\n  Block size: %lu bytes\r\n  Total blocks: %lu\r\n",vhd.u32DiskType,(unsigned long long)vhd.u64CurrentSize,dyn.u32BlockSize,dyn.u32BlockCount);
   } else {
      wsprintf(text+lstrlen(text),"Format: %s\r\n\r\nThis format exposes no structured header through the current reader. The raw sector tab remains available.",DriveTypeName(c->disk->GetDriveType(c->disk)));
   }
   SetDlgItemText(dlg,IDD_TEXT,text);
}

static void UpdateTab(HWND dlg)
{
   DETAILS_CONTEXT *c=(DETAILS_CONTEXT*)GetWindowLong(dlg,DWL_USER);
   int tab=TabCtrl_GetCurSel(GetDlgItem(dlg,IDD_TABS));
   if (!c) return;
   if (tab==0) SetOverview(dlg,c);
   else if (tab==1) SetPartitions(dlg,c);
   else if (tab==2) SetHeader(dlg,c);
   else if (c->chinese) SetDlgItemTextW(dlg,IDD_TEXT,L"\x6247\x533A\x67E5\x770B\x5668\r\n\r\n\x70B9\x51FB\x201C\x6253\x5F00\x6247\x533A\x67E5\x770B\x5668\x201D\xFF0C\x53EF\x4EE5\x4EE5\x5341\x516D\x8FDB\x5236\x548C ASCII \x67E5\x770B\x6247\x533A\x3001\x8DF3\x8F6C\x3001\x5BFC\x51FA\x4EE5\x53CA\x68C0\x67E5 MBR\x3002\r\n\r\n\x8BE5\x67E5\x770B\x5668\x4EE5\x53EA\x8BFB\x65B9\x5F0F\x6253\x5F00\x6B64\x78C1\x76D8\x3002");
   else SetDlgItemText(dlg,IDD_TEXT,"SECTOR VIEWER\r\n\r\nUse 'Open sector viewer' to browse a sector in hexadecimal and ASCII, jump to a sector, export a range, or inspect the MBR.\r\n\r\nThe sector viewer opens this same disk read-only.");
}

static BOOL CALLBACK DetailsProc(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
   switch (msg) {
      case WM_INITDIALOG: {
         DETAILS_CONTEXT *c=(DETAILS_CONTEXT*)lParam;
         TCITEM item;
         const char *names[4]={"Overview","Partitions","Header","Sectors"};
         const WCHAR *chineseNames[4]={L"\x6982\x89C8",L"\x5206\x533A",L"\x6807\x5934",L"\x6247\x533A"};
         int i;
         SetWindowLong(dlg,DWL_USER,lParam);
         ZeroMemory(&item,sizeof(item)); item.mask=TCIF_TEXT;
         for (i=0;i<4;i++) { item.pszText=(LPSTR)names[i]; TabCtrl_InsertItem(GetDlgItem(dlg,IDD_TABS),i,&item); }
         if (c->chinese) { TCITEMW wide; SetWindowTextW(dlg,L"\x865A\x62DF\x78C1\x76D8\x8BE6\x60C5"); SetDlgItemTextW(dlg,IDD_SECTORS,L"\x6253\x5F00\x6247\x533A\x67E5\x770B\x5668"); SetDlgItemTextW(dlg,IDCANCEL,L"\x5173\x95ED"); ZeroMemory(&wide,sizeof(wide)); wide.mask=TCIF_TEXT; for(i=0;i<4;i++){ wide.pszText=(LPWSTR)chineseNames[i]; SendMessageW(GetDlgItem(dlg,IDD_TABS),TCM_SETITEMW,i,(LPARAM)&wide); } }
         UpdateTab(dlg); return TRUE;
      }
      case WM_NOTIFY:
         if (((NMHDR*)lParam)->idFrom==IDD_TABS && ((NMHDR*)lParam)->code==TCN_SELCHANGE) { UpdateTab(dlg); return TRUE; }
         break;
      case WM_COMMAND:
         if (LOWORD(wParam)==IDD_SECTORS) {
            DETAILS_CONTEXT *c=(DETAILS_CONTEXT*)GetWindowLong(dlg,DWL_USER);
            if (c) SectorViewer_Show(GetModuleHandle(NULL),dlg,c->disk);
            return TRUE;
         }
         if (LOWORD(wParam)==IDCANCEL || LOWORD(wParam)==IDOK) { EndDialog(dlg,0); return TRUE; }
         break;
   }
   return FALSE;
}

void Details_Show(HINSTANCE instance, HWND parent, CPFN filename)
{
   DETAILS_CONTEXT c;
   INT_PTR result;
   ZeroMemory(&c,sizeof(c));
   lstrcpyn(c.filename,filename,sizeof(c.filename));
   c.chinese=(Localization_ResolveLanguage(Profile_GetOptionDefault("Language",VDI_LANGUAGE_AUTO))==VDI_LANGUAGE_ZH_CN);
   VDDR_OpenMediaRegistry(c.filename);
   c.disk=VDDR_Open(c.filename,0);
   if (!c.disk) { MessageBox(parent,VDDR_GetErrorString(0xFFFFFFFF),"VDI-Tools",MB_ICONEXCLAMATION|MB_OK); return; }
   c.disk->ReadSectors(c.disk,c.mbr,0,1);
   result=DialogBoxParam(instance,"DLG_DETAILS",parent,DetailsProc,(LPARAM)&c);
   if (result==-1) {
      char message[256];
      wsprintf(message,"Could not open the details window (Windows error %lu).",GetLastError());
      MessageBox(parent,message,"VDI-Tools",MB_ICONEXCLAMATION|MB_OK);
   }
   c.disk->Close(c.disk);
}
