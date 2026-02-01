
 
#include <linux/efi.h>
#include <asm/efi.h>

#include "efistub.h"

 
static const efi_guid_t shim_guid = EFI_SHIM_LOCK_GUID;
static const efi_char16_t shim_MokSBState_name[] = L"MokSBStateRT";

static efi_status_t get_var(efi_char16_t *name, efi_guid_t *vendor, u32 *attr,
			    unsigned long *data_size, void *data)
{
	return get_efi_var(name, vendor, attr, data_size, data);
}

 
enum efi_secureboot_mode efi_get_secureboot(void)
{
	u32 attr;
	unsigned long size;
	enum efi_secureboot_mode mode;
	efi_status_t status;
	u8 moksbstate;

	mode = efi_get_secureboot_mode(get_var);
	if (mode == efi_secureboot_mode_unknown) {
		efi_err("Could not determine UEFI Secure Boot status.\n");
		return efi_secureboot_mode_unknown;
	}
	if (mode != efi_secureboot_mode_enabled)
		return mode;

	 
	size = sizeof(moksbstate);
	status = get_efi_var(shim_MokSBState_name, &shim_guid,
			     &attr, &size, &moksbstate);

	 
	if (status != EFI_SUCCESS)
		goto secure_boot_enabled;
	if (!(attr & EFI_VARIABLE_NON_VOLATILE) && moksbstate == 1)
		return efi_secureboot_mode_disabled;

secure_boot_enabled:
	efi_info("UEFI Secure Boot is enabled.\n");
	return efi_secureboot_mode_enabled;
}
