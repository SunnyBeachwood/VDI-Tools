/* Background batch scheduler shared by the GUI and command line. */
#ifndef TASK_H
#define TASK_H

#include <windows.h>
#include "parms.h"
#include "encryption.h"

typedef enum {
   VDI_JOB_WAITING,
   VDI_JOB_RUNNING,
   VDI_JOB_SUCCEEDED,
   VDI_JOB_FAILED,
   VDI_JOB_SKIPPED,
   VDI_JOB_CANCELLED,
   VDI_JOB_PAUSED
} VDI_JOB_STATE;

typedef struct {
   s_CLONEPARMS parm;
   ENC_REPORT encryption;
   volatile LONG state;
   volatile LONG cancelRequested;
   BOOL overwriteExisting;
   UINT ioBurstBlocks;
   LONGLONG virtualBytes;
   volatile LONGLONG progressDoneBytes;
   volatile LONGLONG progressTotalBytes;
   volatile LONG progressPermille;
   volatile DWORD progressTick;
   CHAR error[1024];
} VDI_JOB;

typedef void (*VDI_JOB_NOTIFY)(void *context, VDI_JOB *job);

typedef struct {
   VDI_JOB *jobs;
   UINT count;
   UINT workerCount;
   HANDLE *workers;
   HANDLE completed;
   volatile LONG nextIndex;
   volatile LONG remaining;
   VDI_JOB_NOTIFY notify;
   void *notifyContext;
} VDI_TASK_BATCH;

UINT Task_DefaultThreadLimit(void);
BOOL TaskBatch_Start(VDI_TASK_BATCH *batch, VDI_JOB *jobs, UINT count, UINT threadLimit, VDI_JOB_NOTIFY notify, void *notifyContext);
BOOL TaskBatch_IsComplete(const VDI_TASK_BATCH *batch);
void TaskBatch_Wait(VDI_TASK_BATCH *batch);
void TaskBatch_RequestCancel(VDI_JOB *job);
void TaskBatch_RequestCancelAll(VDI_TASK_BATCH *batch);
void TaskBatch_Destroy(VDI_TASK_BATCH *batch);
BOOL Task_CurrentCancelRequested(void);
UINT Task_CurrentIOBurstBlocks(void);
void Task_ReportProgress(double doneBytes, double totalBytes);

#endif
