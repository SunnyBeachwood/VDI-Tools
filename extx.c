/*================================================================================*/
/* Copyright (C) 2009, Don Milne.                                                 */
/* All rights reserved.                                                           */
/* See LICENSE.TXT for conditions on copying, distribution, modification and use. */
/*================================================================================*/

#include "djwarning.h"
#include "djtypes.h"
#include "extx.h"
#include "vddr.h"
#include "ext2_struct.h"
#include "djfile.h"
#include "mem.h"
#include "memfile.h"

#define DUMP_BLOCK_GROUPS 0

#if DUMP_BLOCK_GROUPS
#include "env.h"
#include "djstring.h"
#endif

// I've deliberately decided to use the word "cluster" in this module to refer
// to ext2 blocks, reserving the word "block" to mean a VDI block.

#define CLUSTERS_PER_BUFF 16

typedef struct {
   CLASS(FSYS) Base;
   HVDDR hVDIsrc;
   SUPERBLK sblk;
   HUGE VolumeBaseLBA;
   HUGE LastSectorLBA;
   UINT nBlockGroups;
   UINT BlockGroupSizeBytes; // step size for moving from one block group to the next.
   UINT SectorsPerClusterShift;
   UINT ClusterSize;
   UINT BitmapBytes;
   UINT *Bitmap;
} EXTXVOLINF, *PEXTXVOL;

/*.....................................................*/

static UINT
PowerOfTwo(UINT x)
{
   int y = -1;
   do {
      x >>= 1;
      y++;
   } while (x);
   return (UINT)y;
}

/*.....................................................*/

static BOOL
ReadClusters(PEXTXVOL pExt2, PVOID dest, HUGE LCN, UINT nClusters)
{
   HUGE LBA;
   hugeop_shl(LBA, LCN, pExt2->SectorsPerClusterShift);
   hugeop_add(LBA, LBA, pExt2->VolumeBaseLBA);
   return pExt2->hVDIsrc->ReadSectors(pExt2->hVDIsrc, dest, LBA, (nClusters<<pExt2->SectorsPerClusterShift));
}

/*.....................................................*/

#if DUMP_BLOCK_GROUPS
#define M_PRINTF(f,szctl) Env_sprintf(sz,szctl);MemFile.WrBin(f,sz,String_Length(sz));
#define M_PRINTF1(f,szctl,arg1) Env_sprintf(sz,szctl,arg1);MemFile.WrBin(f,sz,String_Length(sz));
#endif

/*.....................................................*/

#if DUMP_BLOCK_GROUPS

static char sz[1024];

static UINT
GetDescSize(SUPERBLK *psblk)
{
   UINT desc_size = 32;
   if (psblk->s_desc_size >= 32 && (psblk->s_desc_size & (psblk->s_desc_size - 1)) == 0 && psblk->s_desc_size <= 1024) {
      desc_size = psblk->s_desc_size;
   } else if (psblk->s_feature_incompat & EXT4_FEATURE_INCOMPAT_64BIT) {
      desc_size = 64;
   }
   return desc_size;
}

static void
DumpBlockInfo(HVDDR hVDI, HUGE iLBA, SUPERBLK *sblk, UINT *Bitmap)
{
   HMEMFILE f = MemFile.Create(0);
   FILE fb;
   UINT desc_size = GetDescSize(sblk);
   UINT nBlockGroups = (sblk->s_blocks_count + (sblk->s_blocks_per_group-1)) / sblk->s_blocks_per_group;
   UINT nLastGroupClust = sblk->s_blocks_count % sblk->s_blocks_per_group;
   UINT i,j,k,grpbase,tmp,BlocksInGroup,bgdt_block;
   PBYTE pBGDBuff;
   
   if (!nLastGroupClust) nLastGroupClust = sblk->s_blocks_per_group;
   if (sblk->s_log_block_size==0) bgdt_block = 2;
   else bgdt_block = 1;
   
   pBGDBuff = (PBYTE)Mem_Alloc(0, 32 * desc_size);
   if (!pBGDBuff) { MemFile.Close(f); return; }
   
   M_PRINTF(f, "\r\nEXT2 Partition Info\r\n");
   M_PRINTF(f, "===================\r\n");
   M_PRINTF1(f,"Cluster size               : %lu\r\n", (1024<<sblk->s_log_block_size));
   M_PRINTF1(f,"Total number of clusters   : %lu\r\n", sblk->s_blocks_count);
   M_PRINTF1(f,"Number of cluster groups   : %lu\r\n", nBlockGroups);
   M_PRINTF1(f,"Clusters per group         : %lu\r\n", sblk->s_blocks_per_group);
   M_PRINTF1(f,"Clusters in last group     : %lu\r\n", nLastGroupClust);
   M_PRINTF1(f,"Inodes per group           : %lu\r\n", sblk->s_inodes_per_group);
   M_PRINTF1(f,"Descriptor size            : %lu\r\n", desc_size);
   M_PRINTF1(f,"Block Group Table in block : %lu\r\n", bgdt_block);
   
   M_PRINTF(f, "\r\nBlock Group Descriptor Table\r\n");
   M_PRINTF(f, "===================\r\n\r\n");
   M_PRINTF(f,"           Group    Bitmap   i-bitmap   i-table    free      free      used  First 64\r\n");
   M_PRINTF(f,"GroupID    Base    Block-id  Block-id  Block-id   Blocks    inodes     dirs  Bitmap Entries\r\n");
   M_PRINTF(f,"-------  --------  --------  --------  --------  --------  --------  ------  --------------\r\n");
   iLBA += (bgdt_block * (2<<sblk->s_log_block_size));
   for (i=0; i<nBlockGroups; i+=32) {
      UINT nSectorsToRead;
      BlocksInGroup = 32;
      if ((i+32)>nBlockGroups) BlocksInGroup = nBlockGroups - i;
      nSectorsToRead = (BlocksInGroup * desc_size + 511) / 512;
      hVDI->ReadSectors(hVDI, pBGDBuff, iLBA, nSectorsToRead);
      for (j=0; j<BlocksInGroup; j++) {
         PBYTE pDesc = pBGDBuff + j * desc_size;
         UINT bm_lo = *(PUINT)(pDesc + 0);
         UINT ibm_lo = *(PUINT)(pDesc + 4);
         UINT itbl_lo = *(PUINT)(pDesc + 8);
         WORD free_b = *(PWORD)(pDesc + 12);
         WORD free_i = *(PWORD)(pDesc + 14);
         WORD used_d = *(PWORD)(pDesc + 16);
         Env_sprintf(sz,"%7lu%10lu%10lu%10lu%10lu%10lu%10lu%8lu  ",
                  i+j,(i+j)*sblk->s_blocks_per_group,
                  bm_lo, ibm_lo, itbl_lo, free_b, free_i, used_d);
         MemFile.WrBin(f,sz,String_Length(sz));
         grpbase = (i+j)*sblk->s_blocks_per_group;
         for (k=0; k<64; k++) {
            sz[k] = '0';
            tmp = grpbase+k;
            if (Bitmap[tmp>>5] & (1<<(tmp&0x1F))) sz[k]='1';
         }
         sz[64] = '\r';
         sz[65] = '\n';
         MemFile.WrBin(f,sz,66);
      }
      iLBA += nSectorsToRead;
   }
   
   Mem_Free(pBGDBuff);
   fb = File_Create("ext2info.txt",TRUE);
   File_WrBin(fb,MemFile.GetPtr(f,0),MemFile.Size(f));
   File_Close(fb);
   MemFile.Close(f);
}

#else

static UINT
GetDescSize(SUPERBLK *psblk)
{
   UINT desc_size = 32;
   if (psblk->s_desc_size >= 32 && (psblk->s_desc_size & (psblk->s_desc_size - 1)) == 0 && psblk->s_desc_size <= 1024) {
      desc_size = psblk->s_desc_size;
   } else if (psblk->s_feature_incompat & EXT4_FEATURE_INCOMPAT_64BIT) {
      desc_size = 64;
   }
   return desc_size;
}

#endif

/*.....................................................*/

static SUPERBLK sblk;

PUBLIC BOOL
Extx_IsLinuxVolume(HVDDR hVDI, HUGE iLBA)
{
   if (hVDI->ReadSectors(hVDI, &sblk, iLBA+2, 2)) {
      if ((sblk.s_magic == EXT2_SUPER_MAGIC) &&
          (sblk.s_first_data_block<2) &&
          (sblk.s_log_block_size<=5) &&
          (sblk.s_blocks_per_group>0)) {
         return TRUE;
      }
   }
   return FALSE;
}

/*.....................................................*/

static BOOL
ReadBitmap(PEXTXVOL pExt2)
{
   UINT i,bgdt_block,bgdt_size_clusters,desc_size;
   PBYTE pBGDT;
   
   desc_size = GetDescSize(&pExt2->sblk);

   // Calculate the LCN of the block group descriptor table, and its size in clusters.
   if (pExt2->sblk.s_log_block_size==0) bgdt_block = 2;
   else bgdt_block = 1;
   bgdt_size_clusters = (pExt2->nBlockGroups*desc_size + (pExt2->ClusterSize-1))/pExt2->ClusterSize;
   
   // allocate memory for the block group table, then read it.
   pBGDT = (PBYTE)Mem_Alloc(0,bgdt_size_clusters*pExt2->ClusterSize);
   if (pBGDT && ReadClusters(pExt2,pBGDT,bgdt_block,bgdt_size_clusters)!=VDDR_RSLT_FAIL) {
         
      // allocate the bitmap memory for the entire partition.
      pExt2->Bitmap = Mem_Alloc(0,pExt2->ClusterSize*pExt2->nBlockGroups+4); // extra 4 bytes to allow dword lookahead within bitmap
      if (pExt2->Bitmap) {
         BYTE *pDest = (BYTE*)pExt2->Bitmap;
         // Read the individual bitmap blocks;
         for (i=0; i<pExt2->nBlockGroups; i++) {
            PBYTE pDesc = pBGDT + i * desc_size;
            UINT lo = *(PUINT)(pDesc + 0); // bg_block_bitmap
            UINT hi = 0;
            HUGE lcn;
            if (desc_size >= 64 && (pExt2->sblk.s_feature_incompat & EXT4_FEATURE_INCOMPAT_64BIT)) {
               hi = *(PUINT)(pDesc + 0x20); // bg_block_bitmap_hi
            }
            lcn = MAKEHUGE(lo, hi);
            if (ReadClusters(pExt2,pDest,lcn,1)==VDDR_RSLT_FAIL) {
               Mem_Free(pExt2->Bitmap);
               pExt2->Bitmap = NULL;
               goto _err_abort;
            }
            pDest += pExt2->ClusterSize;
         }
         Mem_Free(pBGDT);
         return TRUE;
      }
_err_abort:
      Mem_Free(pBGDT);
   }
   return FALSE;
}

/*.....................................................*/

PUBLIC HFSYS
Extx_OpenVolume(HVDDR hVDI, HUGE iLBA, HUGE cLBA, UINT cSectorSize)
{
   if (Extx_IsLinuxVolume(hVDI,iLBA)) {
      PEXTXVOL pExt2 = Mem_Alloc(MEMF_ZEROINIT, sizeof(EXTXVOLINF));
      if (pExt2) {
         HUGE volume_sectors;
         HUGE total_blocks;
         
         pExt2->Base.CloseVolume = Extx_CloseVolume;
         pExt2->Base.IsBlockUsed = Extx_IsBlockUsed;
         Mem_Copy(&pExt2->sblk, &sblk, sizeof(sblk));
         
         pExt2->hVDIsrc = hVDI;
         
         hugeop_fromuint(total_blocks, pExt2->sblk.s_blocks_count);
         if ((pExt2->sblk.s_feature_incompat & EXT4_FEATURE_INCOMPAT_64BIT) && pExt2->sblk.s_blocks_count_hi) {
            HUGE hi_blocks;
            hugeop_fromuint(hi_blocks, pExt2->sblk.s_blocks_count_hi);
            hugeop_shl(hi_blocks, hi_blocks, 32);
            hugeop_add(total_blocks, total_blocks, hi_blocks);
         }
         
         pExt2->nBlockGroups = (UINT)((total_blocks + (pExt2->sblk.s_blocks_per_group-1)) /
                               pExt2->sblk.s_blocks_per_group);
         pExt2->ClusterSize = (1024<<pExt2->sblk.s_log_block_size);
         pExt2->SectorsPerClusterShift = PowerOfTwo((pExt2->ClusterSize>>9));
         pExt2->BlockGroupSizeBytes = pExt2->sblk.s_blocks_per_group*pExt2->ClusterSize;
         pExt2->VolumeBaseLBA = iLBA;
         
         // calculate end sector address.
         volume_sectors = total_blocks;
         hugeop_shl(volume_sectors, volume_sectors, pExt2->SectorsPerClusterShift);
         hugeop_add(pExt2->LastSectorLBA,pExt2->VolumeBaseLBA,volume_sectors);
         
         if (ReadBitmap(pExt2)) {
#if DUMP_BLOCK_GROUPS
            DumpBlockInfo(hVDI,iLBA,&pExt2->sblk,pExt2->Bitmap);
#endif
            return (HFSYS)pExt2;
         }
      }
      Mem_Free(pExt2);
   }
   return NULL;
}

/*.....................................................*/

PUBLIC HFSYS
Extx_CloseVolume(HFSYS hExt)
{
   if (hExt) {
      PEXTXVOL pExt2 = (PEXTXVOL)hExt;
      Mem_Free(pExt2->Bitmap);
      Mem_Free(pExt2);
   }
   return NULL;
}

/*.....................................................*/

static BOOL
SomeClustersUsed(PEXTXVOL pExt2, UINT i, UINT nClusters)
// Return TRUE if any cluster LCN in the range from i to i+nClusters-1
// is in use.
{
   UINT *Bitmap = pExt2->Bitmap + (i>>5);
   UINT nBits = (i & 0x1F); // count of unwanted lsbs
   UINT mask = (*Bitmap++) & (~((1<<nBits)-1)); // zero bit range from 0..[startbit-1] for first UINT only
   nClusters += nBits; // let main loop treat mask.bit0 onwards as valid
   for (; nClusters>=32; nClusters-=32) {
      if (mask) return TRUE;
      mask = (*Bitmap++);
   }
   if (nClusters) {
      if (mask & ((1<<nClusters)-1)) return TRUE;
   }
   return FALSE;
}

/*.....................................................*/

PUBLIC int
Extx_IsBlockUsed(HFSYS hExt, UINT iBlock, UINT SectorsPerBlockShift)
{
   if (hExt) {
      PEXTXVOL pExt2 = (PEXTXVOL)hExt;
      UINT SectorsPerBlock = (1<<SectorsPerBlockShift);
      HUGE LBAstart, LBAend, htemp;

      LBAstart = MAKEHUGE(iBlock,0);

      hugeop_shl(LBAstart, LBAstart, SectorsPerBlockShift);
      if (hugeop_compare(LBAstart,pExt2->VolumeBaseLBA)>=0) { // if LBAstart>=BootSectorLBA ...
         LBAend = LBAstart;
         hugeop_adduint(LBAend, LBAend, SectorsPerBlock);
         if (hugeop_compare(LBAend,pExt2->LastSectorLBA)<=0) { // if LBAend<LastSectorLBA ...
            UINT iFirstCluster,iLastCluster;

            // convert start/end LBA to volume relative form.
            hugeop_sub(LBAstart,LBAstart,pExt2->VolumeBaseLBA);
            hugeop_sub(LBAend,LBAend,pExt2->VolumeBaseLBA);
            
            // then convert from sector addressing to cluster addressing
            hugeop_shr(htemp, LBAstart, pExt2->SectorsPerClusterShift);
            iFirstCluster = LO32(htemp);
            hugeop_shr(htemp, LBAend, pExt2->SectorsPerClusterShift);
            iLastCluster = LO32(htemp);

            // we need to take account of the possibility that block boundaries are not
            // aligned with clusters (the LBAstart shift is safe).
            if (LO32(LBAend) & ((1<<(pExt2->SectorsPerClusterShift))-1)) iLastCluster++;

            if (SomeClustersUsed(pExt2,iFirstCluster,iLastCluster-iFirstCluster)) return FSYS_BLOCK_USED;
            return FSYS_BLOCK_UNUSED;
         }
      }
   }
   return FSYS_BLOCK_OUTSIDE;
}

/*.....................................................*/

PUBLIC UINT
Extx_GrowPartition(HVDDR hVDI, UINT iLBA, UINT OldSectors, UINT NewSectors, UINT cHeads)
{
   return OldSectors;
}

/*.....................................................*/

/* end of extx.c */

