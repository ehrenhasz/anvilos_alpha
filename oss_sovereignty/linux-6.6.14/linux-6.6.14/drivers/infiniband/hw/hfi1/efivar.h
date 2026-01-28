#ifndef _HFI1_EFIVAR_H
#define _HFI1_EFIVAR_H
#include <linux/efi.h>
#include "hfi.h"
int read_hfi1_efi_var(struct hfi1_devdata *dd, const char *kind,
		      unsigned long *size, void **return_data);
#endif  
