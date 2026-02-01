


 

#include <sound/soc-acpi.h>
#include <sound/soc-acpi-intel-match.h>
#include "../skylake/skl.h"

static struct skl_machine_pdata hda_pdata = {
	.use_tplg_pcm = true,
};

struct snd_soc_acpi_mach snd_soc_acpi_intel_hda_machines[] = {
	{
		 
		.drv_name = "skl_hda_dsp_generic",

		 

		.sof_tplg_filename = "sof-hda-generic.tplg",

		 
		.pdata = &hda_pdata,
	},
	{},
};
EXPORT_SYMBOL_GPL(snd_soc_acpi_intel_hda_machines);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Intel Common ACPI Match module");
