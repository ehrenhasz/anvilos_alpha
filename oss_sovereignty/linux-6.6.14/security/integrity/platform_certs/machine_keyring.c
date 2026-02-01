
 

#include <linux/efi.h>
#include "../integrity.h"

static __init int machine_keyring_init(void)
{
	int rc;

	rc = integrity_init_keyring(INTEGRITY_KEYRING_MACHINE);
	if (rc)
		return rc;

	pr_notice("Machine keyring initialized\n");
	return 0;
}
device_initcall(machine_keyring_init);

void __init add_to_machine_keyring(const char *source, const void *data, size_t len)
{
	key_perm_t perm;
	int rc;

	perm = (KEY_POS_ALL & ~KEY_POS_SETATTR) | KEY_USR_VIEW;
	rc = integrity_load_cert(INTEGRITY_KEYRING_MACHINE, source, data, len, perm);

	 
	if (rc && efi_enabled(EFI_BOOT) &&
	    IS_ENABLED(CONFIG_INTEGRITY_PLATFORM_KEYRING))
		rc = integrity_load_cert(INTEGRITY_KEYRING_PLATFORM, source,
					 data, len, perm);

	if (rc)
		pr_info("Error adding keys to machine keyring %s\n", source);
}

 
static __init bool uefi_check_trust_mok_keys(void)
{
	struct efi_mokvar_table_entry *mokvar_entry;

	mokvar_entry = efi_mokvar_entry_find("MokListTrustedRT");

	if (mokvar_entry)
		return true;

	return false;
}

static bool __init trust_moklist(void)
{
	static bool initialized;
	static bool trust_mok;

	if (!initialized) {
		initialized = true;
		trust_mok = false;

		if (uefi_check_trust_mok_keys())
			trust_mok = true;
	}

	return trust_mok;
}

 
bool __init imputed_trust_enabled(void)
{
	if (efi_enabled(EFI_BOOT))
		return trust_moklist();

	return true;
}
