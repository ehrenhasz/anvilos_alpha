

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/cred.h>
#include <linux/err.h>
#include <linux/efi.h>
#include <linux/slab.h>
#include <keys/asymmetric-type.h>
#include <keys/system_keyring.h>
#include "../integrity.h"
#include "keyring_handler.h"

static efi_guid_t efi_cert_x509_guid __initdata = EFI_CERT_X509_GUID;
static efi_guid_t efi_cert_x509_sha256_guid __initdata =
	EFI_CERT_X509_SHA256_GUID;
static efi_guid_t efi_cert_sha256_guid __initdata = EFI_CERT_SHA256_GUID;

 
static __init void uefi_blacklist_x509_tbs(const char *source,
					   const void *data, size_t len)
{
	mark_hash_blacklisted(data, len, BLACKLIST_HASH_X509_TBS);
}

 
static __init void uefi_blacklist_binary(const char *source,
					 const void *data, size_t len)
{
	mark_hash_blacklisted(data, len, BLACKLIST_HASH_BINARY);
}

 
static __init void uefi_revocation_list_x509(const char *source,
					     const void *data, size_t len)
{
	add_key_to_revocation_list(data, len);
}

 
__init efi_element_handler_t get_handler_for_db(const efi_guid_t *sig_type)
{
	if (efi_guidcmp(*sig_type, efi_cert_x509_guid) == 0)
		return add_to_platform_keyring;
	return NULL;
}

 
__init efi_element_handler_t get_handler_for_mok(const efi_guid_t *sig_type)
{
	if (efi_guidcmp(*sig_type, efi_cert_x509_guid) == 0) {
		if (IS_ENABLED(CONFIG_INTEGRITY_MACHINE_KEYRING) &&
		    imputed_trust_enabled())
			return add_to_machine_keyring;
		else
			return add_to_platform_keyring;
	}
	return NULL;
}

__init efi_element_handler_t get_handler_for_ca_keys(const efi_guid_t *sig_type)
{
	if (efi_guidcmp(*sig_type, efi_cert_x509_guid) == 0)
		return add_to_machine_keyring;

	return NULL;
}

__init efi_element_handler_t get_handler_for_code_signing_keys(const efi_guid_t *sig_type)
{
	if (efi_guidcmp(*sig_type, efi_cert_x509_guid) == 0)
		return add_to_secondary_keyring;

	return NULL;
}

 
__init efi_element_handler_t get_handler_for_dbx(const efi_guid_t *sig_type)
{
	if (efi_guidcmp(*sig_type, efi_cert_x509_sha256_guid) == 0)
		return uefi_blacklist_x509_tbs;
	if (efi_guidcmp(*sig_type, efi_cert_sha256_guid) == 0)
		return uefi_blacklist_binary;
	if (efi_guidcmp(*sig_type, efi_cert_x509_guid) == 0)
		return uefi_revocation_list_x509;
	return NULL;
}
