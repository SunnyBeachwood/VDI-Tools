#ifndef CLONEVDI_MODERN_COMPAT_H
#define CLONEVDI_MODERN_COMPAT_H

/*
 * The original VS2008 project used global 4-byte structure packing. Modern
 * Windows SDK headers require their default packing, so load them first and
 * then restore the packing expected by CloneVDI's on-disk structures.
 */
#include <windows.h>
#pragma pack(push,4)

#endif
