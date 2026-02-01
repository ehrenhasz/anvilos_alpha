









#include <linux/firmware.h>
#include <linux/module.h>
#include <sound/soc.h>
#include <sound/sof.h>
#include "sof-priv.h"
#include "ops.h"

#define CREATE_TRACE_POINTS
#include <trace/events/sof.h>

 
static int sof_core_debug =  IS_ENABLED(CONFIG_SND_SOC_SOF_DEBUG_ENABLE_FIRMWARE_TRACE);
module_param_named(sof_debug, sof_core_debug, int, 0444);
MODULE_PARM_DESC(sof_debug, "SOF core debug options (0x0 all off)");

 
#define TIMEOUT_DEFAULT_IPC_MS  500
#define TIMEOUT_DEFAULT_BOOT_MS 2000

 
bool sof_debug_check_flag(int mask)
{
	if ((sof_core_debug & mask) == mask)
		return true;

	return false;
}
EXPORT_SYMBOL(sof_debug_check_flag);

 

struct sof_panic_msg {
	u32 id;
	const char *msg;
};

 
static const struct sof_panic_msg panic_msg[] = {
	{SOF_IPC_PANIC_MEM, "out of memory"},
	{SOF_IPC_PANIC_WORK, "work subsystem init failed"},
	{SOF_IPC_PANIC_IPC, "IPC subsystem init failed"},
	{SOF_IPC_PANIC_ARCH, "arch init failed"},
	{SOF_IPC_PANIC_PLATFORM, "platform init failed"},
	{SOF_IPC_PANIC_TASK, "scheduler init failed"},
	{SOF_IPC_PANIC_EXCEPTION, "runtime exception"},
	{SOF_IPC_PANIC_DEADLOCK, "deadlock"},
	{SOF_IPC_PANIC_STACK, "stack overflow"},
	{SOF_IPC_PANIC_IDLE, "can't enter idle"},
	{SOF_IPC_PANIC_WFI, "invalid wait state"},
	{SOF_IPC_PANIC_ASSERT, "assertion failed"},
};

 
void sof_print_oops_and_stack(struct snd_sof_dev *sdev, const char *level,
			      u32 panic_code, u32 tracep_code, void *oops,
			      struct sof_ipc_panic_info *panic_info,
			      void *stack, size_t stack_words)
{
	u32 code;
	int i;

	 
	if ((panic_code & SOF_IPC_PANIC_MAGIC_MASK) != SOF_IPC_PANIC_MAGIC) {
		dev_printk(level, sdev->dev, "unexpected fault %#010x trace %#010x\n",
			   panic_code, tracep_code);
		return;  
	}

	code = panic_code & (SOF_IPC_PANIC_MAGIC_MASK | SOF_IPC_PANIC_CODE_MASK);

	for (i = 0; i < ARRAY_SIZE(panic_msg); i++) {
		if (panic_msg[i].id == code) {
			dev_printk(level, sdev->dev, "reason: %s (%#x)\n",
				   panic_msg[i].msg, code & SOF_IPC_PANIC_CODE_MASK);
			dev_printk(level, sdev->dev, "trace point: %#010x\n", tracep_code);
			goto out;
		}
	}

	 
	dev_printk(level, sdev->dev, "unknown panic code: %#x\n",
		   code & SOF_IPC_PANIC_CODE_MASK);
	dev_printk(level, sdev->dev, "trace point: %#010x\n", tracep_code);

out:
	dev_printk(level, sdev->dev, "panic at %s:%d\n", panic_info->filename,
		   panic_info->linenum);
	sof_oops(sdev, level, oops);
	sof_stack(sdev, level, oops, stack, stack_words);
}
EXPORT_SYMBOL(sof_print_oops_and_stack);

 
void sof_set_fw_state(struct snd_sof_dev *sdev, enum sof_fw_state new_state)
{
	if (sdev->fw_state == new_state)
		return;

	dev_dbg(sdev->dev, "fw_state change: %d -> %d\n", sdev->fw_state, new_state);
	sdev->fw_state = new_state;

	switch (new_state) {
	case SOF_FW_BOOT_NOT_STARTED:
	case SOF_FW_BOOT_COMPLETE:
	case SOF_FW_CRASHED:
		sof_client_fw_state_dispatcher(sdev);
		fallthrough;
	default:
		break;
	}
}
EXPORT_SYMBOL(sof_set_fw_state);

 

static int sof_probe_continue(struct snd_sof_dev *sdev)
{
	struct snd_sof_pdata *plat_data = sdev->pdata;
	int ret;

	 
	ret = snd_sof_probe(sdev);
	if (ret < 0) {
		dev_err(sdev->dev, "error: failed to probe DSP %d\n", ret);
		goto probe_err;
	}

	sof_set_fw_state(sdev, SOF_FW_BOOT_PREPARE);

	 
	ret = sof_machine_check(sdev);
	if (ret < 0) {
		dev_err(sdev->dev, "error: failed to get machine info %d\n",
			ret);
		goto dsp_err;
	}

	 
	snd_sof_new_platform_drv(sdev);

	if (sdev->dspless_mode_selected) {
		sof_set_fw_state(sdev, SOF_DSPLESS_MODE);
		goto skip_dsp_init;
	}

	 
	ret = snd_sof_dbg_init(sdev);
	if (ret < 0) {
		 
		dev_err(sdev->dev, "error: failed to init DSP trace/debug %d\n",
			ret);
		goto dbg_err;
	}

	 
	sdev->ipc = snd_sof_ipc_init(sdev);
	if (!sdev->ipc) {
		ret = -ENOMEM;
		dev_err(sdev->dev, "error: failed to init DSP IPC %d\n", ret);
		goto ipc_err;
	}

	 
	ret = snd_sof_load_firmware(sdev);
	if (ret < 0) {
		dev_err(sdev->dev, "error: failed to load DSP firmware %d\n",
			ret);
		sof_set_fw_state(sdev, SOF_FW_BOOT_FAILED);
		goto fw_load_err;
	}

	sof_set_fw_state(sdev, SOF_FW_BOOT_IN_PROGRESS);

	 
	ret = snd_sof_run_firmware(sdev);
	if (ret < 0) {
		dev_err(sdev->dev, "error: failed to boot DSP firmware %d\n",
			ret);
		sof_set_fw_state(sdev, SOF_FW_BOOT_FAILED);
		goto fw_run_err;
	}

	if (sof_debug_check_flag(SOF_DBG_ENABLE_TRACE)) {
		sdev->fw_trace_is_supported = true;

		 
		ret = sof_fw_trace_init(sdev);
		if (ret < 0) {
			 
			dev_warn(sdev->dev, "failed to initialize firmware tracing %d\n",
				 ret);
		}
	} else {
		dev_dbg(sdev->dev, "SOF firmware trace disabled\n");
	}

skip_dsp_init:
	 
	sdev->first_boot = false;

	 
	ret = devm_snd_soc_register_component(sdev->dev, &sdev->plat_drv,
					      sof_ops(sdev)->drv,
					      sof_ops(sdev)->num_drv);
	if (ret < 0) {
		dev_err(sdev->dev,
			"error: failed to register DSP DAI driver %d\n", ret);
		goto fw_trace_err;
	}

	ret = snd_sof_machine_register(sdev, plat_data);
	if (ret < 0) {
		dev_err(sdev->dev,
			"error: failed to register machine driver %d\n", ret);
		goto fw_trace_err;
	}

	ret = sof_register_clients(sdev);
	if (ret < 0) {
		dev_err(sdev->dev, "failed to register clients %d\n", ret);
		goto sof_machine_err;
	}

	 
	if (!sof_ops(sdev)->runtime_suspend || !sof_ops(sdev)->runtime_resume)
		pm_runtime_get_noresume(sdev->dev);

	if (plat_data->sof_probe_complete)
		plat_data->sof_probe_complete(sdev->dev);

	sdev->probe_completed = true;

	return 0;

sof_machine_err:
	snd_sof_machine_unregister(sdev, plat_data);
fw_trace_err:
	sof_fw_trace_free(sdev);
fw_run_err:
	snd_sof_fw_unload(sdev);
fw_load_err:
	snd_sof_ipc_free(sdev);
ipc_err:
dbg_err:
	snd_sof_free_debug(sdev);
dsp_err:
	snd_sof_remove(sdev);
probe_err:
	sof_ops_free(sdev);

	 
	sof_set_fw_state(sdev, SOF_FW_BOOT_NOT_STARTED);
	sdev->first_boot = true;

	return ret;
}

static void sof_probe_work(struct work_struct *work)
{
	struct snd_sof_dev *sdev =
		container_of(work, struct snd_sof_dev, probe_work);
	int ret;

	ret = sof_probe_continue(sdev);
	if (ret < 0) {
		 
		dev_err(sdev->dev, "error: %s failed err: %d\n", __func__, ret);
	}
}

int snd_sof_device_probe(struct device *dev, struct snd_sof_pdata *plat_data)
{
	struct snd_sof_dev *sdev;
	int ret;

	sdev = devm_kzalloc(dev, sizeof(*sdev), GFP_KERNEL);
	if (!sdev)
		return -ENOMEM;

	 
	sdev->dev = dev;

	 
	sdev->dsp_power_state.state = SOF_DSP_PM_D0;

	sdev->pdata = plat_data;
	sdev->first_boot = true;
	dev_set_drvdata(dev, sdev);

	if (sof_core_debug)
		dev_info(dev, "sof_debug value: %#x\n", sof_core_debug);

	if (sof_debug_check_flag(SOF_DBG_DSPLESS_MODE)) {
		if (plat_data->desc->dspless_mode_supported) {
			dev_info(dev, "Switching to DSPless mode\n");
			sdev->dspless_mode_selected = true;
		} else {
			dev_info(dev, "DSPless mode is not supported by the platform\n");
		}
	}

	 
	if (!(BIT(plat_data->ipc_type) & plat_data->desc->ipc_supported_mask)) {
		dev_err(dev, "ipc_type %d is not supported on this platform, mask is %#x\n",
			plat_data->ipc_type, plat_data->desc->ipc_supported_mask);
		return -EINVAL;
	}

	 
	ret = sof_ops_init(sdev);
	if (ret < 0)
		return ret;

	 
	if (!sof_ops(sdev) || !sof_ops(sdev)->probe) {
		sof_ops_free(sdev);
		dev_err(dev, "missing mandatory ops\n");
		return -EINVAL;
	}

	if (!sdev->dspless_mode_selected &&
	    (!sof_ops(sdev)->run || !sof_ops(sdev)->block_read ||
	     !sof_ops(sdev)->block_write || !sof_ops(sdev)->send_msg ||
	     !sof_ops(sdev)->load_firmware || !sof_ops(sdev)->ipc_msg_data)) {
		sof_ops_free(sdev);
		dev_err(dev, "missing mandatory DSP ops\n");
		return -EINVAL;
	}

	INIT_LIST_HEAD(&sdev->pcm_list);
	INIT_LIST_HEAD(&sdev->kcontrol_list);
	INIT_LIST_HEAD(&sdev->widget_list);
	INIT_LIST_HEAD(&sdev->pipeline_list);
	INIT_LIST_HEAD(&sdev->dai_list);
	INIT_LIST_HEAD(&sdev->dai_link_list);
	INIT_LIST_HEAD(&sdev->route_list);
	INIT_LIST_HEAD(&sdev->ipc_client_list);
	INIT_LIST_HEAD(&sdev->ipc_rx_handler_list);
	INIT_LIST_HEAD(&sdev->fw_state_handler_list);
	spin_lock_init(&sdev->ipc_lock);
	spin_lock_init(&sdev->hw_lock);
	mutex_init(&sdev->power_state_access);
	mutex_init(&sdev->ipc_client_mutex);
	mutex_init(&sdev->client_event_handler_mutex);

	 
	if (plat_data->desc->ipc_timeout == 0)
		sdev->ipc_timeout = TIMEOUT_DEFAULT_IPC_MS;
	else
		sdev->ipc_timeout = plat_data->desc->ipc_timeout;
	if (plat_data->desc->boot_timeout == 0)
		sdev->boot_timeout = TIMEOUT_DEFAULT_BOOT_MS;
	else
		sdev->boot_timeout = plat_data->desc->boot_timeout;

	sof_set_fw_state(sdev, SOF_FW_BOOT_NOT_STARTED);

	if (IS_ENABLED(CONFIG_SND_SOC_SOF_PROBE_WORK_QUEUE)) {
		INIT_WORK(&sdev->probe_work, sof_probe_work);
		schedule_work(&sdev->probe_work);
		return 0;
	}

	return sof_probe_continue(sdev);
}
EXPORT_SYMBOL(snd_sof_device_probe);

bool snd_sof_device_probe_completed(struct device *dev)
{
	struct snd_sof_dev *sdev = dev_get_drvdata(dev);

	return sdev->probe_completed;
}
EXPORT_SYMBOL(snd_sof_device_probe_completed);

int snd_sof_device_remove(struct device *dev)
{
	struct snd_sof_dev *sdev = dev_get_drvdata(dev);
	struct snd_sof_pdata *pdata = sdev->pdata;
	int ret;
	bool aborted = false;

	if (IS_ENABLED(CONFIG_SND_SOC_SOF_PROBE_WORK_QUEUE))
		aborted = cancel_work_sync(&sdev->probe_work);

	 
	sof_unregister_clients(sdev);

	 
	snd_sof_machine_unregister(sdev, pdata);

	if (sdev->fw_state > SOF_FW_BOOT_NOT_STARTED) {
		sof_fw_trace_free(sdev);
		ret = snd_sof_dsp_power_down_notify(sdev);
		if (ret < 0)
			dev_warn(dev, "error: %d failed to prepare DSP for device removal",
				 ret);

		snd_sof_ipc_free(sdev);
		snd_sof_free_debug(sdev);
		snd_sof_remove(sdev);
		sof_ops_free(sdev);
	} else if (aborted) {
		 
		sof_ops_free(sdev);
	}

	 
	snd_sof_fw_unload(sdev);

	return 0;
}
EXPORT_SYMBOL(snd_sof_device_remove);

int snd_sof_device_shutdown(struct device *dev)
{
	struct snd_sof_dev *sdev = dev_get_drvdata(dev);

	if (IS_ENABLED(CONFIG_SND_SOC_SOF_PROBE_WORK_QUEUE))
		cancel_work_sync(&sdev->probe_work);

	if (sdev->fw_state == SOF_FW_BOOT_COMPLETE) {
		sof_fw_trace_free(sdev);
		return snd_sof_shutdown(sdev);
	}

	return 0;
}
EXPORT_SYMBOL(snd_sof_device_shutdown);

MODULE_AUTHOR("Liam Girdwood");
MODULE_DESCRIPTION("Sound Open Firmware (SOF) Core");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_ALIAS("platform:sof-audio");
MODULE_IMPORT_NS(SND_SOC_SOF_CLIENT);
