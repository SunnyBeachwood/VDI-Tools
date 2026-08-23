/* Journaled in-place compaction and virtual-capacity growth for dynamic VDI. */
#include "djwarning.h"
#include <stdarg.h>
#include <stddef.h>
#include <windef.h>
#include <winbase.h>
#include <wingdi.h>
#include <winuser.h>
#include "vdiinplace.h"
#include "vdir.h"
#include "vdistructs.h"
#include "djfile.h"
#include "mem.h"
#include "fsys.h"
#include "partinfo.h"
#include "progress.h"
#include "task.h"
#include "random.h"
#include "mediareg.h"
#include "env.h"
#include "filename.h"

#define JOURNAL_VERSION 1
#define MAX_MAPPED_PARTITIONS 32
#define JOURNAL_SUFFIX ".clonevdi-journal"

#define JS_PREPARED       1
#define JS_COMPACT_MOVE   2
#define JS_COMPACT_COMMIT 3
#define JS_SHIFT_MOVE     4
#define JS_FINAL_COMMIT   5
#define JS_PATCH_MBR      6
#define JS_DONE           7

typedef struct {
   BYTE signature[16];
   UINT version;
   UINT headerSize;
   UINT stage;
   UINT nextSid;
   UINT oldMapCount;
   UINT newMapCount;
   UINT oldAllocated;
   UINT newAllocated;
   UINT blockSize;
   UINT blockShift;
   UINT patchSize;
   UINT payloadCrc;
   UINT headerCrc;
   VDI_HEADER oldHeader;
   VDI_HEADER newHeader;
} VDIIP_JOURNAL;

typedef struct {
   VDIIP_JOURNAL j;
   UINT *oldMap;
   UINT *newMap;
   BYTE *patch;
   FILE journalFile;
   FNCHAR source[2048];
   FNCHAR journal[2048];
   BOOL noUI;
} VDIIP_CONTEXT;

static const BYTE JournalSignature[16]={'C','l','o','n','e','V','D','I','-','I','P','-','J','N','L',0};
static __declspec(thread) char LastErrorText[512];

static void SetError(PSTR text)
{
   lstrcpyn(LastErrorText,text,sizeof(LastErrorText));
}

PUBLIC PSTR VDIIP_GetErrorString(void)
{
   return LastErrorText;
}

static UINT Crc32Update(UINT crc, const BYTE *p, UINT cb)
{
   UINT i,j;
   crc=~crc;
   for (i=0;i<cb;i++) {
      crc^=p[i];
      for (j=0;j<8;j++) crc=(crc>>1)^(0xEDB88320U & (0-(crc&1)));
   }
   return ~crc;
}

static UINT HeaderCrc(const VDIIP_JOURNAL *j)
{
   VDIIP_JOURNAL copy=*j;
   copy.headerCrc=0;
   return Crc32Update(0,(const BYTE*)&copy,sizeof(copy));
}

static UINT PayloadCrc(const VDIIP_CONTEXT *c)
{
   UINT crc=0;
   crc=Crc32Update(crc,(const BYTE*)c->oldMap,c->j.oldMapCount*sizeof(UINT));
   crc=Crc32Update(crc,(const BYTE*)c->newMap,c->j.newMapCount*sizeof(UINT));
   if (c->patch && c->j.patchSize) crc=Crc32Update(crc,c->patch,c->j.patchSize);
   return crc;
}

static void JournalName(PFN out, CPFN source)
{
   lstrcpyn(out,source,2048-lstrlen(JOURNAL_SUFFIX));
   lstrcat(out,JOURNAL_SUFFIX);
}

PUBLIC BOOL VDIIP_HasJournal(CPFN sourceName)
{
   FNCHAR name[2048];
   JournalName(name,sourceName);
   return File_Exists(name);
}

static BOOL WriteJournalHeader(VDIIP_CONTEXT *c)
{
   if (!c->journalFile || c->journalFile==NULLFILE) c->journalFile=File_Open(c->journal);
   if (c->journalFile==NULLFILE) { SetError("Could not update the in-place recovery journal."); return FALSE; }
   c->j.headerCrc=HeaderCrc(&c->j);
   File_Seek(c->journalFile,0);
   if (File_WrBin(c->journalFile,&c->j,sizeof(c->j))!=sizeof(c->j) || !File_Flush(c->journalFile)) {
      SetError("Could not flush the in-place recovery journal."); return FALSE;
   }
   return TRUE;
}

static BOOL CreateJournal(VDIIP_CONTEXT *c)
{
   FILE f;
   c->j.payloadCrc=PayloadCrc(c);
   c->j.headerCrc=HeaderCrc(&c->j);
   f=File_Create(c->journal,DJFILE_FLAG_SEQUENTIAL);
   if (f==NULLFILE) { SetError("Could not create the in-place recovery journal beside the source VDI."); return FALSE; }
   if (File_WrBin(f,&c->j,sizeof(c->j))!=sizeof(c->j) ||
       File_WrBin(f,c->oldMap,c->j.oldMapCount*sizeof(UINT))!=c->j.oldMapCount*sizeof(UINT) ||
       File_WrBin(f,c->newMap,c->j.newMapCount*sizeof(UINT))!=c->j.newMapCount*sizeof(UINT) ||
       (c->j.patchSize && File_WrBin(f,c->patch,c->j.patchSize)!=c->j.patchSize) || !File_Flush(f)) {
      File_Close(f);
      File_Erase(c->journal); /* The source is still untouched, so a partial journal is not recoverable state. */
      SetError("Could not write the in-place recovery journal."); return FALSE;
   }
   File_Close(f);
   return TRUE;
}

static void FreeContext(VDIIP_CONTEXT *c)
{
   if (c->journalFile && c->journalFile!=NULLFILE) File_Close(c->journalFile);
   c->journalFile=NULLFILE;
   c->oldMap=Mem_Free(c->oldMap);
   c->newMap=Mem_Free(c->newMap);
   c->patch=Mem_Free(c->patch);
}

static BOOL LoadJournal(VDIIP_CONTEXT *c, CPFN source)
{
   FILE f;
   HUGE size,expected;
   Mem_Zero(c,sizeof(*c));
   lstrcpyn(c->source,source,2048);
   JournalName(c->journal,source);
   f=File_OpenRead(c->journal);
   if (f==NULLFILE) { SetError("Could not open the in-place recovery journal."); return FALSE; }
   if (File_RdBin(f,&c->j,sizeof(c->j))!=sizeof(c->j) ||
       Mem_Compare(c->j.signature,JournalSignature,sizeof(JournalSignature)) ||
       c->j.version!=JOURNAL_VERSION || c->j.headerSize!=sizeof(c->j) ||
       c->j.headerCrc!=HeaderCrc(&c->j) || !c->j.oldMapCount ||
       c->j.newMapCount<c->j.oldMapCount || c->j.blockSize<512 ||
       (c->j.blockSize&(c->j.blockSize-1)) || c->j.patchSize>c->j.blockSize) {
      File_Close(f); SetError("The in-place recovery journal is invalid or damaged."); return FALSE;
   }
   expected=sizeof(c->j)+(HUGE)(c->j.oldMapCount+c->j.newMapCount)*sizeof(UINT)+c->j.patchSize;
   File_Size(f,&size);
   if (size!=expected) { File_Close(f); SetError("The in-place recovery journal has an invalid size."); return FALSE; }
   c->oldMap=Mem_Alloc(0,c->j.oldMapCount*sizeof(UINT));
   c->newMap=Mem_Alloc(0,c->j.newMapCount*sizeof(UINT));
   if (c->j.patchSize) c->patch=Mem_Alloc(0,c->j.patchSize);
   if (!c->oldMap || !c->newMap || (c->j.patchSize && !c->patch)) {
      File_Close(f); FreeContext(c); SetError("Not enough memory to load the in-place recovery journal."); return FALSE;
   }
   if (File_RdBin(f,c->oldMap,c->j.oldMapCount*sizeof(UINT))!=c->j.oldMapCount*sizeof(UINT) ||
       File_RdBin(f,c->newMap,c->j.newMapCount*sizeof(UINT))!=c->j.newMapCount*sizeof(UINT) ||
       (c->j.patchSize && File_RdBin(f,c->patch,c->j.patchSize)!=c->j.patchSize)) {
      File_Close(f); FreeContext(c); SetError("Could not read the in-place recovery journal."); return FALSE;
   }
   File_Close(f);
   if (c->j.payloadCrc!=PayloadCrc(c)) { FreeContext(c); SetError("The in-place recovery journal checksum does not match."); return FALSE; }
   return TRUE;
}

static UINT PowerOfTwoExact(UINT x)
{
   UINT shift=0;
   if (!x || (x&(x-1))) return 0xFFFFFFFF;
   while (x>1) { x>>=1; shift++; }
   return shift;
}

static BOOL AllZero(const BYTE *p, UINT cb)
{
   while (cb--) if (*p++) return FALSE;
   return TRUE;
}

static BOOL IsNullUUID(const S_UUID *u)
{
   return !(u->au32[0]|u->au32[1]|u->au32[2]|u->au32[3]);
}

/* Keep child detection local: the legacy media registry is process-global and
 * cannot safely represent several batch jobs at once. */
static BOOL HasVDIChild(CPFN source, const S_UUID *parent)
{
   FNCHAR path[2048],pattern[2048],candidate[2048];
   WIN32_FIND_DATA fd;
   HANDLE find;
   Filename_SplitPath(source,path,NULL);
   if (!path[0]) lstrcpy(path,".");
   lstrcpy(pattern,path);
   if (pattern[lstrlen(pattern)-1]!='\\') lstrcat(pattern,"\\");
   lstrcat(pattern,"*.vdi");
   find=FindFirstFile(pattern,&fd);
   if (find==INVALID_HANDLE_VALUE) return FALSE;
   do {
      FILE f;
      VDI_PREHEADER ph;
      VDI_HEADER h;
      if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
      lstrcpy(candidate,path);
      if (candidate[lstrlen(candidate)-1]!='\\') lstrcat(candidate,"\\");
      lstrcat(candidate,fd.cFileName);
      if (Filename_Compare(candidate,source)==0) continue;
      f=File_OpenRead(candidate);
      if (f!=NULLFILE) {
         BOOL match=File_RdBin(f,&ph,sizeof(ph))==sizeof(ph) && ph.u32Signature==VDI_SIGNATURE &&
                    File_RdBin(f,&h,sizeof(h))==sizeof(h) && Mem_Compare(&h.uuidLinkage,parent,sizeof(S_UUID))==0;
         File_Close(f);
         if (match) { FindClose(find); return TRUE; }
      }
   } while (FindNextFile(find,&fd));
   FindClose(find);
   return FALSE;
}

static void NewUUID(S_UUID *u)
{
   UINT i;
   for (i=0;i<16;i++) u->au8[i]=(BYTE)Random_Integer(256);
   u->Gen.u8ClockSeqHiAndReserved=(BYTE)((u->Gen.u8ClockSeqHiAndReserved&0x3F)|0x80);
   u->Gen.u16TimeHiAndVersion=(WORD)((u->Gen.u16TimeHiAndVersion&0x0FFF)|0x4000);
}

static void CalcGeometry(VDIDISKGEOMETRY *g, UINT sectors)
{
   UINT h=16,c=sectors/(16*63);
   while (c>1024) { h+=h; c>>=1; }
   if (h>255) { h=255; c=sectors/(255*63); }
   g->cCylinders=c; g->cHeads=h; g->cSectorsPerTrack=63; g->cBytesPerSector=512;
}

static UINT ReadLe32(const BYTE *p)
{
   return (UINT)p[0]|((UINT)p[1]<<8)|((UINT)p[2]<<16)|((UINT)p[3]<<24);
}

static HUGE ReadLe64(const BYTE *p)
{
   return (HUGE)ReadLe32(p) | ((HUGE)ReadLe32(p+4)<<32);
}

static void LBA2CHS(BYTE *cyl, BYTE *head, BYTE *sect, UINT lba, UINT heads)
{
   UINT c,h,s,t;
   if (lba>=(1023*254*63)) { c=1023; h=254; s=63; }
   else { c=lba/(heads*63); t=lba-c*heads*63; h=t/63; s=t-h*63+1; }
   *cyl=(BYTE)(c&0xFF); *head=(BYTE)h; *sect=(BYTE)(((c>>2)&0xC0)+s);
}

static BOOL PatchMBR(BYTE *block, UINT blockSize, UINT newSectors, UINT heads)
{
   UINT i;
   if (blockSize<512 || block[510]!=0x55 || block[511]!=0xAA) return FALSE;
   for (i=0;i<4;i++) {
      BYTE *p=block+446+i*16;
      UINT start=ReadLe32(p+8),count=ReadLe32(p+12);
      if (!start || !count) continue;
      LBA2CHS(p+3,p+1,p+2,start,heads);
      LBA2CHS(p+7,p+5,p+6,start+count-1,heads);
   }
   {
      BYTE *p=block+446;
      UINT start=ReadLe32(p+8),off=start<<9;
      if (start<2048 && off+512<=blockSize) {
         BYTE *boot=block+off;
         if ((Mem_Compare(boot+3,"NTFS    ",8)==0) ||
             (Mem_Compare(boot+54,"FAT",3)==0) || (Mem_Compare(boot+82,"FAT",3)==0)) {
            boot[26]=(BYTE)heads; boot[27]=(BYTE)(heads>>8);
            if (off+7*512<=blockSize && Mem_Compare(boot+82,"FAT32",5)==0) {
               boot[6*512+26]=(BYTE)heads; boot[6*512+27]=(BYTE)(heads>>8);
            }
         }
      }
   }
   return TRUE;
}

static UINT MapPartitions(HVDDR disk, HFSYS fs[MAX_MAPPED_PARTITIONS])
{
   BYTE mbr[512];
   UINT i,n=0;
   BOOL isGPT=FALSE;
   for (i=0;i<MAX_MAPPED_PARTITIONS;i++) fs[i]=NULL;
   if (disk->ReadSectors(disk,mbr,0,1)==VDDR_RSLT_FAIL || mbr[510]!=0x55 || mbr[511]!=0xAA) return 0;
   
   for (i=0;i<4;i++) {
      if (mbr[446+i*16+4] == 0xEE) {
         BYTE gptHdr[512];
         if (disk->ReadSectors(disk,gptHdr,1,1)!=VDDR_RSLT_FAIL && Mem_Compare(gptHdr,"EFI PART",8)==0) {
            isGPT = TRUE;
            break;
         }
      }
   }

   if (isGPT) {
      BYTE gptHdr[512];
      if (disk->ReadSectors(disk,gptHdr,1,1)!=VDDR_RSLT_FAIL && Mem_Compare(gptHdr,"EFI PART",8)==0) {
         HUGE entriesLBA = ReadLe64(gptHdr+72);
         UINT nEntries   = ReadLe32(gptHdr+80);
         UINT cbEntry    = ReadLe32(gptHdr+84);
         if (entriesLBA>0 && nEntries>0 && cbEntry>=128 && cbEntry<=512 && (512%cbEntry)==0) {
            BYTE secBuffer[512];
            UINT perSec = 512 / cbEntry;
            if (nEntries > 128) nEntries = 128;
            for (i=0; i<nEntries && n<(MAX_MAPPED_PARTITIONS-1); i++) {
               BYTE *pEntry;
               HUGE firstLBA, lastLBA;
               if ((i % perSec) == 0) {
                  if (disk->ReadSectors(disk,secBuffer,entriesLBA+(i/perSec),1)==VDDR_RSLT_FAIL) break;
               }
               pEntry = secBuffer + (i % perSec) * cbEntry;
               if (!IsNullUUID((const S_UUID*)pEntry)) {
                  firstLBA = ReadLe64(pEntry+32);
                  lastLBA  = ReadLe64(pEntry+40);
                  if (firstLBA>0 && lastLBA>=firstLBA) {
                     HUGE cLBA = lastLBA - firstLBA + 1;
                     HFSYS h = FSys_OpenVolume(0,disk,firstLBA,cLBA,512);
                     if (h) fs[n++] = h;
                  }
               }
            }
         }
      }
   } else {
      for (i=0;i<4 && n<MAX_MAPPED_PARTITIONS;i++) {
         BYTE *p=mbr+446+i*16;
         HUGE start=ReadLe32(p+8),count=ReadLe32(p+12);
         HFSYS h=FSys_OpenVolume(p[4],disk,start,count,512);
         if (h) fs[n++]=h;
      }
   }

   if (n<MAX_MAPPED_PARTITIONS) {
      HUGE sectors;
      HFSYS h;
      disk->GetDriveSize(disk,&sectors); sectors>>=9;
      h=FSys_OpenVolume(0xFFFFFFFF,disk,0,sectors,512);
      if (h) fs[n++]=h;
   }
   return n;
}

static BOOL PageUnused(HFSYS fs[MAX_MAPPED_PARTITIONS], UINT n, UINT page, UINT spbShift)
{
   UINT i;
   for (i=0;i<n;i++) {
      UINT state=fs[i]->IsBlockUsed(fs[i],page,spbShift);
      if (state!=FSYS_BLOCK_OUTSIDE) return state==FSYS_BLOCK_UNUSED;
   }
   return FALSE;
}

static UINT FirstPartitionLBA(HVDDR disk)
{
   BYTE mbr[512]; UINT i,best=63;
   if (disk->ReadSectors(disk,mbr,0,1)==VDDR_RSLT_FAIL || mbr[510]!=0x55 || mbr[511]!=0xAA) return best;
   best=0xFFFFFFFF;
   for (i=0;i<4;i++) {
      UINT start=ReadLe32(mbr+446+i*16+8);
      if (start && start<best) best=start;
   }
   return best==0xFFFFFFFF ? 63 : best;
}

static BOOL ReadRawMap(CPFN fn, const VDI_HEADER *h, UINT *map)
{
   FILE f=File_OpenRead(fn);
   BOOL ok=FALSE;
   if (f!=NULLFILE) {
      File_Seek(f,h->offset_Blocks);
      ok=File_RdBin(f,map,h->nBlocks*sizeof(UINT))==h->nBlocks*sizeof(UINT);
      File_Close(f);
   }
   return ok;
}

static BOOL CheckOperationSpace(const VDIIP_CONTEXT *c)
{
   FNCHAR path[2048];
   HUGE freeBytes,journalBytes,growthBytes,required;
   UINT len;

   Filename_SplitPath(c->source,path,NULL);
   if (!path[0]) lstrcpy(path,".");
   if (path[0]=='\\' && path[1]=='\\') {
      len=Filename_Length(path);
      if (len && path[len-1]!='\\') { path[len]='\\'; path[len+1]=0; }
   }
   if (!Env_GetDiskFreeSpace(path,&freeBytes)) {
      SetError("Could not determine the free space available beside the source VDI.");
      return FALSE;
   }
   journalBytes=sizeof(c->j)+(HUGE)(c->j.oldMapCount+c->j.newMapCount)*sizeof(UINT)+c->j.patchSize;
   growthBytes=0;
   if (c->j.newHeader.offset_Image>c->j.oldHeader.offset_Image)
      growthBytes=c->j.newHeader.offset_Image-c->j.oldHeader.offset_Image;
   required=journalBytes+growthBytes+65536; /* Leave room for filesystem allocation overhead. */
   if (freeBytes<required) {
      SetError("There is not enough free space for the recovery journal and in-place VDI metadata relocation.");
      return FALSE;
   }
   return TRUE;
}

static BOOL Analyze(VDIIP_CONTEXT *c, s_CLONEPARMS *parm)
{
   HVDDR disk;
   VDI_PREHEADER ph;
   VDI_HEADER h;
   UINT *logicalBySid=NULL,*state=NULL;
   BYTE *buffer=NULL;
   HFSYS fs[MAX_MAPPED_PARTITIONS];
   UINT nfs=0,i,sid,newSid=0,targetBlocks,spbShift,bootLBA;
   BOOL compact=(parm->flags&PARM_FLAG_COMPACT)!=0;
   HUGE oldSectors;

   Mem_Zero(c,sizeof(*c)); lstrcpyn(c->source,parm->srcfn,2048); JournalName(c->journal,c->source);
   Task_MediaRegistryEnter();
   VDDR_OpenMediaRegistry(c->source);
   disk=VDDR_Open(c->source,0);
   Task_MediaRegistryLeave();
   if (!disk) { SetError(VDDR_GetErrorString(0xFFFFFFFF)); return FALSE; }
   if (disk->GetDriveType(disk)!=VDD_TYPE_VDI || !VDIR_GetHeader(disk,&ph,&h)) {
      disk->Close(disk); SetError("In-place mode supports VDI files only."); return FALSE;
   }
   spbShift=PowerOfTwoExact(h.BlockSize>>9);
   if (ph.u32Version!=VDI_VERSION_1_1 || h.vdi_type!=VDI_TYPE_DYNAMIC || h.cbBlockExtra!=0 ||
       spbShift==0xFFFFFFFF || h.BlockSize<512 || !IsNullUUID(&h.uuidLinkage) || !IsNullUUID(&h.uuidParentModify)) {
      disk->Close(disk); SetError("In-place mode requires a standalone VDI 1.1 dynamic image with no extra per-block data."); return FALSE;
   }
   if (HasVDIChild(c->source,&h.uuidCreate)) {
      disk->Close(disk); SetError("A dependent child VDI was found. In-place modification of a snapshot base is not allowed."); return FALSE;
   }
   targetBlocks=h.nBlocks;
   if (parm->flags&PARM_FLAG_ENLARGE) {
      HUGE targetSectors=parm->DestSectors;
      targetBlocks=(UINT)(targetSectors>>(spbShift));
      if (targetSectors&(((HUGE)1<<spbShift)-1)) targetBlocks++;
      if (targetBlocks<h.nBlocks) { disk->Close(disk); SetError("The new virtual capacity cannot be smaller than the existing capacity."); return FALSE; }
   }
   if (!compact && targetBlocks==h.nBlocks) { disk->Close(disk); SetError("No in-place operation was selected."); return FALSE; }

   c->oldMap=Mem_Alloc(0,h.nBlocks*sizeof(UINT));
   c->newMap=Mem_Alloc(0,targetBlocks*sizeof(UINT));
   logicalBySid=Mem_Alloc(0,h.nBlocksAllocated*sizeof(UINT));
   state=Mem_Alloc(MEMF_ZEROINIT,h.nBlocks*sizeof(UINT));
   if (!c->oldMap || !c->newMap || (h.nBlocksAllocated && !logicalBySid) || !state || !ReadRawMap(c->source,&h,c->oldMap)) {
      disk->Close(disk); logicalBySid=Mem_Free(logicalBySid); state=Mem_Free(state); FreeContext(c);
      SetError("Could not allocate or read the VDI block maps."); return FALSE;
   }
   for (i=0;i<targetBlocks;i++) c->newMap[i]=VDI_PAGE_FREE;
   for (i=0;i<h.nBlocksAllocated;i++) logicalBySid[i]=0xFFFFFFFF;
   for (i=0;i<h.nBlocks;i++) {
      sid=c->oldMap[i];
      if (VDI_BLOCK_ALLOCATED(sid)) {
         if (sid>=h.nBlocksAllocated || logicalBySid[sid]!=0xFFFFFFFF) {
            disk->Close(disk); logicalBySid=Mem_Free(logicalBySid); state=Mem_Free(state); FreeContext(c);
            SetError("The VDI block map is invalid or contains duplicate physical blocks."); return FALSE;
         }
         logicalBySid[sid]=i; state[i]=1;
      } else c->newMap[i]=sid;
   }
   for (i=0;i<h.nBlocksAllocated;i++) if (logicalBySid[i]==0xFFFFFFFF) {
      disk->Close(disk); logicalBySid=Mem_Free(logicalBySid); state=Mem_Free(state); FreeContext(c);
      SetError("The VDI block map has missing physical block identifiers."); return FALSE;
   }

   if (compact) {
      nfs=MapPartitions(disk,fs);
      buffer=Mem_Alloc(0,h.BlockSize);
      if (!buffer) { disk->Close(disk); logicalBySid=Mem_Free(logicalBySid); state=Mem_Free(state); FreeContext(c); SetError("Not enough memory for the VDI block buffer."); return FALSE; }
      for (i=0;i<h.nBlocks;i++) if (state[i]) {
         if (i!=0 && PageUnused(fs,nfs,i,spbShift)) { state[i]=0; c->newMap[i]=VDI_PAGE_FREE; }
         else {
            int rr=disk->ReadPage(disk,buffer,i,spbShift);
            if (rr==VDDR_RSLT_FAIL) { SetError(VDDR_GetErrorString(0xFFFFFFFF)); goto analyze_fail; }
            if (rr==VDDR_RSLT_BLANKPAGE || AllZero(buffer,h.BlockSize)) { state[i]=0; c->newMap[i]=VDI_PAGE_ZERO; }
         }
      }
   }
   for (sid=0;sid<h.nBlocksAllocated;sid++) {
      i=logicalBySid[sid];
      if (state[i]) c->newMap[i]=newSid++;
   }

   Mem_Copy(c->j.signature,JournalSignature,sizeof(JournalSignature));
   c->j.version=JOURNAL_VERSION; c->j.headerSize=sizeof(c->j); c->j.stage=JS_PREPARED;
   c->j.oldMapCount=h.nBlocks; c->j.newMapCount=targetBlocks;
   c->j.oldAllocated=h.nBlocksAllocated; c->j.newAllocated=newSid;
   c->j.blockSize=h.BlockSize; c->j.blockShift=PowerOfTwoExact(h.BlockSize);
   c->j.oldHeader=h; c->j.newHeader=h;
   c->j.newHeader.nBlocks=targetBlocks; c->j.newHeader.nBlocksAllocated=newSid;
   if (targetBlocks!=h.nBlocks) {
      UINT required=(h.offset_Blocks+targetBlocks*sizeof(UINT)+511)&~511U;
      UINT targetSectors=targetBlocks<<(c->j.blockShift-9);
      c->j.newHeader.DiskSize=(HUGE)targetBlocks<<c->j.blockShift;
      c->j.newHeader.offset_Image=h.offset_Image;
      if (required>c->j.newHeader.offset_Image) c->j.newHeader.offset_Image=required;
      bootLBA=FirstPartitionLBA(disk);
      while ((((c->j.newHeader.offset_Image>>9)+bootLBA)&7)!=0) c->j.newHeader.offset_Image+=512;
      CalcGeometry(&c->j.newHeader.LCHSGeometry,targetSectors);
      oldSectors=h.DiskSize>>9;
      if (oldSectors<(1023*255*63)) {
         c->patch=Mem_Alloc(0,h.BlockSize);
         if (!c->patch) { SetError("Not enough memory for MBR geometry correction."); goto analyze_fail; }
         if (disk->ReadPage(disk,c->patch,0,spbShift)==VDDR_RSLT_NORMAL &&
             PatchMBR(c->patch,h.BlockSize,targetSectors,c->j.newHeader.LCHSGeometry.cHeads)) c->j.patchSize=h.BlockSize;
         else c->patch=Mem_Free(c->patch);
      }
   }
   NewUUID(&c->j.newHeader.uuidModify);
   if (!CheckOperationSpace(c)) goto analyze_fail;
   for (i=0;i<nfs;i++) fs[i]->CloseVolume(fs[i]);
   disk->Close(disk); buffer=Mem_Free(buffer); logicalBySid=Mem_Free(logicalBySid); state=Mem_Free(state);
   if (!CreateJournal(c)) { FreeContext(c); return FALSE; }
   return TRUE;

analyze_fail:
   for (i=0;i<nfs;i++) if (fs[i]) fs[i]->CloseVolume(fs[i]);
   disk->Close(disk); buffer=Mem_Free(buffer); logicalBySid=Mem_Free(logicalBySid); state=Mem_Free(state); FreeContext(c);
   return FALSE;
}

static BOOL RawCopyBlock(FILE f, BYTE *buffer, HUGE from, HUGE to, UINT blockSize)
{
   File_Seek(f,from);
   if (File_RdBin(f,buffer,blockSize)!=blockSize) return FALSE;
   File_Seek(f,to);
   if (File_WrBin(f,buffer,blockSize)!=blockSize || !File_Flush(f)) return FALSE;
   return TRUE;
}

static BOOL WriteHeaderAndMap(FILE f, const VDI_HEADER *h, const UINT *map, UINT count)
{
   File_Seek(f,h->offset_Blocks);
   if (File_WrBin(f,(PVOID)map,count*sizeof(UINT))!=count*sizeof(UINT)) return FALSE;
   if (!File_Flush(f)) return FALSE;
   File_Seek(f,sizeof(VDI_PREHEADER));
   if (File_WrBin(f,(PVOID)h,sizeof(*h))!=sizeof(*h) || !File_Flush(f)) return FALSE;
   return TRUE;
}

static BOOL ValidateSourceForStage(FILE f, const VDIIP_CONTEXT *c)
{
   VDI_PREHEADER ph;
   VDI_HEADER h;
   UINT *map=NULL;
   BOOL ok=FALSE;
   File_Seek(f,0);
   if (File_RdBin(f,&ph,sizeof(ph))!=sizeof(ph) || ph.u32Signature!=VDI_SIGNATURE ||
       File_RdBin(f,&h,sizeof(h))!=sizeof(h) || Mem_Compare(&h.uuidCreate,&c->j.oldHeader.uuidCreate,sizeof(S_UUID))) {
      SetError("The source VDI no longer matches its recovery journal."); return FALSE;
   }
   if (c->j.stage!=JS_PREPARED) return TRUE;
   if (h.nBlocks!=c->j.oldHeader.nBlocks || h.nBlocksAllocated!=c->j.oldHeader.nBlocksAllocated ||
       h.offset_Blocks!=c->j.oldHeader.offset_Blocks || h.offset_Image!=c->j.oldHeader.offset_Image ||
       h.BlockSize!=c->j.oldHeader.BlockSize || h.DiskSize!=c->j.oldHeader.DiskSize ||
       Mem_Compare(&h.uuidModify,&c->j.oldHeader.uuidModify,sizeof(S_UUID))) {
      SetError("The source VDI changed after the in-place operation was prepared."); return FALSE;
   }
   map=Mem_Alloc(0,c->j.oldMapCount*sizeof(UINT));
   if (map) {
      File_Seek(f,h.offset_Blocks);
      if (File_RdBin(f,map,c->j.oldMapCount*sizeof(UINT))==c->j.oldMapCount*sizeof(UINT) &&
          !Mem_Compare(map,c->oldMap,c->j.oldMapCount*sizeof(UINT))) ok=TRUE;
   }
   Mem_Free(map);
   if (!ok) SetError("The source VDI block map changed after the operation was prepared.");
   return ok;
}

static BOOL Execute(VDIIP_CONTEXT *c, HINSTANCE hInstRes, HWND parent)
{
   FILE f;
   BYTE *buffer;
   UINT *logicalBySid;
   ProgInfo prog;
   UINT sid,logical,dest;
   HUGE from,to,finalSize;
   VDI_HEADER compactHeader;
   BOOL ok=FALSE;

   f=File_Open(c->source);
   if (f==NULLFILE) { SetError("The source VDI is read-only, in use, or cannot be opened exclusively."); return FALSE; }
   if (!ValidateSourceForStage(f,c)) { File_Close(f); return FALSE; }
   buffer=Mem_Alloc(0,c->j.blockSize);
   logicalBySid=Mem_Alloc(0,c->j.oldAllocated*sizeof(UINT));
   if (!buffer || (c->j.oldAllocated && !logicalBySid)) {
      Mem_Free(buffer); Mem_Free(logicalBySid); File_Close(f); SetError("Not enough memory for in-place block movement."); return FALSE;
   }
   for (sid=0;sid<c->j.oldAllocated;sid++) logicalBySid[sid]=0xFFFFFFFF;
   for (logical=0;logical<c->j.oldMapCount;logical++) {
      sid=c->oldMap[logical];
      if (VDI_BLOCK_ALLOCATED(sid) && sid<c->j.oldAllocated) logicalBySid[sid]=logical;
   }
   FillMemory(&prog,sizeof(prog),0);
   prog.bNoUI=c->noUI;
   prog.pszFn=c->source; prog.pszMsg="Modifying the source VDI in place - do not interrupt...";
   prog.pszCaption="In-place VDI modification";
   prog.BytesTotal=(c->j.oldAllocated+c->j.newAllocated)*(1.0*c->j.blockSize);
   Progress.Begin(hInstRes,parent,&prog); Progress.UpdateStats(&prog);

   while (c->j.stage!=JS_DONE) {
      if (c->j.stage==JS_PREPARED) {
         c->j.stage=JS_COMPACT_MOVE; c->j.nextSid=0;
         if (!WriteJournalHeader(c)) goto done;
      } else if (c->j.stage==JS_COMPACT_MOVE) {
         for (sid=c->j.nextSid;sid<c->j.oldAllocated;sid++) {
            logical=logicalBySid[sid];
            if (logical==0xFFFFFFFF) { SetError("Recovery journal contains an invalid old block map."); goto done; }
            if (VDI_BLOCK_ALLOCATED(c->newMap[logical])) {
               dest=c->newMap[logical];
               if (dest!=sid) {
                  from=c->j.oldHeader.offset_Image+(HUGE)sid*c->j.blockSize;
                  to=c->j.oldHeader.offset_Image+(HUGE)dest*c->j.blockSize;
                  if (!RawCopyBlock(f,buffer,from,to,c->j.blockSize)) { SetError("I/O error while compacting VDI blocks in place."); goto done; }
               }
               prog.BytesDone+=(1.0*c->j.blockSize); Progress.UpdateStats(&prog); prog.bUserCancel=FALSE;
            }
            c->j.nextSid=sid+1;
            if (!WriteJournalHeader(c)) goto done;
            if (Task_CurrentCancelRequested()) { SetError("In-place task paused at a recovery checkpoint."); goto done; }
         }
         c->j.stage=JS_COMPACT_COMMIT; c->j.nextSid=0;
         if (!WriteJournalHeader(c)) goto done;
      } else if (c->j.stage==JS_COMPACT_COMMIT) {
         compactHeader=c->j.oldHeader; compactHeader.nBlocksAllocated=c->j.newAllocated;
         if (!WriteHeaderAndMap(f,&compactHeader,c->newMap,c->j.oldMapCount)) { SetError("Could not commit the compacted VDI block map."); goto done; }
         finalSize=c->j.oldHeader.offset_Image+(HUGE)c->j.newAllocated*c->j.blockSize;
         if (!File_Truncate(f,finalSize) || !File_Flush(f)) { SetError("Could not truncate the compacted VDI file."); goto done; }
         c->j.stage=JS_SHIFT_MOVE; c->j.nextSid=c->j.newAllocated;
         if (!WriteJournalHeader(c)) goto done;
      } else if (c->j.stage==JS_SHIFT_MOVE) {
         while (c->j.nextSid) {
            sid=c->j.nextSid-1;
            if (c->j.newHeader.offset_Image!=c->j.oldHeader.offset_Image) {
               from=c->j.oldHeader.offset_Image+(HUGE)sid*c->j.blockSize;
               to=c->j.newHeader.offset_Image+(HUGE)sid*c->j.blockSize;
               if (!RawCopyBlock(f,buffer,from,to,c->j.blockSize)) { SetError("I/O error while moving VDI data for the enlarged block map."); goto done; }
            }
            prog.BytesDone+=(1.0*c->j.blockSize); Progress.UpdateStats(&prog); prog.bUserCancel=FALSE;
            c->j.nextSid=sid;
            if (!WriteJournalHeader(c)) goto done;
            if (Task_CurrentCancelRequested()) { SetError("In-place task paused at a recovery checkpoint."); goto done; }
         }
         c->j.stage=JS_FINAL_COMMIT;
         if (!WriteJournalHeader(c)) goto done;
      } else if (c->j.stage==JS_FINAL_COMMIT) {
         if (!WriteHeaderAndMap(f,&c->j.newHeader,c->newMap,c->j.newMapCount)) { SetError("Could not commit the final VDI header and block map."); goto done; }
         finalSize=c->j.newHeader.offset_Image+(HUGE)c->j.newAllocated*c->j.blockSize;
         if (!File_Truncate(f,finalSize) || !File_Flush(f)) { SetError("Could not set the final VDI file size."); goto done; }
         c->j.stage=JS_PATCH_MBR;
         if (!WriteJournalHeader(c)) goto done;
      } else if (c->j.stage==JS_PATCH_MBR) {
         if (c->j.patchSize && VDI_BLOCK_ALLOCATED(c->newMap[0])) {
            to=c->j.newHeader.offset_Image+(HUGE)c->newMap[0]*c->j.blockSize;
            File_Seek(f,to);
            if (File_WrBin(f,c->patch,c->j.patchSize)!=c->j.patchSize || !File_Flush(f)) { SetError("Could not write the corrected MBR block."); goto done; }
         }
         c->j.stage=JS_DONE;
         if (!WriteJournalHeader(c)) goto done;
      } else { SetError("The recovery journal contains an unknown operation stage."); goto done; }
   }
   ok=TRUE;
done:
   Progress.End(&prog); Mem_Free(buffer); Mem_Free(logicalBySid); File_Close(f);
   if (c->journalFile && c->journalFile!=NULLFILE) { File_Close(c->journalFile); c->journalFile=NULLFILE; }
   if (ok) File_Erase(c->journal);
   return ok;
}

static BOOL ParseRequestedSize(s_CLONEPARMS *parm)
{
   const char *p=parm->szDestSize;
   double value=0.0,fraction=0.1;
   BOOL digits=FALSE;
   while (*p==' ' || *p=='\t') p++;
   while (*p>='0' && *p<='9') { digits=TRUE; value=value*10+(*p-'0'); p++; }
   if (*p=='.') {
      p++;
      while (*p>='0' && *p<='9') { digits=TRUE; value+=(*p-'0')*fraction; fraction*=0.1; p++; }
   }
   while (*p==' ' || *p=='\t') p++;
   if (!digits || value<=0) { SetError("The new drive size is invalid."); return FALSE; }
   if (*p=='G' || *p=='g') {
      if (value>2047.0) { SetError("This utility cannot enlarge a virtual drive beyond 2047 GB."); return FALSE; }
      parm->DestSectors=(UINT)(value*1024.0*2048.0+0.5); p++;
   } else if (*p=='M' || *p=='m') {
      if (value>2047.0*1024.0) { SetError("This utility cannot enlarge a virtual drive beyond 2047 GB."); return FALSE; }
      parm->DestSectors=(UINT)(value*2048.0+0.5); p++;
   } else { SetError("The new drive size must end in MB or GB."); return FALSE; }
   if (*p=='B' || *p=='b') p++;
   while (*p==' ' || *p=='\t') p++;
   if (*p) { SetError("The new drive size contains unexpected characters."); return FALSE; }
   return TRUE;
}

PUBLIC BOOL VDIIP_Proceed(HINSTANCE hInstRes, HWND hWndParent, s_CLONEPARMS *parm, const ENC_REPORT *encryption)
{
   VDIIP_CONTEXT c;
   BOOL ok;
   LastErrorText[0]=0;
   if (Encryption_HasWarning(encryption) && !(parm->flags&PARM_FLAG_FORCE_ENCRYPTED)) {
      SetError("Encryption was detected or could not be ruled out. Explicit confirmation is required for in-place modification.");
      return FALSE;
   }
   if (VDIIP_HasJournal(parm->srcfn)) ok=LoadJournal(&c,parm->srcfn);
   else {
      if ((parm->flags&PARM_FLAG_ENLARGE) && !ParseRequestedSize(parm)) return FALSE;
      ok=Analyze(&c,parm);
   }
   if (!ok) return FALSE;
   c.noUI=(parm->flags&PARM_FLAG_BATCHMODE)!=0;
   ok=Execute(&c,hInstRes,hWndParent);
   FreeContext(&c);
   return ok;
}
