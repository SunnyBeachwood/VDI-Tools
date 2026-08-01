/* Best-effort detection of encrypted guest volumes. VeraCrypt intentionally has
 * no plaintext signature, so it is only reported as suspected unless its boot
 * loader name is visible. */
#include "djwarning.h"
#include <stdarg.h>
#include <windef.h>
#include <winbase.h>
#include "encryption.h"
#include "mem.h"
#include "djstring.h"
#include "env.h"
#include "ids.h"

#define MAX_CANDIDATES 64
#define SAMPLE_SECTORS 8
#define BOOT_SCAN_SECTORS 63

static PSTR pszENC_NOTSCANNED;
static PSTR pszENC_NONE;
static PSTR pszENC_INCOMPLETE;
static PSTR pszENC_PARTITION;
static PSTR pszENC_SUSPECTED;
static PSTR pszENC_UNKNOWN;

typedef struct {
   HUGE start;
   HUGE count;
   UINT index;
   BOOL luksType;
} VOLUME_CANDIDATE;

static UINT ReadLe32(const BYTE *p)
{
   return (UINT)p[0] | ((UINT)p[1]<<8) | ((UINT)p[2]<<16) | ((UINT)p[3]<<24);
}

static HUGE ReadLe64(const BYTE *p)
{
   HUGE value = ReadLe32(p+4);
   value <<= 32;
   value |= ReadLe32(p);
   return value;
}

static BOOL BytesEqual(const BYTE *a, const BYTE *b, UINT n)
{
   while (n--) if (*a++ != *b++) return FALSE;
   return TRUE;
}

static BOOL ContainsText(const BYTE *p, UINT cb, const char *text)
{
   UINT i,n=lstrlen(text);
   if (n>cb) return FALSE;
   for (i=0; i+n<=cb; i++) if (BytesEqual(p+i,(const BYTE*)text,n)) return TRUE;
   return FALSE;
}

static BOOL KnownPlainFileSystem(const BYTE *p, UINT cb)
{
   if (cb>=90) {
      if (BytesEqual(p+3,(const BYTE*)"NTFS    ",8)) return TRUE;
      if (BytesEqual(p+3,(const BYTE*)"EXFAT   ",8)) return TRUE;
      if (BytesEqual(p+54,(const BYTE*)"FAT",3)) return TRUE;
      if (BytesEqual(p+82,(const BYTE*)"FAT",3)) return TRUE;
   }
   if (cb>=1082 && p[1080]==0x53 && p[1081]==0xEF) return TRUE; /* ext2/3/4 */
   return FALSE;
}

static BOOL LooksRandom(const BYTE *p, UINT cb)
{
   UINT freq[256],i,unique=0,max=0,zeros=0;
   Mem_Zero(freq,sizeof(freq));
   for (i=0; i<cb; i++) freq[p[i]]++;
   for (i=0; i<256; i++) {
      if (freq[i]) unique++;
      if (freq[i]>max) max=freq[i];
   }
   zeros=freq[0];
   if (cb>=4096) return unique>=245 && max<=40 && zeros<=40;
   return unique>=205 && max<=10 && zeros<=10;
}

static void AddCandidate(VOLUME_CANDIDATE *v, UINT *count, HUGE start, HUGE sectors, UINT index, BOOL luksType)
{
   UINT i;
   if (!sectors || *count>=MAX_CANDIDATES) return;
   for (i=0;i<*count;i++) if (v[i].start==start) return;
   v[*count].start=start;
   v[*count].count=sectors;
   v[*count].index=index;
   v[*count].luksType=luksType;
   (*count)++;
}

static BOOL IsExtended(UINT type)
{
   return type==0x05 || type==0x0F || type==0x85;
}

static void ScanExtended(HVDDR disk, VOLUME_CANDIDATE *v, UINT *count, HUGE base, HUGE total, UINT *partIndex, BOOL *complete)
{
   BYTE sector[512];
   HUGE ebr=base;
   UINT guard;
   for (guard=0; guard<128; guard++) {
      BYTE *p;
      HUGE rel,size,next;
      if (disk->ReadSectors(disk,sector,ebr,1)==VDDR_RSLT_FAIL) { *complete=FALSE; return; }
      if (sector[510]!=0x55 || sector[511]!=0xAA) return;
      p=sector+446;
      rel=ReadLe32(p+8); size=ReadLe32(p+12);
      if (size) AddCandidate(v,count,ebr+rel,size,(*partIndex)++,FALSE);
      p+=16;
      next=ReadLe32(p+8);
      if (!next || !IsExtended(p[4])) return;
      ebr=base+next;
      if (ebr>=total) { *complete=FALSE; return; }
   }
   *complete=FALSE;
}

static BOOL IsLuksGuid(const BYTE *p)
{
   static const BYTE luksGuid[16]={0xCB,0x7C,0x7D,0xCA,0xED,0x63,0x53,0x4C,0x86,0x1C,0x17,0x42,0x53,0x60,0x59,0xCC};
   return BytesEqual(p,luksGuid,16);
}

static void ScanGPT(HVDDR disk, VOLUME_CANDIDATE *v, UINT *count, HUGE total, UINT *partIndex, BOOL *complete)
{
   BYTE hdr[512],entry[512];
   HUGE entriesLBA;
   UINT nEntries,entrySize,i,perSector;
   if (disk->ReadSectors(disk,hdr,1,1)==VDDR_RSLT_FAIL) { *complete=FALSE; return; }
   if (!BytesEqual(hdr,(const BYTE*)"EFI PART",8)) { *complete=FALSE; return; }
   entriesLBA=ReadLe64(hdr+72);
   nEntries=ReadLe32(hdr+80);
   entrySize=ReadLe32(hdr+84);
   if (entrySize<128 || entrySize>512 || (512%entrySize)!=0 || nEntries>4096) { *complete=FALSE; return; }
   perSector=512/entrySize;
   for (i=0;i<nEntries && *count<MAX_CANDIDATES;i++) {
      BYTE *p;
      HUGE first,last;
      if ((i%perSector)==0) {
         if (disk->ReadSectors(disk,entry,entriesLBA+(i/perSector),1)==VDDR_RSLT_FAIL) { *complete=FALSE; return; }
      }
      p=entry+(i%perSector)*entrySize;
      first=ReadLe64(p+32); last=ReadLe64(p+40);
      if (first && last>=first && last<total) AddCandidate(v,count,first,last-first+1,(*partIndex)++,IsLuksGuid(p));
   }
   if (i<nEntries) *complete=FALSE;
}

static void FindVolumes(HVDDR disk, VOLUME_CANDIDATE *v, UINT *count, HUGE total, BOOL *complete)
{
   BYTE mbr[512];
   UINT i,partIndex=1;
   BOOL protective=FALSE;
   if (disk->ReadSectors(disk,mbr,0,1)==VDDR_RSLT_FAIL) { *complete=FALSE; return; }
   if (mbr[510]==0x55 && mbr[511]==0xAA) {
      for (i=0;i<4;i++) {
         BYTE *p=mbr+446+i*16;
         UINT type=p[4];
         HUGE start=ReadLe32(p+8),sectors=ReadLe32(p+12);
         if (!sectors) continue;
         if (type==0xEE) protective=TRUE;
         else if (IsExtended(type)) ScanExtended(disk,v,count,start,total,&partIndex,complete);
         else AddCandidate(v,count,start,sectors,partIndex++,FALSE);
      }
      if (protective) { *count=0; partIndex=1; ScanGPT(disk,v,count,total,&partIndex,complete); }
   }
   if (!*count) AddCandidate(v,count,0,total,0,FALSE);
}

static void AddFinding(ENC_REPORT *r, UINT type, UINT confidence, const VOLUME_CANDIDATE *v)
{
   ENC_FINDING *f;
   UINT i;
   for (i=0;i<r->count;i++) if (r->finding[i].startLBA==v->start && r->finding[i].type==type) return;
   if (r->count>=ENC_MAX_FINDINGS) { r->complete=FALSE; return; }
   f=&r->finding[r->count++];
   f->type=type; f->confidence=confidence; f->startLBA=v->start; f->partition=v->index;
}

static BOOL ReadSample(HVDDR disk, BYTE *buf, HUGE lba, UINT sectors, ENC_REPORT *r)
{
   if (disk->ReadSectors(disk,buf,lba,sectors)==VDDR_RSLT_FAIL) { r->complete=FALSE; return FALSE; }
   return TRUE;
}

static void ScanVolume(HVDDR disk, const VOLUME_CANDIDATE *v, ENC_REPORT *r)
{
   BYTE sample[SAMPLE_SECTORS*512];
   BYTE bootSample[BOOT_SCAN_SECTORS*512];
   HUGE offsets[4];
   UINT i,n=0,random=0,available=0,bootSectors;
   if (!ReadSample(disk,sample,v->start,SAMPLE_SECTORS,r)) return;
   if (BytesEqual(sample+3,(const BYTE*)"-FVE-FS-",8)) { AddFinding(r,ENC_TYPE_BITLOCKER,ENC_CONFIDENCE_CONFIRMED,v); return; }
   if (sample[0]=='L' && sample[1]=='U' && sample[2]=='K' && sample[3]=='S' &&
       sample[4]==0xBA && sample[5]==0xBE) { AddFinding(r,ENC_TYPE_LUKS,ENC_CONFIDENCE_CONFIRMED,v); return; }
   bootSectors=(v->count<BOOT_SCAN_SECTORS) ? (UINT)v->count : BOOT_SCAN_SECTORS;
   if (bootSectors && ReadSample(disk,bootSample,v->start,bootSectors,r) &&
       (ContainsText(bootSample,bootSectors*512,"VeraCrypt") || ContainsText(bootSample,bootSectors*512,"TrueCrypt"))) {
      AddFinding(r,ENC_TYPE_VERACRYPT,ENC_CONFIDENCE_CONFIRMED,v); return;
   }
   if (v->luksType) AddFinding(r,ENC_TYPE_LUKS,ENC_CONFIDENCE_SUSPECTED,v);
   if (KnownPlainFileSystem(sample,sizeof(sample))) return;

   offsets[n++]=0;
   if (v->count>SAMPLE_SECTORS+2048) offsets[n++]=2048;
   if (v->count>SAMPLE_SECTORS*4) offsets[n++]=(v->count/2)&~((HUGE)SAMPLE_SECTORS-1);
   if (v->count>SAMPLE_SECTORS*2) offsets[n++]=v->count-SAMPLE_SECTORS;
   for (i=0;i<n;i++) {
      if (offsets[i]+SAMPLE_SECTORS>v->count) continue;
      if (ReadSample(disk,sample,v->start+offsets[i],SAMPLE_SECTORS,r)) {
         available++;
         if (LooksRandom(sample,sizeof(sample))) random++;
      }
   }
   if ((available>=3 && random>=3) || (available>0 && available<3 && random==available))
      AddFinding(r,ENC_TYPE_VERACRYPT|ENC_TYPE_UNKNOWN,ENC_CONFIDENCE_SUSPECTED,v);
}

PUBLIC BOOL Encryption_Scan(HVDDR disk, ENC_REPORT *report)
{
   VOLUME_CANDIDATE volume[MAX_CANDIDATES];
   HUGE size;
   UINT count=0,i;
   if (!report) return FALSE;
   Mem_Zero(report,sizeof(*report)); report->complete=TRUE;
   if (!disk || !disk->GetDriveSize(disk,&size)) { report->complete=FALSE; return FALSE; }
   size>>=9;
   FindVolumes(disk,volume,&count,size,&report->complete);
   for (i=0;i<count;i++) ScanVolume(disk,&volume[i],report);
   return report->complete;
}

PUBLIC BOOL Encryption_HasWarning(const ENC_REPORT *report)
{
   return report && (report->count || !report->complete);
}

PUBLIC void Encryption_FormatReport(const ENC_REPORT *report, PSTR text, UINT textSize)
{
   UINT i,used=0;
   if (!text || !textSize) return;
   text[0]=0;
   if (!report) { lstrcpyn(text,RSTR(ENC_NOTSCANNED),textSize); return; }
   if (!report->count) {
      lstrcpyn(text,report->complete ? RSTR(ENC_NONE) : RSTR(ENC_INCOMPLETE),textSize);
      return;
   }
   for (i=0;i<report->count;i++) {
      const ENC_FINDING *f=&report->finding[i];
      const char *name;
      char item[96];
      if (f->type & ENC_TYPE_BITLOCKER) name="BitLocker";
      else if (f->type & ENC_TYPE_LUKS) name="LUKS";
      else if (f->type & ENC_TYPE_VERACRYPT) name="VeraCrypt";
      else name=RSTR(ENC_UNKNOWN);
      wsprintf(item,RSTR(ENC_PARTITION),i ? ", " : "",name,(unsigned long)f->partition);
      if (f->confidence==ENC_CONFIDENCE_SUSPECTED) lstrcat(item,RSTR(ENC_SUSPECTED));
      if (used+lstrlen(item)+1>=textSize) break;
      lstrcat(text,item); used=lstrlen(text);
   }
   if (!report->complete && used+lstrlen(RSTR(ENC_INCOMPLETE))+3<textSize) { lstrcat(text,"; "); lstrcat(text,RSTR(ENC_INCOMPLETE)); }
}
