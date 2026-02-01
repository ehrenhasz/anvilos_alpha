 
#ifndef __PSP_V3_1_H__
#define __PSP_V3_1_H__

#include "amdgpu_psp.h"

enum { PSP_DIRECTORY_TABLE_ENTRIES = 4 };
enum { PSP_BINARY_ALIGNMENT = 64 };
enum { PSP_BOOTLOADER_1_MEG_ALIGNMENT = 0x100000 };
enum { PSP_BOOTLOADER_8_MEM_ALIGNMENT = 0x800000 };

void psp_v3_1_set_psp_funcs(struct psp_context *psp);

#endif
