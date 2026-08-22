#ifndef CLONEVDI_MODERN_COMPAT_H
#define CLONEVDI_MODERN_COMPAT_H

/*
 * The original VS2008 project used global 4-byte structure packing. Modern
 * Windows SDK headers require their default packing, so load them first and
 * then restore the packing expected by CloneVDI's on-disk structures.
 */
#include <windows.h>
#include <math.h>

/*
 * The current UCRT exports a legacy floating-point symbol named HUGE.  The
 * original codebase uses HUGE as its signed 64-bit type, so alias that type
 * name only after the Windows headers have been parsed.
 */
#define HUGE DJ_HUGE

#pragma pack(push,4)

#endif
