








 

#include <linux/module.h>
#include <sound/sof/xtensa.h>
#include "../ops.h"
#include "mtk-adsp-common.h"

 
static void mtk_adsp_get_registers(struct snd_sof_dev *sdev,
				   struct sof_ipc_dsp_oops_xtensa *xoops,
				   struct sof_ipc_panic_info *panic_info,
				   u32 *stack, size_t stack_words)
{
	u32 offset = sdev->dsp_oops_offset;

	 
	sof_mailbox_read(sdev, offset, xoops, sizeof(*xoops));

	 
	if (xoops->arch_hdr.totalsize > EXCEPT_MAX_HDR_SIZE) {
		dev_err(sdev->dev, "invalid header size 0x%x\n",
			xoops->arch_hdr.totalsize);
		return;
	}
	offset += xoops->arch_hdr.totalsize;
	sof_mailbox_read(sdev, offset, panic_info, sizeof(*panic_info));

	 
	offset += sizeof(*panic_info);
	sof_mailbox_read(sdev, offset, stack, stack_words * sizeof(u32));
}

 
void mtk_adsp_dump(struct snd_sof_dev *sdev, u32 flags)
{
	char *level = (flags & SOF_DBG_DUMP_OPTIONAL) ? KERN_DEBUG : KERN_ERR;
	struct sof_ipc_dsp_oops_xtensa xoops;
	struct sof_ipc_panic_info panic_info = {};
	u32 stack[MTK_ADSP_STACK_DUMP_SIZE];
	u32 status;

	 
	sof_mailbox_read(sdev, sdev->debug_box.offset + 0x4, &status, 4);

	 
	mtk_adsp_get_registers(sdev, &xoops, &panic_info, stack,
			       MTK_ADSP_STACK_DUMP_SIZE);

	 
	sof_print_oops_and_stack(sdev, level, status, status, &xoops, &panic_info,
				 stack, MTK_ADSP_STACK_DUMP_SIZE);
}
EXPORT_SYMBOL(mtk_adsp_dump);

MODULE_LICENSE("Dual BSD/GPL");
