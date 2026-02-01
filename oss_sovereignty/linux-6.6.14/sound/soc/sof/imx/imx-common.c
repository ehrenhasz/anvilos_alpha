





#include <linux/module.h>
#include <sound/sof/xtensa.h>
#include "../ops.h"

#include "imx-common.h"

 
void imx8_get_registers(struct snd_sof_dev *sdev,
			struct sof_ipc_dsp_oops_xtensa *xoops,
			struct sof_ipc_panic_info *panic_info,
			u32 *stack, size_t stack_words)
{
	u32 offset = sdev->dsp_oops_offset;

	 
	sof_mailbox_read(sdev, offset, xoops, sizeof(*xoops));

	 
	if (xoops->arch_hdr.totalsize > EXCEPT_MAX_HDR_SIZE) {
		dev_err(sdev->dev, "invalid header size 0x%x. FW oops is bogus\n",
			xoops->arch_hdr.totalsize);
		return;
	}
	offset += xoops->arch_hdr.totalsize;
	sof_mailbox_read(sdev, offset, panic_info, sizeof(*panic_info));

	 
	offset += sizeof(*panic_info);
	sof_mailbox_read(sdev, offset, stack, stack_words * sizeof(u32));
}

 
void imx8_dump(struct snd_sof_dev *sdev, u32 flags)
{
	struct sof_ipc_dsp_oops_xtensa xoops;
	struct sof_ipc_panic_info panic_info;
	u32 stack[IMX8_STACK_DUMP_SIZE];
	u32 status;

	 
	sof_mailbox_read(sdev, sdev->debug_box.offset + 0x4, &status, 4);

	 
	imx8_get_registers(sdev, &xoops, &panic_info, stack,
			   IMX8_STACK_DUMP_SIZE);

	 
	sof_print_oops_and_stack(sdev, KERN_ERR, status, status, &xoops,
				 &panic_info, stack, IMX8_STACK_DUMP_SIZE);
}
EXPORT_SYMBOL(imx8_dump);

int imx8_parse_clocks(struct snd_sof_dev *sdev, struct imx_clocks *clks)
{
	int ret;

	ret = devm_clk_bulk_get(sdev->dev, clks->num_dsp_clks, clks->dsp_clks);
	if (ret)
		dev_err(sdev->dev, "Failed to request DSP clocks\n");

	return ret;
}
EXPORT_SYMBOL(imx8_parse_clocks);

int imx8_enable_clocks(struct snd_sof_dev *sdev, struct imx_clocks *clks)
{
	return clk_bulk_prepare_enable(clks->num_dsp_clks, clks->dsp_clks);
}
EXPORT_SYMBOL(imx8_enable_clocks);

void imx8_disable_clocks(struct snd_sof_dev *sdev, struct imx_clocks *clks)
{
	clk_bulk_disable_unprepare(clks->num_dsp_clks, clks->dsp_clks);
}
EXPORT_SYMBOL(imx8_disable_clocks);

MODULE_LICENSE("Dual BSD/GPL");
