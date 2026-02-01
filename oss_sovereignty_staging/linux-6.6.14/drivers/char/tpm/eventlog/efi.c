
 

#include <linux/device.h>
#include <linux/efi.h>
#include <linux/tpm_eventlog.h>

#include "../tpm.h"
#include "common.h"

 
int tpm_read_log_efi(struct tpm_chip *chip)
{

	struct efi_tcg2_final_events_table *final_tbl = NULL;
	int final_events_log_size = efi_tpm_final_log_size;
	struct linux_efi_tpm_eventlog *log_tbl;
	struct tpm_bios_log *log;
	u32 log_size;
	u8 tpm_log_version;
	void *tmp;
	int ret;

	if (!(chip->flags & TPM_CHIP_FLAG_TPM2))
		return -ENODEV;

	if (efi.tpm_log == EFI_INVALID_TABLE_ADDR)
		return -ENODEV;

	log = &chip->log;

	log_tbl = memremap(efi.tpm_log, sizeof(*log_tbl), MEMREMAP_WB);
	if (!log_tbl) {
		pr_err("Could not map UEFI TPM log table !\n");
		return -ENOMEM;
	}

	log_size = log_tbl->size;
	memunmap(log_tbl);

	if (!log_size) {
		pr_warn("UEFI TPM log area empty\n");
		return -EIO;
	}

	log_tbl = memremap(efi.tpm_log, sizeof(*log_tbl) + log_size,
			   MEMREMAP_WB);
	if (!log_tbl) {
		pr_err("Could not map UEFI TPM log table payload!\n");
		return -ENOMEM;
	}

	 
	log->bios_event_log = devm_kmemdup(&chip->dev, log_tbl->log, log_size, GFP_KERNEL);
	if (!log->bios_event_log) {
		ret = -ENOMEM;
		goto out;
	}

	log->bios_event_log_end = log->bios_event_log + log_size;
	tpm_log_version = log_tbl->version;

	ret = tpm_log_version;

	if (efi.tpm_final_log == EFI_INVALID_TABLE_ADDR ||
	    final_events_log_size == 0 ||
	    tpm_log_version != EFI_TCG2_EVENT_LOG_FORMAT_TCG_2)
		goto out;

	final_tbl = memremap(efi.tpm_final_log,
			     sizeof(*final_tbl) + final_events_log_size,
			     MEMREMAP_WB);
	if (!final_tbl) {
		pr_err("Could not map UEFI TPM final log\n");
		devm_kfree(&chip->dev, log->bios_event_log);
		ret = -ENOMEM;
		goto out;
	}

	 
	final_events_log_size -= log_tbl->final_events_preboot_size;

	 
	tmp = devm_krealloc(&chip->dev, log->bios_event_log,
			    log_size + final_events_log_size,
			    GFP_KERNEL);
	if (!tmp) {
		devm_kfree(&chip->dev, log->bios_event_log);
		ret = -ENOMEM;
		goto out;
	}

	log->bios_event_log = tmp;

	 
	memcpy((void *)log->bios_event_log + log_size,
	       final_tbl->events + log_tbl->final_events_preboot_size,
	       final_events_log_size);
	 
	log->bios_event_log_end = log->bios_event_log +
		log_size + final_events_log_size;

out:
	memunmap(final_tbl);
	memunmap(log_tbl);
	return ret;
}
