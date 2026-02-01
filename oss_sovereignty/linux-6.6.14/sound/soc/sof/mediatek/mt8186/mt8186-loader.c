








#include <sound/sof.h>
#include "mt8186.h"
#include "../../ops.h"

void mt8186_sof_hifixdsp_boot_sequence(struct snd_sof_dev *sdev, u32 boot_addr)
{
	 
	snd_sof_dsp_update_bits(sdev, DSP_REG_BAR, ADSP_HIFI_IO_CONFIG,
				RUNSTALL, RUNSTALL);

	 
	snd_sof_dsp_update_bits(sdev, DSP_REG_BAR, ADSP_MBOX_IRQ_EN,
				DSP_MBOX0_IRQ_EN | DSP_MBOX1_IRQ_EN,
				DSP_MBOX0_IRQ_EN | DSP_MBOX1_IRQ_EN);

	 
	snd_sof_dsp_write(sdev, DSP_SECREG_BAR, ADSP_ALTVEC_C0, boot_addr);
	snd_sof_dsp_write(sdev, DSP_SECREG_BAR, ADSP_ALTVECSEL, ADSP_ALTVECSEL_C0);

	 
	snd_sof_dsp_update_bits(sdev, DSP_REG_BAR, ADSP_CFGREG_SW_RSTN,
				SW_RSTN_C0 | SW_DBG_RSTN_C0,
				SW_RSTN_C0 | SW_DBG_RSTN_C0);

	 
	udelay(1);

	 
	snd_sof_dsp_update_bits(sdev, DSP_REG_BAR, ADSP_CFGREG_SW_RSTN,
				SW_RSTN_C0 | SW_DBG_RSTN_C0,
				0);

	 
	snd_sof_dsp_update_bits(sdev, DSP_REG_BAR, ADSP_HIFI_IO_CONFIG,
				RUNSTALL, 0);
}

void mt8186_sof_hifixdsp_shutdown(struct snd_sof_dev *sdev)
{
	 
	snd_sof_dsp_update_bits(sdev, DSP_REG_BAR, ADSP_HIFI_IO_CONFIG,
				RUNSTALL, RUNSTALL);

	 
	snd_sof_dsp_update_bits(sdev, DSP_REG_BAR, ADSP_CFGREG_SW_RSTN,
				SW_RSTN_C0 | SW_DBG_RSTN_C0,
				SW_RSTN_C0 | SW_DBG_RSTN_C0);
}

