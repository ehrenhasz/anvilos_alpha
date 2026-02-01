









#include "ops.h"
#include "sof-priv.h"
#include "sof-audio.h"

 
static u32 snd_sof_dsp_power_target(struct snd_sof_dev *sdev)
{
	u32 target_dsp_state;

	switch (sdev->system_suspend_target) {
	case SOF_SUSPEND_S5:
	case SOF_SUSPEND_S4:
		 
	case SOF_SUSPEND_S3:
		 
		target_dsp_state = SOF_DSP_PM_D3;
		break;
	case SOF_SUSPEND_S0IX:
		 
		if (snd_sof_stream_suspend_ignored(sdev))
			target_dsp_state = SOF_DSP_PM_D0;
		else
			target_dsp_state = SOF_DSP_PM_D3;
		break;
	default:
		 
		target_dsp_state = SOF_DSP_PM_D3;
		break;
	}

	return target_dsp_state;
}

#if IS_ENABLED(CONFIG_SND_SOC_SOF_DEBUG_ENABLE_DEBUGFS_CACHE)
static void sof_cache_debugfs(struct snd_sof_dev *sdev)
{
	struct snd_sof_dfsentry *dfse;

	list_for_each_entry(dfse, &sdev->dfsentry_list, list) {

		 
		if (dfse->type == SOF_DFSENTRY_TYPE_BUF)
			continue;

		 
		if (dfse->access_type == SOF_DEBUGFS_ACCESS_D0_ONLY)
			memcpy_fromio(dfse->cache_buf, dfse->io_mem,
				      dfse->size);
	}
}
#endif

static int sof_resume(struct device *dev, bool runtime_resume)
{
	struct snd_sof_dev *sdev = dev_get_drvdata(dev);
	const struct sof_ipc_pm_ops *pm_ops = sof_ipc_get_ops(sdev, pm);
	const struct sof_ipc_tplg_ops *tplg_ops = sof_ipc_get_ops(sdev, tplg);
	u32 old_state = sdev->dsp_power_state.state;
	int ret;

	 
	if (!runtime_resume && !sof_ops(sdev)->resume)
		return 0;

	if (runtime_resume && !sof_ops(sdev)->runtime_resume)
		return 0;

	 
	if (sdev->first_boot)
		return 0;

	 
	if (runtime_resume)
		ret = snd_sof_dsp_runtime_resume(sdev);
	else
		ret = snd_sof_dsp_resume(sdev);
	if (ret < 0) {
		dev_err(sdev->dev,
			"error: failed to power up DSP after resume\n");
		return ret;
	}

	if (sdev->dspless_mode_selected) {
		sof_set_fw_state(sdev, SOF_DSPLESS_MODE);
		return 0;
	}

	 
	if (!runtime_resume && sof_ops(sdev)->set_power_state &&
	    old_state == SOF_DSP_PM_D0) {
		ret = sof_fw_trace_resume(sdev);
		if (ret < 0)
			 
			dev_warn(sdev->dev,
				 "failed to enable trace after resume %d\n", ret);
		return 0;
	}

	sof_set_fw_state(sdev, SOF_FW_BOOT_PREPARE);

	 
	ret = snd_sof_load_firmware(sdev);
	if (ret < 0) {
		dev_err(sdev->dev,
			"error: failed to load DSP firmware after resume %d\n",
			ret);
		sof_set_fw_state(sdev, SOF_FW_BOOT_FAILED);
		return ret;
	}

	sof_set_fw_state(sdev, SOF_FW_BOOT_IN_PROGRESS);

	 
	ret = snd_sof_run_firmware(sdev);
	if (ret < 0) {
		dev_err(sdev->dev,
			"error: failed to boot DSP firmware after resume %d\n",
			ret);
		sof_set_fw_state(sdev, SOF_FW_BOOT_FAILED);
		return ret;
	}

	 
	ret = sof_fw_trace_resume(sdev);
	if (ret < 0) {
		 
		dev_warn(sdev->dev,
			 "warning: failed to init trace after resume %d\n",
			 ret);
	}

	 
	if (tplg_ops && tplg_ops->set_up_all_pipelines) {
		ret = tplg_ops->set_up_all_pipelines(sdev, false);
		if (ret < 0) {
			dev_err(sdev->dev, "Failed to restore pipeline after resume %d\n", ret);
			goto setup_fail;
		}
	}

	 
	sof_resume_clients(sdev);

	 
	if (pm_ops && pm_ops->ctx_restore) {
		ret = pm_ops->ctx_restore(sdev);
		if (ret < 0)
			dev_err(sdev->dev, "ctx_restore IPC error during resume: %d\n", ret);
	}

setup_fail:
#if IS_ENABLED(CONFIG_SND_SOC_SOF_DEBUG_ENABLE_DEBUGFS_CACHE)
	if (ret < 0) {
		 
		sof_cache_debugfs(sdev);
	}
#endif

	return ret;
}

static int sof_suspend(struct device *dev, bool runtime_suspend)
{
	struct snd_sof_dev *sdev = dev_get_drvdata(dev);
	const struct sof_ipc_pm_ops *pm_ops = sof_ipc_get_ops(sdev, pm);
	const struct sof_ipc_tplg_ops *tplg_ops = sof_ipc_get_ops(sdev, tplg);
	pm_message_t pm_state;
	u32 target_state = snd_sof_dsp_power_target(sdev);
	u32 old_state = sdev->dsp_power_state.state;
	int ret;

	 
	if (!runtime_suspend && !sof_ops(sdev)->suspend)
		return 0;

	if (runtime_suspend && !sof_ops(sdev)->runtime_suspend)
		return 0;

	 
	if (tplg_ops && tplg_ops->tear_down_all_pipelines && (old_state == SOF_DSP_PM_D0))
		tplg_ops->tear_down_all_pipelines(sdev, false);

	if (sdev->fw_state != SOF_FW_BOOT_COMPLETE)
		goto suspend;

	 
	if (!runtime_suspend) {
		ret = snd_sof_dsp_hw_params_upon_resume(sdev);
		if (ret < 0) {
			dev_err(sdev->dev,
				"error: setting hw_params flag during suspend %d\n",
				ret);
			return ret;
		}
	}

	pm_state.event = target_state;

	 
	sof_fw_trace_suspend(sdev, pm_state);

	 
	sof_suspend_clients(sdev, pm_state);

	 
	if (target_state == SOF_DSP_PM_D0)
		goto suspend;

#if IS_ENABLED(CONFIG_SND_SOC_SOF_DEBUG_ENABLE_DEBUGFS_CACHE)
	 
	if (runtime_suspend)
		sof_cache_debugfs(sdev);
#endif
	 
	if (pm_ops && pm_ops->ctx_save) {
		ret = pm_ops->ctx_save(sdev);
		if (ret == -EBUSY || ret == -EAGAIN) {
			 
			dev_err(sdev->dev, "ctx_save IPC error during suspend: %d\n", ret);
			return ret;
		} else if (ret < 0) {
			 
			dev_warn(sdev->dev, "ctx_save IPC error: %d, proceeding with suspend\n",
				 ret);
		}
	}

suspend:

	 
	if (sdev->fw_state == SOF_FW_BOOT_NOT_STARTED)
		return 0;

	 
	if (runtime_suspend)
		ret = snd_sof_dsp_runtime_suspend(sdev);
	else
		ret = snd_sof_dsp_suspend(sdev, target_state);
	if (ret < 0)
		dev_err(sdev->dev,
			"error: failed to power down DSP during suspend %d\n",
			ret);

	 
	if (target_state == SOF_DSP_PM_D0)
		return ret;

	 
	sof_set_fw_state(sdev, SOF_FW_BOOT_NOT_STARTED);
	sdev->enabled_cores_mask = 0;

	return ret;
}

int snd_sof_dsp_power_down_notify(struct snd_sof_dev *sdev)
{
	const struct sof_ipc_pm_ops *pm_ops = sof_ipc_get_ops(sdev, pm);

	 
	if (sof_ops(sdev)->remove && pm_ops && pm_ops->ctx_save)
		return pm_ops->ctx_save(sdev);

	return 0;
}

int snd_sof_runtime_suspend(struct device *dev)
{
	return sof_suspend(dev, true);
}
EXPORT_SYMBOL(snd_sof_runtime_suspend);

int snd_sof_runtime_idle(struct device *dev)
{
	struct snd_sof_dev *sdev = dev_get_drvdata(dev);

	return snd_sof_dsp_runtime_idle(sdev);
}
EXPORT_SYMBOL(snd_sof_runtime_idle);

int snd_sof_runtime_resume(struct device *dev)
{
	return sof_resume(dev, true);
}
EXPORT_SYMBOL(snd_sof_runtime_resume);

int snd_sof_resume(struct device *dev)
{
	return sof_resume(dev, false);
}
EXPORT_SYMBOL(snd_sof_resume);

int snd_sof_suspend(struct device *dev)
{
	return sof_suspend(dev, false);
}
EXPORT_SYMBOL(snd_sof_suspend);

int snd_sof_prepare(struct device *dev)
{
	struct snd_sof_dev *sdev = dev_get_drvdata(dev);
	const struct sof_dev_desc *desc = sdev->pdata->desc;

	 
	sdev->system_suspend_target = SOF_SUSPEND_S3;

	 
	if (sdev->fw_state == SOF_FW_CRASHED ||
	    sdev->fw_state == SOF_FW_BOOT_FAILED)
		return 0;

	if (!desc->use_acpi_target_states)
		return 0;

#if defined(CONFIG_ACPI)
	switch (acpi_target_system_state()) {
	case ACPI_STATE_S0:
		sdev->system_suspend_target = SOF_SUSPEND_S0IX;
		break;
	case ACPI_STATE_S1:
	case ACPI_STATE_S2:
	case ACPI_STATE_S3:
		sdev->system_suspend_target = SOF_SUSPEND_S3;
		break;
	case ACPI_STATE_S4:
		sdev->system_suspend_target = SOF_SUSPEND_S4;
		break;
	case ACPI_STATE_S5:
		sdev->system_suspend_target = SOF_SUSPEND_S5;
		break;
	default:
		break;
	}
#endif

	return 0;
}
EXPORT_SYMBOL(snd_sof_prepare);

void snd_sof_complete(struct device *dev)
{
	struct snd_sof_dev *sdev = dev_get_drvdata(dev);

	sdev->system_suspend_target = SOF_SUSPEND_NONE;
}
EXPORT_SYMBOL(snd_sof_complete);
