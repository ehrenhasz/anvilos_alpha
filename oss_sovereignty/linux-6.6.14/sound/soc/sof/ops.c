









#include <linux/pci.h>
#include "ops.h"

static
bool snd_sof_pci_update_bits_unlocked(struct snd_sof_dev *sdev, u32 offset,
				      u32 mask, u32 value)
{
	struct pci_dev *pci = to_pci_dev(sdev->dev);
	unsigned int old, new;
	u32 ret = 0;

	pci_read_config_dword(pci, offset, &ret);
	old = ret;
	dev_dbg(sdev->dev, "Debug PCIR: %8.8x at  %8.8x\n", old & mask, offset);

	new = (old & ~mask) | (value & mask);

	if (old == new)
		return false;

	pci_write_config_dword(pci, offset, new);
	dev_dbg(sdev->dev, "Debug PCIW: %8.8x at  %8.8x\n", value,
		offset);

	return true;
}

bool snd_sof_pci_update_bits(struct snd_sof_dev *sdev, u32 offset,
			     u32 mask, u32 value)
{
	unsigned long flags;
	bool change;

	spin_lock_irqsave(&sdev->hw_lock, flags);
	change = snd_sof_pci_update_bits_unlocked(sdev, offset, mask, value);
	spin_unlock_irqrestore(&sdev->hw_lock, flags);
	return change;
}
EXPORT_SYMBOL(snd_sof_pci_update_bits);

bool snd_sof_dsp_update_bits_unlocked(struct snd_sof_dev *sdev, u32 bar,
				      u32 offset, u32 mask, u32 value)
{
	unsigned int old, new;
	u32 ret;

	ret = snd_sof_dsp_read(sdev, bar, offset);

	old = ret;
	new = (old & ~mask) | (value & mask);

	if (old == new)
		return false;

	snd_sof_dsp_write(sdev, bar, offset, new);

	return true;
}
EXPORT_SYMBOL(snd_sof_dsp_update_bits_unlocked);

bool snd_sof_dsp_update_bits64_unlocked(struct snd_sof_dev *sdev, u32 bar,
					u32 offset, u64 mask, u64 value)
{
	u64 old, new;

	old = snd_sof_dsp_read64(sdev, bar, offset);

	new = (old & ~mask) | (value & mask);

	if (old == new)
		return false;

	snd_sof_dsp_write64(sdev, bar, offset, new);

	return true;
}
EXPORT_SYMBOL(snd_sof_dsp_update_bits64_unlocked);

 
bool snd_sof_dsp_update_bits(struct snd_sof_dev *sdev, u32 bar, u32 offset,
			     u32 mask, u32 value)
{
	unsigned long flags;
	bool change;

	spin_lock_irqsave(&sdev->hw_lock, flags);
	change = snd_sof_dsp_update_bits_unlocked(sdev, bar, offset, mask,
						  value);
	spin_unlock_irqrestore(&sdev->hw_lock, flags);
	return change;
}
EXPORT_SYMBOL(snd_sof_dsp_update_bits);

bool snd_sof_dsp_update_bits64(struct snd_sof_dev *sdev, u32 bar, u32 offset,
			       u64 mask, u64 value)
{
	unsigned long flags;
	bool change;

	spin_lock_irqsave(&sdev->hw_lock, flags);
	change = snd_sof_dsp_update_bits64_unlocked(sdev, bar, offset, mask,
						    value);
	spin_unlock_irqrestore(&sdev->hw_lock, flags);
	return change;
}
EXPORT_SYMBOL(snd_sof_dsp_update_bits64);

static
void snd_sof_dsp_update_bits_forced_unlocked(struct snd_sof_dev *sdev, u32 bar,
					     u32 offset, u32 mask, u32 value)
{
	unsigned int old, new;
	u32 ret;

	ret = snd_sof_dsp_read(sdev, bar, offset);

	old = ret;
	new = (old & ~mask) | (value & mask);

	snd_sof_dsp_write(sdev, bar, offset, new);
}

 
void snd_sof_dsp_update_bits_forced(struct snd_sof_dev *sdev, u32 bar,
				    u32 offset, u32 mask, u32 value)
{
	unsigned long flags;

	spin_lock_irqsave(&sdev->hw_lock, flags);
	snd_sof_dsp_update_bits_forced_unlocked(sdev, bar, offset, mask, value);
	spin_unlock_irqrestore(&sdev->hw_lock, flags);
}
EXPORT_SYMBOL(snd_sof_dsp_update_bits_forced);

 
void snd_sof_dsp_panic(struct snd_sof_dev *sdev, u32 offset, bool non_recoverable)
{
	 
	if (!sdev->dsp_oops_offset)
		sdev->dsp_oops_offset = offset;

	 
	if (sdev->dsp_oops_offset != offset)
		dev_warn(sdev->dev,
			 "%s: dsp_oops_offset %zu differs from panic offset %u\n",
			 __func__, sdev->dsp_oops_offset, offset);

	 
	sdev->dbg_dump_printed = false;
	if (non_recoverable) {
		snd_sof_dsp_dbg_dump(sdev, "DSP panic!",
				     SOF_DBG_DUMP_REGS | SOF_DBG_DUMP_MBOX);
		sof_set_fw_state(sdev, SOF_FW_CRASHED);
		sof_fw_trace_fw_crashed(sdev);
	} else {
		snd_sof_dsp_dbg_dump(sdev,
				     "DSP panic (recovery will be attempted)",
				     SOF_DBG_DUMP_REGS | SOF_DBG_DUMP_MBOX);
	}
}
EXPORT_SYMBOL(snd_sof_dsp_panic);
