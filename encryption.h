/* Encryption/container detection for guest volumes. */
#ifndef ENCRYPTION_H
#define ENCRYPTION_H

#include "vddr.h"

#define ENC_TYPE_BITLOCKER  0x0001
#define ENC_TYPE_LUKS       0x0002
#define ENC_TYPE_VERACRYPT  0x0004
#define ENC_TYPE_UNKNOWN    0x0008

#define ENC_CONFIDENCE_SUSPECTED 1
#define ENC_CONFIDENCE_CONFIRMED 2

#define ENC_MAX_FINDINGS 16

typedef struct {
   UINT type;
   UINT confidence;
   HUGE startLBA;
   UINT partition;
} ENC_FINDING;

typedef struct {
   UINT count;
   BOOL complete;
   ENC_FINDING finding[ENC_MAX_FINDINGS];
} ENC_REPORT;

BOOL Encryption_Scan(HVDDR disk, ENC_REPORT *report);
BOOL Encryption_HasWarning(const ENC_REPORT *report);
void Encryption_FormatReport(const ENC_REPORT *report, PSTR text, UINT textSize);

#endif
