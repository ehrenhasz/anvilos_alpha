 
 
#ifndef __AMD_MACH_CONFIG_H
#define __AMD_MACH_CONFIG_H

#include <sound/soc-acpi.h>

#define FLAG_AMD_SOF			BIT(1)
#define FLAG_AMD_SOF_ONLY_DMIC		BIT(2)
#define FLAG_AMD_LEGACY			BIT(3)

#define ACP_PCI_DEV_ID			0x15E2

extern struct snd_soc_acpi_mach snd_soc_acpi_amd_sof_machines[];
extern struct snd_soc_acpi_mach snd_soc_acpi_amd_rmb_sof_machines[];
extern struct snd_soc_acpi_mach snd_soc_acpi_amd_vangogh_sof_machines[];

struct config_entry {
	u32 flags;
	u16 device;
	const struct dmi_system_id *dmi_table;
};

#endif
