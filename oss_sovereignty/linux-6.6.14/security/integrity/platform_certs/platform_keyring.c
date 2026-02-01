
 

#include <linux/export.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/cred.h>
#include <linux/err.h>
#include <linux/slab.h>
#include "../integrity.h"

 
void __init add_to_platform_keyring(const char *source, const void *data,
				    size_t len)
{
	key_perm_t perm;
	int rc;

	perm = (KEY_POS_ALL & ~KEY_POS_SETATTR) | KEY_USR_VIEW;

	rc = integrity_load_cert(INTEGRITY_KEYRING_PLATFORM, source, data, len,
				 perm);
	if (rc)
		pr_info("Error adding keys to platform keyring %s\n", source);
}

 
static __init int platform_keyring_init(void)
{
	int rc;

	rc = integrity_init_keyring(INTEGRITY_KEYRING_PLATFORM);
	if (rc)
		return rc;

	pr_notice("Platform Keyring initialized\n");
	return 0;
}

 
device_initcall(platform_keyring_init);
