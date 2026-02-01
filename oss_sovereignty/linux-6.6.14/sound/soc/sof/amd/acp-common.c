









 

#include "../sof-priv.h"
#include "../sof-audio.h"
#include "../ops.h"
#include "../sof-audio.h"
#include "acp.h"
#include "acp-dsp-offset.h"
#include <sound/sof/xtensa.h>

 
void amd_sof_ipc_dump(struct snd_sof_dev *sdev)
{
	const struct sof_amd_acp_desc *desc = get_chip_info(sdev->pdata);
	u32 base = desc->dsp_intr_base;
	u32 dsp_msg_write = sdev->debug_box.offset +
			    offsetof(struct scratch_ipc_conf, sof_dsp_msg_write);
	u32 dsp_ack_write = sdev->debug_box.offset +
			    offsetof(struct scratch_ipc_conf, sof_dsp_ack_write);
	u32 host_msg_write = sdev->debug_box.offset +
			     offsetof(struct scratch_ipc_conf, sof_host_msg_write);
	u32 host_ack_write = sdev->debug_box.offset +
			     offsetof(struct scratch_ipc_conf, sof_host_ack_write);
	u32 dsp_msg, dsp_ack, host_msg, host_ack, irq_stat;

	dsp_msg = snd_sof_dsp_read(sdev, ACP_DSP_BAR, ACP_SCRATCH_REG_0 + dsp_msg_write);
	dsp_ack = snd_sof_dsp_read(sdev, ACP_DSP_BAR, ACP_SCRATCH_REG_0 + dsp_ack_write);
	host_msg = snd_sof_dsp_read(sdev, ACP_DSP_BAR, ACP_SCRATCH_REG_0 + host_msg_write);
	host_ack = snd_sof_dsp_read(sdev, ACP_DSP_BAR, ACP_SCRATCH_REG_0 + host_ack_write);
	irq_stat = snd_sof_dsp_read(sdev, ACP_DSP_BAR, base + DSP_SW_INTR_STAT_OFFSET);

	dev_err(sdev->dev,
		"dsp_msg = %#x dsp_ack = %#x host_msg = %#x host_ack = %#x irq_stat = %#x\n",
		dsp_msg, dsp_ack, host_msg, host_ack, irq_stat);
}

 
static void amd_get_registers(struct snd_sof_dev *sdev,
			      struct sof_ipc_dsp_oops_xtensa *xoops,
			      struct sof_ipc_panic_info *panic_info,
			      u32 *stack, size_t stack_words)
{
	u32 offset = sdev->dsp_oops_offset;

	 
	acp_mailbox_read(sdev, offset, xoops, sizeof(*xoops));

	 
	if (xoops->arch_hdr.totalsize > EXCEPT_MAX_HDR_SIZE) {
		dev_err(sdev->dev, "invalid header size 0x%x. FW oops is bogus\n",
			xoops->arch_hdr.totalsize);
		return;
	}

	offset += xoops->arch_hdr.totalsize;
	acp_mailbox_read(sdev, offset, panic_info, sizeof(*panic_info));

	 
	offset += sizeof(*panic_info);
	acp_mailbox_read(sdev, offset, stack, stack_words * sizeof(u32));
}

 
void amd_sof_dump(struct snd_sof_dev *sdev, u32 flags)
{
	struct sof_ipc_dsp_oops_xtensa xoops;
	struct sof_ipc_panic_info panic_info;
	u32 stack[AMD_STACK_DUMP_SIZE];
	u32 status;

	 
	if (sdev->dsp_oops_offset > sdev->debug_box.offset) {
		acp_mailbox_read(sdev, sdev->debug_box.offset, &status, sizeof(u32));
	} else {
		 
		acp_mailbox_read(sdev, sdev->dsp_box.offset, &status, sizeof(u32));
		sdev->dsp_oops_offset = sdev->dsp_box.offset + sizeof(status);
	}

	 
	amd_get_registers(sdev, &xoops, &panic_info, stack, AMD_STACK_DUMP_SIZE);

	 
	sof_print_oops_and_stack(sdev, KERN_ERR, status, status, &xoops,
				 &panic_info, stack, AMD_STACK_DUMP_SIZE);
}

struct snd_soc_acpi_mach *amd_sof_machine_select(struct snd_sof_dev *sdev)
{
	struct snd_sof_pdata *sof_pdata = sdev->pdata;
	const struct sof_dev_desc *desc = sof_pdata->desc;
	struct snd_soc_acpi_mach *mach;

	mach = snd_soc_acpi_find_machine(desc->machines);
	if (!mach) {
		dev_warn(sdev->dev, "No matching ASoC machine driver found\n");
		return NULL;
	}

	sof_pdata->tplg_filename = mach->sof_tplg_filename;
	sof_pdata->fw_filename = mach->fw_filename;

	return mach;
}

 
struct snd_sof_dsp_ops sof_acp_common_ops = {
	 
	.probe			= amd_sof_acp_probe,
	.remove			= amd_sof_acp_remove,

	 
	.write			= sof_io_write,
	.read			= sof_io_read,

	 
	.block_read		= acp_dsp_block_read,
	.block_write		= acp_dsp_block_write,

	 
	.load_firmware		= snd_sof_load_firmware_memcpy,
	.pre_fw_run		= acp_dsp_pre_fw_run,
	.get_bar_index		= acp_get_bar_index,

	 
	.run			= acp_sof_dsp_run,

	 
	.send_msg		= acp_sof_ipc_send_msg,
	.ipc_msg_data		= acp_sof_ipc_msg_data,
	.set_stream_data_offset = acp_set_stream_data_offset,
	.get_mailbox_offset	= acp_sof_ipc_get_mailbox_offset,
	.get_window_offset      = acp_sof_ipc_get_window_offset,
	.irq_thread		= acp_sof_ipc_irq_thread,

	 
	.pcm_open		= acp_pcm_open,
	.pcm_close		= acp_pcm_close,
	.pcm_hw_params		= acp_pcm_hw_params,
	.pcm_pointer		= acp_pcm_pointer,

	.hw_info		= SNDRV_PCM_INFO_MMAP |
				  SNDRV_PCM_INFO_MMAP_VALID |
				  SNDRV_PCM_INFO_INTERLEAVED |
				  SNDRV_PCM_INFO_PAUSE |
				  SNDRV_PCM_INFO_NO_PERIOD_WAKEUP,

	 
	.machine_select		= amd_sof_machine_select,
	.machine_register	= sof_machine_register,
	.machine_unregister	= sof_machine_unregister,

	 
	.trace_init		= acp_sof_trace_init,
	.trace_release		= acp_sof_trace_release,

	 
	.suspend                = amd_sof_acp_suspend,
	.resume                 = amd_sof_acp_resume,

	.ipc_dump		= amd_sof_ipc_dump,
	.dbg_dump		= amd_sof_dump,
	.debugfs_add_region_item = snd_sof_debugfs_add_region_item_iomem,
	.dsp_arch_ops = &sof_xtensa_arch_ops,

	 
	.register_ipc_clients = acp_probes_register,
	.unregister_ipc_clients = acp_probes_unregister,
};
EXPORT_SYMBOL_NS(sof_acp_common_ops, SND_SOC_SOF_AMD_COMMON);

MODULE_IMPORT_NS(SND_SOC_SOF_AMD_COMMON);
MODULE_IMPORT_NS(SND_SOC_SOF_XTENSA);
MODULE_DESCRIPTION("ACP SOF COMMON Driver");
MODULE_LICENSE("Dual BSD/GPL");
