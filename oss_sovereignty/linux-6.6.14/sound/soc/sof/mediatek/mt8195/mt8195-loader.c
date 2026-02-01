







#include <sound/sof.h>
#include "mt8195.h"
#include "../../ops.h"

void sof_hifixdsp_boot_sequence(struct snd_sof_dev *sdev, u32 boot_addr)
{
	 
	snd_sof_dsp_write(sdev, DSP_REG_BAR, DSP_ALTRESETVEC, boot_addr);

	 
	snd_sof_dsp_update_bits(sdev, DSP_REG_BAR, DSP_RESET_SW,
				ADSP_RUNSTALL, ADSP_RUNSTALL);

	 
	snd_sof_dsp_update_bits(sdev, DSP_REG_BAR, DSP_RESET_SW,
				STATVECTOR_SEL, STATVECTOR_SEL);

	 
	 
	snd_sof_dsp_update_bits(sdev, DSP_REG_BAR, DSP_RESET_SW,
				ADSP_BRESET_SW | ADSP_DRESET_SW,
				ADSP_BRESET_SW | ADSP_DRESET_SW);

	 
	udelay(1);

	 
	snd_sof_dsp_update_bits(sdev, DSP_REG_BAR, DSP_RESET_SW,
				ADSP_BRESET_SW | ADSP_DRESET_SW,
				0);

	 
	snd_sof_dsp_update_bits(sdev, DSP_REG_BAR, DSP_PDEBUGBUS0,
				PDEBUG_ENABLE,
				PDEBUG_ENABLE);

	 
	snd_sof_dsp_update_bits(sdev, DSP_REG_BAR, DSP_RESET_SW,
				ADSP_RUNSTALL, 0);
}

void sof_hifixdsp_shutdown(struct snd_sof_dev *sdev)
{
	 
	snd_sof_dsp_update_bits(sdev, DSP_REG_BAR, DSP_RESET_SW,
				ADSP_RUNSTALL, ADSP_RUNSTALL);

	 
	snd_sof_dsp_update_bits(sdev, DSP_REG_BAR, DSP_RESET_SW,
				ADSP_BRESET_SW | ADSP_DRESET_SW,
				ADSP_BRESET_SW | ADSP_DRESET_SW);
}

