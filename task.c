#include "djwarning.h"
#include <stdint.h>
#include <process.h>
#include <windows.h>
#include "task.h"
#include "clone.h"
#include "vdiinplace.h"
#include "vddr.h"
#include "vdiw.h"
#include "encryption.h"
#include "filename.h"
#include "djstring.h"

static __declspec(thread) VDI_JOB *CurrentJob;

BOOL Task_CurrentCancelRequested(void)
{
   return CurrentJob && InterlockedCompareExchange(&CurrentJob->cancelRequested,0,0);
}

UINT Task_CurrentIOBurstBlocks(void)
{
   return CurrentJob && CurrentJob->ioBurstBlocks ? CurrentJob->ioBurstBlocks : 16;
}

void Task_ReportProgress(double doneBytes, double totalBytes)
{
   DWORD now;
   LONGLONG done,total;
   LONG permille;
   if (!CurrentJob) return;
   done=(doneBytes>0.0 ? (LONGLONG)doneBytes : 0);
   total=(totalBytes>0.0 ? (LONGLONG)totalBytes : 0);
   permille=(total>0 ? (LONG)((done*1000)/total) : 0);
   if (permille>1000) permille=1000;
   InterlockedExchange64(&CurrentJob->progressDoneBytes,done);
   InterlockedExchange64(&CurrentJob->progressTotalBytes,total);
   InterlockedExchange(&CurrentJob->progressPermille,permille);
   now=GetTickCount();
   if (!CurrentJob->progressTick || (DWORD)(now-CurrentJob->progressTick)>=200 || permille>=1000) {
      CurrentJob->progressTick=now;
      /* The GUI callback only posts a message; it is safe from worker threads. */
      /* Notify is reached through the owning batch in the worker's normal state updates. */
   }
}

static void Notify(VDI_TASK_BATCH *batch, VDI_JOB *job)
{
   if (batch->notify) batch->notify(batch->notifyContext,job);
}

static void SetFailure(VDI_TASK_BATCH *batch, VDI_JOB *job, PSTR message)
{
   lstrcpyn(job->error,message ? message : "Unknown task error",sizeof(job->error));
   InterlockedExchange(&job->state,VDI_JOB_FAILED);
   Notify(batch,job);
}

static BOOL Exists(CPFN filename)
{
   DWORD attrs=GetFileAttributes(filename);
   return attrs!=INVALID_FILE_ATTRIBUTES && !(attrs&FILE_ATTRIBUTE_DIRECTORY);
}

static BOOL ScanEncryption(VDI_JOB *job)
{
   HVDDR disk=VDDR_Open(job->parm.srcfn,0);
   if (!disk) {
      lstrcpyn(job->error,VDDR_GetErrorString(0xFFFFFFFF),sizeof(job->error));
      return FALSE;
   }
   Encryption_Scan(disk,&job->encryption);
   disk->Close(disk);
   return TRUE;
}

static void RunOne(VDI_TASK_BATCH *batch, VDI_JOB *job)
{
   BOOL ok;
   s_CLONEPARMS parm=job->parm;
   FNCHAR finalName[1024],temporaryName[1024];
   if (InterlockedCompareExchange(&job->cancelRequested,0,0)) {
      InterlockedExchange(&job->state,VDI_JOB_CANCELLED);
      Notify(batch,job);
      return;
   }
   InterlockedExchange(&job->state,VDI_JOB_RUNNING);
   Notify(batch,job);
   CurrentJob=job;
   parm.flags=(parm.flags & ~PARM_FLAG_CLIMODE)|PARM_FLAG_BATCHMODE;
   VDIW_SetNonInteractive(TRUE);
   if (!ScanEncryption(job)) { SetFailure(batch,job,job->error); CurrentJob=NULL; return; }
   if (parm.flags&PARM_FLAG_INPLACE) {
      if (Encryption_HasWarning(&job->encryption) && !(parm.flags&PARM_FLAG_FORCE_ENCRYPTED)) {
         SetFailure(batch,job,"Encryption warning requires explicit confirmation before in-place batch work."); CurrentJob=NULL;
         return;
      }
      ok=VDIIP_Proceed(GetModuleHandle(NULL),NULL,&parm,&job->encryption);
      if (!ok) {
         if (Task_CurrentCancelRequested()) {
            lstrcpyn(job->error,VDIIP_GetErrorString(),sizeof(job->error));
            InterlockedExchange(&job->state,VDI_JOB_PAUSED); Notify(batch,job); CurrentJob=NULL; return;
         }
         SetFailure(batch,job,VDIIP_GetErrorString()); CurrentJob=NULL; return;
      }
   } else {
      String_Copy(finalName,parm.dstfn,1024);
      if (Exists(finalName) && !job->overwriteExisting) {
         SetFailure(batch,job,"Destination VDI already exists. Resolve the conflict before starting the batch."); CurrentJob=NULL;
         return;
      }
      if (lstrlen(finalName)>960) { SetFailure(batch,job,"Destination filename is too long for a temporary batch output."); CurrentJob=NULL; return; }
      wsprintf(temporaryName,"%s.vdi-tools-partial-%lu-%lu",finalName,GetCurrentProcessId(),GetCurrentThreadId());
      String_Copy(parm.dstfn,temporaryName,1024);
      ok=Clone_Proceed(GetModuleHandle(NULL),NULL,&parm);
      if (!ok) {
         if (Task_CurrentCancelRequested()) {
            lstrcpyn(job->error,"Cancelled",sizeof(job->error));
            InterlockedExchange(&job->state,VDI_JOB_CANCELLED); Notify(batch,job); CurrentJob=NULL; return;
         }
         SetFailure(batch,job,Clone_GetLastError()); CurrentJob=NULL; return;
      }
      if (!MoveFileEx(temporaryName,finalName,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) {
         DeleteFile(temporaryName);
         SetFailure(batch,job,"The completed temporary VDI could not replace the destination file."); CurrentJob=NULL; return;
      }
   }
   if (InterlockedCompareExchange(&job->cancelRequested,0,0)) {
      InterlockedExchange(&job->state,(parm.flags&PARM_FLAG_INPLACE) ? VDI_JOB_PAUSED : VDI_JOB_CANCELLED);
   } else InterlockedExchange(&job->state,VDI_JOB_SUCCEEDED);
   Notify(batch,job);
   CurrentJob=NULL;
}

static unsigned __stdcall Worker(void *argument)
{
   VDI_TASK_BATCH *batch=(VDI_TASK_BATCH*)argument;
   for (;;) {
      LONG index=InterlockedIncrement(&batch->nextIndex)-1;
      if ((UINT)index>=batch->count) break;
      RunOne(batch,&batch->jobs[index]);
      if (InterlockedDecrement(&batch->remaining)==0) SetEvent(batch->completed);
   }
   return 0;
}

UINT Task_DefaultThreadLimit(void)
{
   SYSTEM_INFO si;
   UINT n;
   GetSystemInfo(&si);
   n=si.dwNumberOfProcessors;
   if (n<1) n=1;
   if (n>8) n=8;
   return n;
}

BOOL TaskBatch_Start(VDI_TASK_BATCH *batch, VDI_JOB *jobs, UINT count, UINT threadLimit, VDI_JOB_NOTIFY notify, void *notifyContext)
{
   UINT i,blocks;
   MEMORYSTATUSEX memory;
   DWORDLONG budget,perWorker;
   ZeroMemory(batch,sizeof(*batch));
   if (!jobs || !count) return FALSE;
   if (!threadLimit) threadLimit=Task_DefaultThreadLimit();
   if (threadLimit>32) threadLimit=32;
   if (threadLimit>count) threadLimit=count;
   batch->jobs=jobs; batch->count=count; batch->workerCount=threadLimit;
   batch->notify=notify; batch->notifyContext=notifyContext;
   batch->nextIndex=0; batch->remaining=(LONG)count;
   ZeroMemory(&memory,sizeof(memory)); memory.dwLength=sizeof(memory);
   budget=(GlobalMemoryStatusEx(&memory) ? memory.ullAvailPhys/8 : 256ULL*1024*1024);
   if (budget>512ULL*1024*1024) budget=512ULL*1024*1024;
   if (budget<64ULL*1024*1024) budget=64ULL*1024*1024;
   perWorker=budget/(threadLimit ? threadLimit : 1)/2;
   blocks=(UINT)(perWorker/(1024*1024));
   if (blocks<4) blocks=4;
   if (blocks>32) blocks=32;
   batch->completed=CreateEvent(NULL,TRUE,FALSE,NULL);
   batch->workers=(HANDLE*)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(HANDLE)*threadLimit);
   if (!batch->completed || !batch->workers) { TaskBatch_Destroy(batch); return FALSE; }
   for (i=0;i<count;i++) {
      jobs[i].state=VDI_JOB_WAITING;
      jobs[i].cancelRequested=FALSE;
      jobs[i].ioBurstBlocks=blocks;
      jobs[i].progressDoneBytes=0;
      jobs[i].progressTotalBytes=0;
      jobs[i].progressPermille=0;
      jobs[i].progressTick=0;
      jobs[i].error[0]=0;
   }
   for (i=0;i<threadLimit;i++) {
      uintptr_t thread=_beginthreadex(NULL,0,Worker,batch,0,NULL);
      if (!thread) { TaskBatch_RequestCancelAll(batch); break; }
      batch->workers[i]=(HANDLE)thread;
   }
   batch->workerCount=i;
   if (!batch->workerCount) { TaskBatch_Destroy(batch); return FALSE; }
   return TRUE;
}

BOOL TaskBatch_IsComplete(const VDI_TASK_BATCH *batch)
{
   return batch && batch->completed && WaitForSingleObject(batch->completed,0)==WAIT_OBJECT_0;
}

void TaskBatch_Wait(VDI_TASK_BATCH *batch)
{
   if (batch && batch->completed) WaitForSingleObject(batch->completed,INFINITE);
}

void TaskBatch_RequestCancel(VDI_JOB *job)
{
   if (job) InterlockedExchange(&job->cancelRequested,TRUE);
}

void TaskBatch_RequestCancelAll(VDI_TASK_BATCH *batch)
{
   UINT i;
   if (!batch) return;
   for (i=0;i<batch->count;i++) TaskBatch_RequestCancel(&batch->jobs[i]);
}

void TaskBatch_Destroy(VDI_TASK_BATCH *batch)
{
   UINT i;
   if (!batch) return;
   if (batch->workers) {
      for (i=0;i<batch->workerCount;i++) if (batch->workers[i]) {
         WaitForSingleObject(batch->workers[i],INFINITE);
         CloseHandle(batch->workers[i]);
      }
      HeapFree(GetProcessHeap(),0,batch->workers);
   }
   if (batch->completed) CloseHandle(batch->completed);
   ZeroMemory(batch,sizeof(*batch));
}
