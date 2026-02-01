
 

#include <linux/init.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/ima.h>
#include <generated/utsrelease.h>

#include "ima.h"

 
const char boot_aggregate_name[] = "boot_aggregate";
struct tpm_chip *ima_tpm_chip;

 
static int __init ima_add_boot_aggregate(void)
{
	static const char op[] = "add_boot_aggregate";
	const char *audit_cause = "ENOMEM";
	struct ima_template_entry *entry;
	struct integrity_iint_cache tmp_iint, *iint = &tmp_iint;
	struct ima_event_data event_data = { .iint = iint,
					     .filename = boot_aggregate_name };
	struct ima_max_digest_data hash;
	int result = -ENOMEM;
	int violation = 0;

	memset(iint, 0, sizeof(*iint));
	memset(&hash, 0, sizeof(hash));
	iint->ima_hash = &hash.hdr;
	iint->ima_hash->algo = ima_hash_algo;
	iint->ima_hash->length = hash_digest_size[ima_hash_algo];

	 
	if (ima_tpm_chip) {
		result = ima_calc_boot_aggregate(&hash.hdr);
		if (result < 0) {
			audit_cause = "hashing_error";
			goto err_out;
		}
	}

	result = ima_alloc_init_template(&event_data, &entry, NULL);
	if (result < 0) {
		audit_cause = "alloc_entry";
		goto err_out;
	}

	result = ima_store_template(entry, violation, NULL,
				    boot_aggregate_name,
				    CONFIG_IMA_MEASURE_PCR_IDX);
	if (result < 0) {
		ima_free_template_entry(entry);
		audit_cause = "store_entry";
		goto err_out;
	}
	return 0;
err_out:
	integrity_audit_msg(AUDIT_INTEGRITY_PCR, NULL, boot_aggregate_name, op,
			    audit_cause, result, 0);
	return result;
}

#ifdef CONFIG_IMA_LOAD_X509
void __init ima_load_x509(void)
{
	int unset_flags = ima_policy_flag & IMA_APPRAISE;

	ima_policy_flag &= ~unset_flags;
	integrity_load_x509(INTEGRITY_KEYRING_IMA, CONFIG_IMA_X509_PATH);

	 
	evm_load_x509();

	ima_policy_flag |= unset_flags;
}
#endif

int __init ima_init(void)
{
	int rc;

	ima_tpm_chip = tpm_default_chip();
	if (!ima_tpm_chip)
		pr_info("No TPM chip found, activating TPM-bypass!\n");

	rc = integrity_init_keyring(INTEGRITY_KEYRING_IMA);
	if (rc)
		return rc;

	rc = ima_init_crypto();
	if (rc)
		return rc;
	rc = ima_init_template();
	if (rc != 0)
		return rc;

	 
	ima_load_kexec_buffer();

	rc = ima_init_digests();
	if (rc != 0)
		return rc;
	rc = ima_add_boot_aggregate();	 
	if (rc != 0)
		return rc;

	ima_init_policy();

	rc = ima_fs_init();
	if (rc != 0)
		return rc;

	ima_init_key_queue();

	ima_measure_critical_data("kernel_info", "kernel_version",
				  UTS_RELEASE, strlen(UTS_RELEASE), false,
				  NULL, 0);

	return rc;
}
