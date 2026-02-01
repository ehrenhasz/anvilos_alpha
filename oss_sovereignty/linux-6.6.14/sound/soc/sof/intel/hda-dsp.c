












 

#include <linux/module.h>
#include <sound/hdaudio_ext.h>
#include <sound/hda_register.h>
#include <sound/hda-mlink.h>
#include <trace/events/sof_intel.h>
#include "../sof-audio.h"
#include "../ops.h"
#include "hda.h"
#include "hda-ipc.h"

static bool hda_enable_trace_D0I3_S0;
#if IS_ENABLED(CONFIG_SND_SOC_SOF_DEBUG)
module_param_named(enable_trace_D0I3_S0, hda_enable_trace_D0I3_S0, bool, 0444);
MODULE_PARM_DESC(enable_trace_D0I3_S0,
		 "SOF HDA enable trace when the DSP is in D0I3 in S0");
#endif

 

static int hda_dsp_core_reset_enter(struct snd_sof_dev *sdev, unsigned int core_mask)
{
	u32 adspcs;
	u32 reset;
	int ret;

	 
	reset = HDA_DSP_ADSPCS_CRST_MASK(core_mask);
	snd_sof_dsp_update_bits_unlocked(sdev, HDA_DSP_BAR,
					 HDA_DSP_REG_ADSPCS,
					 reset, reset);

	 
	ret = snd_sof_dsp_read_poll_timeout(sdev, HDA_DSP_BAR,
					HDA_DSP_REG_ADSPCS, adspcs,
					((adspcs & reset) == reset),
					HDA_DSP_REG_POLL_INTERVAL_US,
					HDA_DSP_RESET_TIMEOUT_US);
	if (ret < 0) {
		dev_err(sdev->dev,
			"error: %s: timeout on HDA_DSP_REG_ADSPCS read\n",
			__func__);
		return ret;
	}

	 
	adspcs = snd_sof_dsp_read(sdev, HDA_DSP_BAR,
				  HDA_DSP_REG_ADSPCS);
	if ((adspcs & HDA_DSP_ADSPCS_CRST_MASK(core_mask)) !=
		HDA_DSP_ADSPCS_CRST_MASK(core_mask)) {
		dev_err(sdev->dev,
			"error: reset enter failed: core_mask %x adspcs 0x%x\n",
			core_mask, adspcs);
		ret = -EIO;
	}

	return ret;
}

static int hda_dsp_core_reset_leave(struct snd_sof_dev *sdev, unsigned int core_mask)
{
	unsigned int crst;
	u32 adspcs;
	int ret;

	 
	snd_sof_dsp_update_bits_unlocked(sdev, HDA_DSP_BAR,
					 HDA_DSP_REG_ADSPCS,
					 HDA_DSP_ADSPCS_CRST_MASK(core_mask),
					 0);

	 
	crst = HDA_DSP_ADSPCS_CRST_MASK(core_mask);
	ret = snd_sof_dsp_read_poll_timeout(sdev, HDA_DSP_BAR,
					    HDA_DSP_REG_ADSPCS, adspcs,
					    !(adspcs & crst),
					    HDA_DSP_REG_POLL_INTERVAL_US,
					    HDA_DSP_RESET_TIMEOUT_US);

	if (ret < 0) {
		dev_err(sdev->dev,
			"error: %s: timeout on HDA_DSP_REG_ADSPCS read\n",
			__func__);
		return ret;
	}

	 
	adspcs = snd_sof_dsp_read(sdev, HDA_DSP_BAR,
				  HDA_DSP_REG_ADSPCS);
	if ((adspcs & HDA_DSP_ADSPCS_CRST_MASK(core_mask)) != 0) {
		dev_err(sdev->dev,
			"error: reset leave failed: core_mask %x adspcs 0x%x\n",
			core_mask, adspcs);
		ret = -EIO;
	}

	return ret;
}

int hda_dsp_core_stall_reset(struct snd_sof_dev *sdev, unsigned int core_mask)
{
	 
	snd_sof_dsp_update_bits_unlocked(sdev, HDA_DSP_BAR,
					 HDA_DSP_REG_ADSPCS,
					 HDA_DSP_ADSPCS_CSTALL_MASK(core_mask),
					 HDA_DSP_ADSPCS_CSTALL_MASK(core_mask));

	 
	return hda_dsp_core_reset_enter(sdev, core_mask);
}

bool hda_dsp_core_is_enabled(struct snd_sof_dev *sdev, unsigned int core_mask)
{
	int val;
	bool is_enable;

	val = snd_sof_dsp_read(sdev, HDA_DSP_BAR, HDA_DSP_REG_ADSPCS);

#define MASK_IS_EQUAL(v, m, field) ({	\
	u32 _m = field(m);		\
	((v) & _m) == _m;		\
})

	is_enable = MASK_IS_EQUAL(val, core_mask, HDA_DSP_ADSPCS_CPA_MASK) &&
		MASK_IS_EQUAL(val, core_mask, HDA_DSP_ADSPCS_SPA_MASK) &&
		!(val & HDA_DSP_ADSPCS_CRST_MASK(core_mask)) &&
		!(val & HDA_DSP_ADSPCS_CSTALL_MASK(core_mask));

#undef MASK_IS_EQUAL

	dev_dbg(sdev->dev, "DSP core(s) enabled? %d : core_mask %x\n",
		is_enable, core_mask);

	return is_enable;
}

int hda_dsp_core_run(struct snd_sof_dev *sdev, unsigned int core_mask)
{
	int ret;

	 
	ret = hda_dsp_core_reset_leave(sdev, core_mask);
	if (ret < 0)
		return ret;

	 
	dev_dbg(sdev->dev, "unstall/run core: core_mask = %x\n", core_mask);
	snd_sof_dsp_update_bits_unlocked(sdev, HDA_DSP_BAR,
					 HDA_DSP_REG_ADSPCS,
					 HDA_DSP_ADSPCS_CSTALL_MASK(core_mask),
					 0);

	 
	if (!hda_dsp_core_is_enabled(sdev, core_mask)) {
		hda_dsp_core_stall_reset(sdev, core_mask);
		dev_err(sdev->dev, "error: DSP start core failed: core_mask %x\n",
			core_mask);
		ret = -EIO;
	}

	return ret;
}

 

int hda_dsp_core_power_up(struct snd_sof_dev *sdev, unsigned int core_mask)
{
	struct sof_intel_hda_dev *hda = sdev->pdata->hw_pdata;
	const struct sof_intel_dsp_desc *chip = hda->desc;
	unsigned int cpa;
	u32 adspcs;
	int ret;

	 
	core_mask &= chip->host_managed_cores_mask;
	 
	if (!core_mask)
		return 0;

	 
	snd_sof_dsp_update_bits(sdev, HDA_DSP_BAR, HDA_DSP_REG_ADSPCS,
				HDA_DSP_ADSPCS_SPA_MASK(core_mask),
				HDA_DSP_ADSPCS_SPA_MASK(core_mask));

	 
	cpa = HDA_DSP_ADSPCS_CPA_MASK(core_mask);
	ret = snd_sof_dsp_read_poll_timeout(sdev, HDA_DSP_BAR,
					    HDA_DSP_REG_ADSPCS, adspcs,
					    (adspcs & cpa) == cpa,
					    HDA_DSP_REG_POLL_INTERVAL_US,
					    HDA_DSP_RESET_TIMEOUT_US);
	if (ret < 0) {
		dev_err(sdev->dev,
			"error: %s: timeout on HDA_DSP_REG_ADSPCS read\n",
			__func__);
		return ret;
	}

	 
	adspcs = snd_sof_dsp_read(sdev, HDA_DSP_BAR,
				  HDA_DSP_REG_ADSPCS);
	if ((adspcs & HDA_DSP_ADSPCS_CPA_MASK(core_mask)) !=
		HDA_DSP_ADSPCS_CPA_MASK(core_mask)) {
		dev_err(sdev->dev,
			"error: power up core failed core_mask %xadspcs 0x%x\n",
			core_mask, adspcs);
		ret = -EIO;
	}

	return ret;
}

static int hda_dsp_core_power_down(struct snd_sof_dev *sdev, unsigned int core_mask)
{
	u32 adspcs;
	int ret;

	 
	snd_sof_dsp_update_bits_unlocked(sdev, HDA_DSP_BAR,
					 HDA_DSP_REG_ADSPCS,
					 HDA_DSP_ADSPCS_SPA_MASK(core_mask), 0);

	ret = snd_sof_dsp_read_poll_timeout(sdev, HDA_DSP_BAR,
				HDA_DSP_REG_ADSPCS, adspcs,
				!(adspcs & HDA_DSP_ADSPCS_CPA_MASK(core_mask)),
				HDA_DSP_REG_POLL_INTERVAL_US,
				HDA_DSP_PD_TIMEOUT * USEC_PER_MSEC);
	if (ret < 0)
		dev_err(sdev->dev,
			"error: %s: timeout on HDA_DSP_REG_ADSPCS read\n",
			__func__);

	return ret;
}

int hda_dsp_enable_core(struct snd_sof_dev *sdev, unsigned int core_mask)
{
	struct sof_intel_hda_dev *hda = sdev->pdata->hw_pdata;
	const struct sof_intel_dsp_desc *chip = hda->desc;
	int ret;

	 
	core_mask &= chip->host_managed_cores_mask;

	 
	if (!core_mask || hda_dsp_core_is_enabled(sdev, core_mask))
		return 0;

	 
	ret = hda_dsp_core_power_up(sdev, core_mask);
	if (ret < 0) {
		dev_err(sdev->dev, "error: dsp core power up failed: core_mask %x\n",
			core_mask);
		return ret;
	}

	return hda_dsp_core_run(sdev, core_mask);
}

int hda_dsp_core_reset_power_down(struct snd_sof_dev *sdev,
				  unsigned int core_mask)
{
	struct sof_intel_hda_dev *hda = sdev->pdata->hw_pdata;
	const struct sof_intel_dsp_desc *chip = hda->desc;
	int ret;

	 
	core_mask &= chip->host_managed_cores_mask;

	 
	if (!core_mask)
		return 0;

	 
	ret = hda_dsp_core_stall_reset(sdev, core_mask);
	if (ret < 0) {
		dev_err(sdev->dev, "error: dsp core reset failed: core_mask %x\n",
			core_mask);
		return ret;
	}

	 
	ret = hda_dsp_core_power_down(sdev, core_mask);
	if (ret < 0) {
		dev_err(sdev->dev, "error: dsp core power down fail mask %x: %d\n",
			core_mask, ret);
		return ret;
	}

	 
	if (hda_dsp_core_is_enabled(sdev, core_mask)) {
		dev_err(sdev->dev, "error: dsp core disable fail mask %x: %d\n",
			core_mask, ret);
		ret = -EIO;
	}

	return ret;
}

void hda_dsp_ipc_int_enable(struct snd_sof_dev *sdev)
{
	struct sof_intel_hda_dev *hda = sdev->pdata->hw_pdata;
	const struct sof_intel_dsp_desc *chip = hda->desc;

	if (sdev->dspless_mode_selected)
		return;

	 
	snd_sof_dsp_update_bits(sdev, HDA_DSP_BAR, chip->ipc_ctl,
			HDA_DSP_REG_HIPCCTL_DONE | HDA_DSP_REG_HIPCCTL_BUSY,
			HDA_DSP_REG_HIPCCTL_DONE | HDA_DSP_REG_HIPCCTL_BUSY);

	 
	snd_sof_dsp_update_bits(sdev, HDA_DSP_BAR, HDA_DSP_REG_ADSPIC,
				HDA_DSP_ADSPIC_IPC, HDA_DSP_ADSPIC_IPC);
}

void hda_dsp_ipc_int_disable(struct snd_sof_dev *sdev)
{
	struct sof_intel_hda_dev *hda = sdev->pdata->hw_pdata;
	const struct sof_intel_dsp_desc *chip = hda->desc;

	if (sdev->dspless_mode_selected)
		return;

	 
	snd_sof_dsp_update_bits(sdev, HDA_DSP_BAR, HDA_DSP_REG_ADSPIC,
				HDA_DSP_ADSPIC_IPC, 0);

	 
	snd_sof_dsp_update_bits(sdev, HDA_DSP_BAR, chip->ipc_ctl,
			HDA_DSP_REG_HIPCCTL_BUSY | HDA_DSP_REG_HIPCCTL_DONE, 0);
}

static int hda_dsp_wait_d0i3c_done(struct snd_sof_dev *sdev)
{
	int retry = HDA_DSP_REG_POLL_RETRY_COUNT;
	struct snd_sof_pdata *pdata = sdev->pdata;
	const struct sof_intel_dsp_desc *chip;

	chip = get_chip_info(pdata);
	while (snd_sof_dsp_read8(sdev, HDA_DSP_HDA_BAR, chip->d0i3_offset) &
		SOF_HDA_VS_D0I3C_CIP) {
		if (!retry--)
			return -ETIMEDOUT;
		usleep_range(10, 15);
	}

	return 0;
}

static int hda_dsp_send_pm_gate_ipc(struct snd_sof_dev *sdev, u32 flags)
{
	const struct sof_ipc_pm_ops *pm_ops = sof_ipc_get_ops(sdev, pm);

	if (pm_ops && pm_ops->set_pm_gate)
		return pm_ops->set_pm_gate(sdev, flags);

	return 0;
}

static int hda_dsp_update_d0i3c_register(struct snd_sof_dev *sdev, u8 value)
{
	struct snd_sof_pdata *pdata = sdev->pdata;
	const struct sof_intel_dsp_desc *chip;
	int ret;
	u8 reg;

	chip = get_chip_info(pdata);

	 
	ret = hda_dsp_wait_d0i3c_done(sdev);
	if (ret < 0) {
		dev_err(sdev->dev, "CIP timeout before D0I3C update!\n");
		return ret;
	}

	 
	snd_sof_dsp_update8(sdev, HDA_DSP_HDA_BAR, chip->d0i3_offset,
			    SOF_HDA_VS_D0I3C_I3, value);

	 
	usleep_range(30, 40);

	 
	ret = hda_dsp_wait_d0i3c_done(sdev);
	if (ret < 0) {
		dev_err(sdev->dev, "CIP timeout after D0I3C update!\n");
		return ret;
	}

	reg = snd_sof_dsp_read8(sdev, HDA_DSP_HDA_BAR, chip->d0i3_offset);
	 
	if ((reg ^ value) & SOF_HDA_VS_D0I3C_I3) {
		dev_err(sdev->dev, "failed to update D0I3C!\n");
		return -EIO;
	}

	trace_sof_intel_D0I3C_updated(sdev, reg);

	return 0;
}

 
static bool hda_dsp_d0i3_streaming_applicable(struct snd_sof_dev *sdev)
{
	struct snd_pcm_substream *substream;
	struct snd_sof_pcm *spcm;
	bool playback_active = false;
	int dir;

	list_for_each_entry(spcm, &sdev->pcm_list, list) {
		for_each_pcm_streams(dir) {
			substream = spcm->stream[dir].substream;
			if (!substream || !substream->runtime)
				continue;

			if (!spcm->stream[dir].d0i3_compatible)
				return false;

			if (dir == SNDRV_PCM_STREAM_PLAYBACK)
				playback_active = true;
		}
	}

	return playback_active;
}

static int hda_dsp_set_D0_state(struct snd_sof_dev *sdev,
				const struct sof_dsp_power_state *target_state)
{
	u32 flags = 0;
	int ret;
	u8 value = 0;

	 
	switch (sdev->dsp_power_state.state) {
	case SOF_DSP_PM_D0:
		 
		break;
	case SOF_DSP_PM_D3:
		 
		return 0;
	default:
		dev_err(sdev->dev, "error: transition from %d to %d not allowed\n",
			sdev->dsp_power_state.state, target_state->state);
		return -EINVAL;
	}

	 
	if (target_state->substate == SOF_HDA_DSP_PM_D0I3) {
		value = SOF_HDA_VS_D0I3C_I3;

		 
		if (!sdev->fw_trace_is_supported ||
		    !hda_enable_trace_D0I3_S0 ||
		    sdev->system_suspend_target != SOF_SUSPEND_NONE)
			flags = HDA_PM_NO_DMA_TRACE;

		if (hda_dsp_d0i3_streaming_applicable(sdev))
			flags |= HDA_PM_PG_STREAMING;
	} else {
		 
		flags = HDA_PM_PPG;
	}

	 
	ret = hda_dsp_update_d0i3c_register(sdev, value);
	if (ret < 0)
		return ret;

	 
	ret = hda_dsp_send_pm_gate_ipc(sdev, flags);
	if (ret < 0) {
		dev_err(sdev->dev,
			"error: PM_GATE ipc error %d\n", ret);
		goto revert;
	}

	return ret;

revert:
	 
	value = value ? 0 : SOF_HDA_VS_D0I3C_I3;

	 
	hda_dsp_update_d0i3c_register(sdev, value);

	return ret;
}

 
static void hda_dsp_state_log(struct snd_sof_dev *sdev)
{
	switch (sdev->dsp_power_state.state) {
	case SOF_DSP_PM_D0:
		switch (sdev->dsp_power_state.substate) {
		case SOF_HDA_DSP_PM_D0I0:
			dev_dbg(sdev->dev, "Current DSP power state: D0I0\n");
			break;
		case SOF_HDA_DSP_PM_D0I3:
			dev_dbg(sdev->dev, "Current DSP power state: D0I3\n");
			break;
		default:
			dev_dbg(sdev->dev, "Unknown DSP D0 substate: %d\n",
				sdev->dsp_power_state.substate);
			break;
		}
		break;
	case SOF_DSP_PM_D1:
		dev_dbg(sdev->dev, "Current DSP power state: D1\n");
		break;
	case SOF_DSP_PM_D2:
		dev_dbg(sdev->dev, "Current DSP power state: D2\n");
		break;
	case SOF_DSP_PM_D3:
		dev_dbg(sdev->dev, "Current DSP power state: D3\n");
		break;
	default:
		dev_dbg(sdev->dev, "Unknown DSP power state: %d\n",
			sdev->dsp_power_state.state);
		break;
	}
}

 
static int hda_dsp_set_power_state(struct snd_sof_dev *sdev,
				   const struct sof_dsp_power_state *target_state)
{
	int ret = 0;

	switch (target_state->state) {
	case SOF_DSP_PM_D0:
		ret = hda_dsp_set_D0_state(sdev, target_state);
		break;
	case SOF_DSP_PM_D3:
		 
		if (sdev->dsp_power_state.state == SOF_DSP_PM_D0 &&
		    sdev->dsp_power_state.substate == SOF_HDA_DSP_PM_D0I0)
			break;

		dev_err(sdev->dev,
			"error: transition from %d to %d not allowed\n",
			sdev->dsp_power_state.state, target_state->state);
		return -EINVAL;
	default:
		dev_err(sdev->dev, "error: target state unsupported %d\n",
			target_state->state);
		return -EINVAL;
	}
	if (ret < 0) {
		dev_err(sdev->dev,
			"failed to set requested target DSP state %d substate %d\n",
			target_state->state, target_state->substate);
		return ret;
	}

	sdev->dsp_power_state = *target_state;
	hda_dsp_state_log(sdev);
	return ret;
}

int hda_dsp_set_power_state_ipc3(struct snd_sof_dev *sdev,
				 const struct sof_dsp_power_state *target_state)
{
	 
	if (target_state->substate == SOF_HDA_DSP_PM_D0I3 &&
	    sdev->system_suspend_target == SOF_SUSPEND_S0IX)
		return hda_dsp_set_power_state(sdev, target_state);

	 
	if (target_state->state == sdev->dsp_power_state.state &&
	    target_state->substate == sdev->dsp_power_state.substate)
		return 0;

	return hda_dsp_set_power_state(sdev, target_state);
}

int hda_dsp_set_power_state_ipc4(struct snd_sof_dev *sdev,
				 const struct sof_dsp_power_state *target_state)
{
	 
	if (target_state->state == sdev->dsp_power_state.state &&
	    target_state->substate == sdev->dsp_power_state.substate)
		return 0;

	return hda_dsp_set_power_state(sdev, target_state);
}

 

static int hda_suspend(struct snd_sof_dev *sdev, bool runtime_suspend)
{
	struct sof_intel_hda_dev *hda = sdev->pdata->hw_pdata;
	const struct sof_intel_dsp_desc *chip = hda->desc;
	struct hdac_bus *bus = sof_to_bus(sdev);
	int ret, j;

	 
	if (sdev->system_suspend_target > SOF_SUSPEND_S3 ||
	    sdev->fw_state == SOF_FW_CRASHED ||
	    sdev->fw_state == SOF_FW_BOOT_FAILED)
		hda->skip_imr_boot = true;

	ret = chip->disable_interrupts(sdev);
	if (ret < 0)
		return ret;

	hda_codec_jack_wake_enable(sdev, runtime_suspend);

	 
	hda_bus_ml_suspend(bus);

	if (sdev->dspless_mode_selected)
		goto skip_dsp;

	ret = chip->power_down_dsp(sdev);
	if (ret < 0) {
		dev_err(sdev->dev, "failed to power down DSP during suspend\n");
		return ret;
	}

	 
	for (j = 0; j < chip->cores_num; j++)
		sdev->dsp_core_ref_count[j] = 0;

	 
	hda_dsp_ctrl_ppcap_enable(sdev, false);
	hda_dsp_ctrl_ppcap_int_enable(sdev, false);
skip_dsp:

	 
	hda_dsp_ctrl_stop_chip(sdev);

	 
	snd_sof_pci_update_bits(sdev, PCI_PGCTL,
				PCI_PGCTL_LSRMD_MASK, PCI_PGCTL_LSRMD_MASK);

	 
	ret = hda_dsp_ctrl_link_reset(sdev, true);
	if (ret < 0) {
		dev_err(sdev->dev,
			"error: failed to reset controller during suspend\n");
		return ret;
	}

	 
	hda_codec_i915_display_power(sdev, false);

	return 0;
}

static int hda_resume(struct snd_sof_dev *sdev, bool runtime_resume)
{
	int ret;

	 
	hda_codec_i915_display_power(sdev, true);

	 
	snd_sof_pci_update_bits(sdev, PCI_TCSEL, 0x07, 0);

	 
	ret = hda_dsp_ctrl_init_chip(sdev);
	if (ret < 0) {
		dev_err(sdev->dev,
			"error: failed to start controller after resume\n");
		goto cleanup;
	}

	 
	if (runtime_resume) {
		hda_codec_jack_wake_enable(sdev, false);
		if (sdev->system_suspend_target == SOF_SUSPEND_NONE)
			hda_codec_jack_check(sdev);
	}

	if (!sdev->dspless_mode_selected) {
		 
		hda_dsp_ctrl_ppcap_enable(sdev, true);
		hda_dsp_ctrl_ppcap_int_enable(sdev, true);
	}

cleanup:
	 
	hda_codec_i915_display_power(sdev, false);

	return 0;
}

int hda_dsp_resume(struct snd_sof_dev *sdev)
{
	struct sof_intel_hda_dev *hda = sdev->pdata->hw_pdata;
	struct hdac_bus *bus = sof_to_bus(sdev);
	struct pci_dev *pci = to_pci_dev(sdev->dev);
	const struct sof_dsp_power_state target_state = {
		.state = SOF_DSP_PM_D0,
		.substate = SOF_HDA_DSP_PM_D0I0,
	};
	int ret;

	 
	if (sdev->dsp_power_state.state == SOF_DSP_PM_D0) {
		ret = hda_bus_ml_resume(bus);
		if (ret < 0) {
			dev_err(sdev->dev,
				"error %d in %s: failed to power up links",
				ret, __func__);
			return ret;
		}

		 
		hda_codec_resume_cmd_io(sdev);

		 
		ret = snd_sof_dsp_set_power_state(sdev, &target_state);
		if (ret < 0) {
			dev_err(sdev->dev, "error: setting dsp state %d substate %d\n",
				target_state.state, target_state.substate);
			return ret;
		}

		 
		if (hda->l1_disabled)
			snd_sof_dsp_update_bits(sdev, HDA_DSP_HDA_BAR,
						HDA_VS_INTEL_EM2,
						HDA_VS_INTEL_EM2_L1SEN, 0);

		 
		pci_restore_state(pci);
		disable_irq_wake(pci->irq);
		return 0;
	}

	 
	ret = hda_resume(sdev, false);
	if (ret < 0)
		return ret;

	return snd_sof_dsp_set_power_state(sdev, &target_state);
}

int hda_dsp_runtime_resume(struct snd_sof_dev *sdev)
{
	const struct sof_dsp_power_state target_state = {
		.state = SOF_DSP_PM_D0,
	};
	int ret;

	 
	ret = hda_resume(sdev, true);
	if (ret < 0)
		return ret;

	return snd_sof_dsp_set_power_state(sdev, &target_state);
}

int hda_dsp_runtime_idle(struct snd_sof_dev *sdev)
{
	struct hdac_bus *hbus = sof_to_bus(sdev);

	if (hbus->codec_powered) {
		dev_dbg(sdev->dev, "some codecs still powered (%08X), not idle\n",
			(unsigned int)hbus->codec_powered);
		return -EBUSY;
	}

	return 0;
}

int hda_dsp_runtime_suspend(struct snd_sof_dev *sdev)
{
	struct sof_intel_hda_dev *hda = sdev->pdata->hw_pdata;
	const struct sof_dsp_power_state target_state = {
		.state = SOF_DSP_PM_D3,
	};
	int ret;

	if (!sdev->dspless_mode_selected) {
		 
		cancel_delayed_work_sync(&hda->d0i3_work);
	}

	 
	ret = hda_suspend(sdev, true);
	if (ret < 0)
		return ret;

	return snd_sof_dsp_set_power_state(sdev, &target_state);
}

int hda_dsp_suspend(struct snd_sof_dev *sdev, u32 target_state)
{
	struct sof_intel_hda_dev *hda = sdev->pdata->hw_pdata;
	struct hdac_bus *bus = sof_to_bus(sdev);
	struct pci_dev *pci = to_pci_dev(sdev->dev);
	const struct sof_dsp_power_state target_dsp_state = {
		.state = target_state,
		.substate = target_state == SOF_DSP_PM_D0 ?
				SOF_HDA_DSP_PM_D0I3 : 0,
	};
	int ret;

	if (!sdev->dspless_mode_selected) {
		 
		cancel_delayed_work_sync(&hda->d0i3_work);
	}

	if (target_state == SOF_DSP_PM_D0) {
		 
		ret = snd_sof_dsp_set_power_state(sdev, &target_dsp_state);
		if (ret < 0) {
			dev_err(sdev->dev, "error: setting dsp state %d substate %d\n",
				target_dsp_state.state,
				target_dsp_state.substate);
			return ret;
		}

		 
		if (hda->l1_disabled)
			snd_sof_dsp_update_bits(sdev, HDA_DSP_HDA_BAR, HDA_VS_INTEL_EM2,
						HDA_VS_INTEL_EM2_L1SEN, HDA_VS_INTEL_EM2_L1SEN);

		 
		hda_codec_suspend_cmd_io(sdev);

		 
		ret = hda_bus_ml_suspend(bus);
		if (ret < 0) {
			dev_err(sdev->dev,
				"error %d in %s: failed to power down links",
				ret, __func__);
			return ret;
		}

		 
		enable_irq_wake(pci->irq);
		pci_save_state(pci);
		return 0;
	}

	 
	ret = hda_suspend(sdev, false);
	if (ret < 0) {
		dev_err(bus->dev, "error: suspending dsp\n");
		return ret;
	}

	return snd_sof_dsp_set_power_state(sdev, &target_dsp_state);
}

static unsigned int hda_dsp_check_for_dma_streams(struct snd_sof_dev *sdev)
{
	struct hdac_bus *bus = sof_to_bus(sdev);
	struct hdac_stream *s;
	unsigned int active_streams = 0;
	int sd_offset;
	u32 val;

	list_for_each_entry(s, &bus->stream_list, list) {
		sd_offset = SOF_STREAM_SD_OFFSET(s);
		val = snd_sof_dsp_read(sdev, HDA_DSP_HDA_BAR,
				       sd_offset);
		if (val & SOF_HDA_SD_CTL_DMA_START)
			active_streams |= BIT(s->index);
	}

	return active_streams;
}

static int hda_dsp_s5_quirk(struct snd_sof_dev *sdev)
{
	int ret;

	 
	usleep_range(500, 1000);

	 
	ret = hda_dsp_ctrl_link_reset(sdev, false);
	if (ret < 0)
		return ret;

	usleep_range(500, 1000);

	 
	ret = hda_dsp_ctrl_link_reset(sdev, true);
	if (ret < 0)
		return ret;

	return ret;
}

int hda_dsp_shutdown_dma_flush(struct snd_sof_dev *sdev)
{
	unsigned int active_streams;
	int ret, ret2;

	 
	active_streams = hda_dsp_check_for_dma_streams(sdev);

	sdev->system_suspend_target = SOF_SUSPEND_S3;
	ret = snd_sof_suspend(sdev->dev);

	if (active_streams) {
		dev_warn(sdev->dev,
			 "There were active DSP streams (%#x) at shutdown, trying to recover\n",
			 active_streams);
		ret2 = hda_dsp_s5_quirk(sdev);
		if (ret2 < 0)
			dev_err(sdev->dev, "shutdown recovery failed (%d)\n", ret2);
	}

	return ret;
}

int hda_dsp_shutdown(struct snd_sof_dev *sdev)
{
	sdev->system_suspend_target = SOF_SUSPEND_S3;
	return snd_sof_suspend(sdev->dev);
}

int hda_dsp_set_hw_params_upon_resume(struct snd_sof_dev *sdev)
{
	int ret;

	 
	ret = hda_dsp_dais_suspend(sdev);
	if (ret < 0)
		dev_warn(sdev->dev, "%s: failure in hda_dsp_dais_suspend\n", __func__);

	return ret;
}

void hda_dsp_d0i3_work(struct work_struct *work)
{
	struct sof_intel_hda_dev *hdev = container_of(work,
						      struct sof_intel_hda_dev,
						      d0i3_work.work);
	struct hdac_bus *bus = &hdev->hbus.core;
	struct snd_sof_dev *sdev = dev_get_drvdata(bus->dev);
	struct sof_dsp_power_state target_state = {
		.state = SOF_DSP_PM_D0,
		.substate = SOF_HDA_DSP_PM_D0I3,
	};
	int ret;

	 
	if (!snd_sof_dsp_only_d0i3_compatible_stream_active(sdev))
		 
		return;

	 
	ret = snd_sof_dsp_set_power_state(sdev, &target_state);
	if (ret < 0)
		dev_err_ratelimited(sdev->dev,
				    "error: failed to set DSP state %d substate %d\n",
				    target_state.state, target_state.substate);
}

int hda_dsp_core_get(struct snd_sof_dev *sdev, int core)
{
	const struct sof_ipc_pm_ops *pm_ops = sdev->ipc->ops->pm;
	int ret, ret1;

	 
	ret = hda_dsp_enable_core(sdev, BIT(core));
	if (ret < 0) {
		dev_err(sdev->dev, "failed to power up core %d with err: %d\n",
			core, ret);
		return ret;
	}

	 
	if (sdev->fw_state != SOF_FW_BOOT_COMPLETE || core == SOF_DSP_PRIMARY_CORE)
		return 0;

	 
	if (!pm_ops->set_core_state)
		return 0;

	 
	ret = pm_ops->set_core_state(sdev, core, true);
	if (ret < 0) {
		dev_err(sdev->dev, "failed to enable secondary core '%d' failed with %d\n",
			core, ret);
		goto power_down;
	}

	return ret;

power_down:
	 
	ret1 = hda_dsp_core_reset_power_down(sdev, BIT(core));
	if (ret1 < 0)
		dev_err(sdev->dev, "failed to power down core: %d with err: %d\n", core, ret1);

	return ret;
}

int hda_dsp_disable_interrupts(struct snd_sof_dev *sdev)
{
	hda_sdw_int_enable(sdev, false);
	hda_dsp_ipc_int_disable(sdev);

	return 0;
}
