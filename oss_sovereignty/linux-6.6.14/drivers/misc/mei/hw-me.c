
 

#include <linux/pci.h>

#include <linux/kthread.h>
#include <linux/interrupt.h>
#include <linux/pm_runtime.h>
#include <linux/sizes.h>
#include <linux/delay.h>

#include "mei_dev.h"
#include "hbm.h"

#include "hw-me.h"
#include "hw-me-regs.h"

#include "mei-trace.h"

 
static inline u32 mei_me_reg_read(const struct mei_me_hw *hw,
			       unsigned long offset)
{
	return ioread32(hw->mem_addr + offset);
}


 
static inline void mei_me_reg_write(const struct mei_me_hw *hw,
				 unsigned long offset, u32 value)
{
	iowrite32(value, hw->mem_addr + offset);
}

 
static inline u32 mei_me_mecbrw_read(const struct mei_device *dev)
{
	return mei_me_reg_read(to_me_hw(dev), ME_CB_RW);
}

 
static inline void mei_me_hcbww_write(struct mei_device *dev, u32 data)
{
	mei_me_reg_write(to_me_hw(dev), H_CB_WW, data);
}

 
static inline u32 mei_me_mecsr_read(const struct mei_device *dev)
{
	u32 reg;

	reg = mei_me_reg_read(to_me_hw(dev), ME_CSR_HA);
	trace_mei_reg_read(dev->dev, "ME_CSR_HA", ME_CSR_HA, reg);

	return reg;
}

 
static inline u32 mei_hcsr_read(const struct mei_device *dev)
{
	u32 reg;

	reg = mei_me_reg_read(to_me_hw(dev), H_CSR);
	trace_mei_reg_read(dev->dev, "H_CSR", H_CSR, reg);

	return reg;
}

 
static inline void mei_hcsr_write(struct mei_device *dev, u32 reg)
{
	trace_mei_reg_write(dev->dev, "H_CSR", H_CSR, reg);
	mei_me_reg_write(to_me_hw(dev), H_CSR, reg);
}

 
static inline void mei_hcsr_set(struct mei_device *dev, u32 reg)
{
	reg &= ~H_CSR_IS_MASK;
	mei_hcsr_write(dev, reg);
}

 
static inline void mei_hcsr_set_hig(struct mei_device *dev)
{
	u32 hcsr;

	hcsr = mei_hcsr_read(dev) | H_IG;
	mei_hcsr_set(dev, hcsr);
}

 
static inline u32 mei_me_d0i3c_read(const struct mei_device *dev)
{
	u32 reg;

	reg = mei_me_reg_read(to_me_hw(dev), H_D0I3C);
	trace_mei_reg_read(dev->dev, "H_D0I3C", H_D0I3C, reg);

	return reg;
}

 
static inline void mei_me_d0i3c_write(struct mei_device *dev, u32 reg)
{
	trace_mei_reg_write(dev->dev, "H_D0I3C", H_D0I3C, reg);
	mei_me_reg_write(to_me_hw(dev), H_D0I3C, reg);
}

 
static int mei_me_trc_status(struct mei_device *dev, u32 *trc)
{
	struct mei_me_hw *hw = to_me_hw(dev);

	if (!hw->cfg->hw_trc_supported)
		return -EOPNOTSUPP;

	*trc = mei_me_reg_read(hw, ME_TRC);
	trace_mei_reg_read(dev->dev, "ME_TRC", ME_TRC, *trc);

	return 0;
}

 
static int mei_me_fw_status(struct mei_device *dev,
			    struct mei_fw_status *fw_status)
{
	struct mei_me_hw *hw = to_me_hw(dev);
	const struct mei_fw_status *fw_src = &hw->cfg->fw_status;
	int ret;
	int i;

	if (!fw_status || !hw->read_fws)
		return -EINVAL;

	fw_status->count = fw_src->count;
	for (i = 0; i < fw_src->count && i < MEI_FW_STATUS_MAX; i++) {
		ret = hw->read_fws(dev, fw_src->status[i],
				   &fw_status->status[i]);
		trace_mei_pci_cfg_read(dev->dev, "PCI_CFG_HFS_X",
				       fw_src->status[i],
				       fw_status->status[i]);
		if (ret)
			return ret;
	}

	return 0;
}

 
static int mei_me_hw_config(struct mei_device *dev)
{
	struct mei_me_hw *hw = to_me_hw(dev);
	u32 hcsr, reg;

	if (WARN_ON(!hw->read_fws))
		return -EINVAL;

	 
	hcsr = mei_hcsr_read(dev);
	hw->hbuf_depth = (hcsr & H_CBD) >> 24;

	reg = 0;
	hw->read_fws(dev, PCI_CFG_HFS_1, &reg);
	trace_mei_pci_cfg_read(dev->dev, "PCI_CFG_HFS_1", PCI_CFG_HFS_1, reg);
	hw->d0i3_supported =
		((reg & PCI_CFG_HFS_1_D0I3_MSK) == PCI_CFG_HFS_1_D0I3_MSK);

	hw->pg_state = MEI_PG_OFF;
	if (hw->d0i3_supported) {
		reg = mei_me_d0i3c_read(dev);
		if (reg & H_D0I3C_I3)
			hw->pg_state = MEI_PG_ON;
	}

	return 0;
}

 
static inline enum mei_pg_state mei_me_pg_state(struct mei_device *dev)
{
	struct mei_me_hw *hw = to_me_hw(dev);

	return hw->pg_state;
}

static inline u32 me_intr_src(u32 hcsr)
{
	return hcsr & H_CSR_IS_MASK;
}

 
static inline void me_intr_disable(struct mei_device *dev, u32 hcsr)
{
	hcsr &= ~H_CSR_IE_MASK;
	mei_hcsr_set(dev, hcsr);
}

 
static inline void me_intr_clear(struct mei_device *dev, u32 hcsr)
{
	if (me_intr_src(hcsr))
		mei_hcsr_write(dev, hcsr);
}

 
static void mei_me_intr_clear(struct mei_device *dev)
{
	u32 hcsr = mei_hcsr_read(dev);

	me_intr_clear(dev, hcsr);
}
 
static void mei_me_intr_enable(struct mei_device *dev)
{
	u32 hcsr;

	if (mei_me_hw_use_polling(to_me_hw(dev)))
		return;

	hcsr = mei_hcsr_read(dev) | H_CSR_IE_MASK;
	mei_hcsr_set(dev, hcsr);
}

 
static void mei_me_intr_disable(struct mei_device *dev)
{
	u32 hcsr = mei_hcsr_read(dev);

	me_intr_disable(dev, hcsr);
}

 
static void mei_me_synchronize_irq(struct mei_device *dev)
{
	struct mei_me_hw *hw = to_me_hw(dev);

	if (mei_me_hw_use_polling(hw))
		return;

	synchronize_irq(hw->irq);
}

 
static void mei_me_hw_reset_release(struct mei_device *dev)
{
	u32 hcsr = mei_hcsr_read(dev);

	hcsr |= H_IG;
	hcsr &= ~H_RST;
	mei_hcsr_set(dev, hcsr);
}

 
static void mei_me_host_set_ready(struct mei_device *dev)
{
	u32 hcsr = mei_hcsr_read(dev);

	if (!mei_me_hw_use_polling(to_me_hw(dev)))
		hcsr |= H_CSR_IE_MASK;

	hcsr |=  H_IG | H_RDY;
	mei_hcsr_set(dev, hcsr);
}

 
static bool mei_me_host_is_ready(struct mei_device *dev)
{
	u32 hcsr = mei_hcsr_read(dev);

	return (hcsr & H_RDY) == H_RDY;
}

 
static bool mei_me_hw_is_ready(struct mei_device *dev)
{
	u32 mecsr = mei_me_mecsr_read(dev);

	return (mecsr & ME_RDY_HRA) == ME_RDY_HRA;
}

 
static bool mei_me_hw_is_resetting(struct mei_device *dev)
{
	u32 mecsr = mei_me_mecsr_read(dev);

	return (mecsr & ME_RST_HRA) == ME_RST_HRA;
}

 
static void mei_gsc_pxp_check(struct mei_device *dev)
{
	struct mei_me_hw *hw = to_me_hw(dev);
	u32 fwsts5 = 0;

	if (dev->pxp_mode == MEI_DEV_PXP_DEFAULT)
		return;

	hw->read_fws(dev, PCI_CFG_HFS_5, &fwsts5);
	trace_mei_pci_cfg_read(dev->dev, "PCI_CFG_HFS_5", PCI_CFG_HFS_5, fwsts5);
	if ((fwsts5 & GSC_CFG_HFS_5_BOOT_TYPE_MSK) == GSC_CFG_HFS_5_BOOT_TYPE_PXP) {
		dev_dbg(dev->dev, "pxp mode is ready 0x%08x\n", fwsts5);
		dev->pxp_mode = MEI_DEV_PXP_READY;
	} else {
		dev_dbg(dev->dev, "pxp mode is not ready 0x%08x\n", fwsts5);
	}
}

 
static int mei_me_hw_ready_wait(struct mei_device *dev)
{
	mutex_unlock(&dev->device_lock);
	wait_event_timeout(dev->wait_hw_ready,
			dev->recvd_hw_ready,
			dev->timeouts.hw_ready);
	mutex_lock(&dev->device_lock);
	if (!dev->recvd_hw_ready) {
		dev_err(dev->dev, "wait hw ready failed\n");
		return -ETIME;
	}

	mei_gsc_pxp_check(dev);

	mei_me_hw_reset_release(dev);
	dev->recvd_hw_ready = false;
	return 0;
}

 
static int mei_me_hw_start(struct mei_device *dev)
{
	int ret = mei_me_hw_ready_wait(dev);

	if (ret)
		return ret;
	dev_dbg(dev->dev, "hw is ready\n");

	mei_me_host_set_ready(dev);
	return ret;
}


 
static unsigned char mei_hbuf_filled_slots(struct mei_device *dev)
{
	u32 hcsr;
	char read_ptr, write_ptr;

	hcsr = mei_hcsr_read(dev);

	read_ptr = (char) ((hcsr & H_CBRP) >> 8);
	write_ptr = (char) ((hcsr & H_CBWP) >> 16);

	return (unsigned char) (write_ptr - read_ptr);
}

 
static bool mei_me_hbuf_is_empty(struct mei_device *dev)
{
	return mei_hbuf_filled_slots(dev) == 0;
}

 
static int mei_me_hbuf_empty_slots(struct mei_device *dev)
{
	struct mei_me_hw *hw = to_me_hw(dev);
	unsigned char filled_slots, empty_slots;

	filled_slots = mei_hbuf_filled_slots(dev);
	empty_slots = hw->hbuf_depth - filled_slots;

	 
	if (filled_slots > hw->hbuf_depth)
		return -EOVERFLOW;

	return empty_slots;
}

 
static u32 mei_me_hbuf_depth(const struct mei_device *dev)
{
	struct mei_me_hw *hw = to_me_hw(dev);

	return hw->hbuf_depth;
}

 
static int mei_me_hbuf_write(struct mei_device *dev,
			     const void *hdr, size_t hdr_len,
			     const void *data, size_t data_len)
{
	unsigned long rem;
	unsigned long i;
	const u32 *reg_buf;
	u32 dw_cnt;
	int empty_slots;

	if (WARN_ON(!hdr || hdr_len & 0x3))
		return -EINVAL;

	if (!data && data_len) {
		dev_err(dev->dev, "wrong parameters null data with data_len = %zu\n", data_len);
		return -EINVAL;
	}

	dev_dbg(dev->dev, MEI_HDR_FMT, MEI_HDR_PRM((struct mei_msg_hdr *)hdr));

	empty_slots = mei_hbuf_empty_slots(dev);
	dev_dbg(dev->dev, "empty slots = %d.\n", empty_slots);

	if (empty_slots < 0)
		return -EOVERFLOW;

	dw_cnt = mei_data2slots(hdr_len + data_len);
	if (dw_cnt > (u32)empty_slots)
		return -EMSGSIZE;

	reg_buf = hdr;
	for (i = 0; i < hdr_len / MEI_SLOT_SIZE; i++)
		mei_me_hcbww_write(dev, reg_buf[i]);

	reg_buf = data;
	for (i = 0; i < data_len / MEI_SLOT_SIZE; i++)
		mei_me_hcbww_write(dev, reg_buf[i]);

	rem = data_len & 0x3;
	if (rem > 0) {
		u32 reg = 0;

		memcpy(&reg, (const u8 *)data + data_len - rem, rem);
		mei_me_hcbww_write(dev, reg);
	}

	mei_hcsr_set_hig(dev);
	if (!mei_me_hw_is_ready(dev))
		return -EIO;

	return 0;
}

 
static int mei_me_count_full_read_slots(struct mei_device *dev)
{
	u32 me_csr;
	char read_ptr, write_ptr;
	unsigned char buffer_depth, filled_slots;

	me_csr = mei_me_mecsr_read(dev);
	buffer_depth = (unsigned char)((me_csr & ME_CBD_HRA) >> 24);
	read_ptr = (char) ((me_csr & ME_CBRP_HRA) >> 8);
	write_ptr = (char) ((me_csr & ME_CBWP_HRA) >> 16);
	filled_slots = (unsigned char) (write_ptr - read_ptr);

	 
	if (filled_slots > buffer_depth)
		return -EOVERFLOW;

	dev_dbg(dev->dev, "filled_slots =%08x\n", filled_slots);
	return (int)filled_slots;
}

 
static int mei_me_read_slots(struct mei_device *dev, unsigned char *buffer,
			     unsigned long buffer_length)
{
	u32 *reg_buf = (u32 *)buffer;

	for (; buffer_length >= MEI_SLOT_SIZE; buffer_length -= MEI_SLOT_SIZE)
		*reg_buf++ = mei_me_mecbrw_read(dev);

	if (buffer_length > 0) {
		u32 reg = mei_me_mecbrw_read(dev);

		memcpy(reg_buf, &reg, buffer_length);
	}

	mei_hcsr_set_hig(dev);
	return 0;
}

 
static void mei_me_pg_set(struct mei_device *dev)
{
	struct mei_me_hw *hw = to_me_hw(dev);
	u32 reg;

	reg = mei_me_reg_read(hw, H_HPG_CSR);
	trace_mei_reg_read(dev->dev, "H_HPG_CSR", H_HPG_CSR, reg);

	reg |= H_HPG_CSR_PGI;

	trace_mei_reg_write(dev->dev, "H_HPG_CSR", H_HPG_CSR, reg);
	mei_me_reg_write(hw, H_HPG_CSR, reg);
}

 
static void mei_me_pg_unset(struct mei_device *dev)
{
	struct mei_me_hw *hw = to_me_hw(dev);
	u32 reg;

	reg = mei_me_reg_read(hw, H_HPG_CSR);
	trace_mei_reg_read(dev->dev, "H_HPG_CSR", H_HPG_CSR, reg);

	WARN(!(reg & H_HPG_CSR_PGI), "PGI is not set\n");

	reg |= H_HPG_CSR_PGIHEXR;

	trace_mei_reg_write(dev->dev, "H_HPG_CSR", H_HPG_CSR, reg);
	mei_me_reg_write(hw, H_HPG_CSR, reg);
}

 
static int mei_me_pg_legacy_enter_sync(struct mei_device *dev)
{
	struct mei_me_hw *hw = to_me_hw(dev);
	int ret;

	dev->pg_event = MEI_PG_EVENT_WAIT;

	ret = mei_hbm_pg(dev, MEI_PG_ISOLATION_ENTRY_REQ_CMD);
	if (ret)
		return ret;

	mutex_unlock(&dev->device_lock);
	wait_event_timeout(dev->wait_pg,
		dev->pg_event == MEI_PG_EVENT_RECEIVED,
		dev->timeouts.pgi);
	mutex_lock(&dev->device_lock);

	if (dev->pg_event == MEI_PG_EVENT_RECEIVED) {
		mei_me_pg_set(dev);
		ret = 0;
	} else {
		ret = -ETIME;
	}

	dev->pg_event = MEI_PG_EVENT_IDLE;
	hw->pg_state = MEI_PG_ON;

	return ret;
}

 
static int mei_me_pg_legacy_exit_sync(struct mei_device *dev)
{
	struct mei_me_hw *hw = to_me_hw(dev);
	int ret;

	if (dev->pg_event == MEI_PG_EVENT_RECEIVED)
		goto reply;

	dev->pg_event = MEI_PG_EVENT_WAIT;

	mei_me_pg_unset(dev);

	mutex_unlock(&dev->device_lock);
	wait_event_timeout(dev->wait_pg,
		dev->pg_event == MEI_PG_EVENT_RECEIVED,
		dev->timeouts.pgi);
	mutex_lock(&dev->device_lock);

reply:
	if (dev->pg_event != MEI_PG_EVENT_RECEIVED) {
		ret = -ETIME;
		goto out;
	}

	dev->pg_event = MEI_PG_EVENT_INTR_WAIT;
	ret = mei_hbm_pg(dev, MEI_PG_ISOLATION_EXIT_RES_CMD);
	if (ret)
		return ret;

	mutex_unlock(&dev->device_lock);
	wait_event_timeout(dev->wait_pg,
		dev->pg_event == MEI_PG_EVENT_INTR_RECEIVED,
		dev->timeouts.pgi);
	mutex_lock(&dev->device_lock);

	if (dev->pg_event == MEI_PG_EVENT_INTR_RECEIVED)
		ret = 0;
	else
		ret = -ETIME;

out:
	dev->pg_event = MEI_PG_EVENT_IDLE;
	hw->pg_state = MEI_PG_OFF;

	return ret;
}

 
static bool mei_me_pg_in_transition(struct mei_device *dev)
{
	return dev->pg_event >= MEI_PG_EVENT_WAIT &&
	       dev->pg_event <= MEI_PG_EVENT_INTR_WAIT;
}

 
static bool mei_me_pg_is_enabled(struct mei_device *dev)
{
	struct mei_me_hw *hw = to_me_hw(dev);
	u32 reg = mei_me_mecsr_read(dev);

	if (hw->d0i3_supported)
		return true;

	if ((reg & ME_PGIC_HRA) == 0)
		goto notsupported;

	if (!dev->hbm_f_pg_supported)
		goto notsupported;

	return true;

notsupported:
	dev_dbg(dev->dev, "pg: not supported: d0i3 = %d HGP = %d hbm version %d.%d ?= %d.%d\n",
		hw->d0i3_supported,
		!!(reg & ME_PGIC_HRA),
		dev->version.major_version,
		dev->version.minor_version,
		HBM_MAJOR_VERSION_PGI,
		HBM_MINOR_VERSION_PGI);

	return false;
}

 
static u32 mei_me_d0i3_set(struct mei_device *dev, bool intr)
{
	u32 reg = mei_me_d0i3c_read(dev);

	reg |= H_D0I3C_I3;
	if (intr)
		reg |= H_D0I3C_IR;
	else
		reg &= ~H_D0I3C_IR;
	mei_me_d0i3c_write(dev, reg);
	 
	reg = mei_me_d0i3c_read(dev);
	return reg;
}

 
static u32 mei_me_d0i3_unset(struct mei_device *dev)
{
	u32 reg = mei_me_d0i3c_read(dev);

	reg &= ~H_D0I3C_I3;
	reg |= H_D0I3C_IR;
	mei_me_d0i3c_write(dev, reg);
	 
	reg = mei_me_d0i3c_read(dev);
	return reg;
}

 
static int mei_me_d0i3_enter_sync(struct mei_device *dev)
{
	struct mei_me_hw *hw = to_me_hw(dev);
	int ret;
	u32 reg;

	reg = mei_me_d0i3c_read(dev);
	if (reg & H_D0I3C_I3) {
		 
		dev_dbg(dev->dev, "d0i3 set not needed\n");
		ret = 0;
		goto on;
	}

	 
	dev->pg_event = MEI_PG_EVENT_WAIT;

	ret = mei_hbm_pg(dev, MEI_PG_ISOLATION_ENTRY_REQ_CMD);
	if (ret)
		 
		goto out;

	mutex_unlock(&dev->device_lock);
	wait_event_timeout(dev->wait_pg,
		dev->pg_event == MEI_PG_EVENT_RECEIVED,
		dev->timeouts.pgi);
	mutex_lock(&dev->device_lock);

	if (dev->pg_event != MEI_PG_EVENT_RECEIVED) {
		ret = -ETIME;
		goto out;
	}
	 

	dev->pg_event = MEI_PG_EVENT_INTR_WAIT;

	reg = mei_me_d0i3_set(dev, true);
	if (!(reg & H_D0I3C_CIP)) {
		dev_dbg(dev->dev, "d0i3 enter wait not needed\n");
		ret = 0;
		goto on;
	}

	mutex_unlock(&dev->device_lock);
	wait_event_timeout(dev->wait_pg,
		dev->pg_event == MEI_PG_EVENT_INTR_RECEIVED,
		dev->timeouts.d0i3);
	mutex_lock(&dev->device_lock);

	if (dev->pg_event != MEI_PG_EVENT_INTR_RECEIVED) {
		reg = mei_me_d0i3c_read(dev);
		if (!(reg & H_D0I3C_I3)) {
			ret = -ETIME;
			goto out;
		}
	}

	ret = 0;
on:
	hw->pg_state = MEI_PG_ON;
out:
	dev->pg_event = MEI_PG_EVENT_IDLE;
	dev_dbg(dev->dev, "d0i3 enter ret = %d\n", ret);
	return ret;
}

 
static int mei_me_d0i3_enter(struct mei_device *dev)
{
	struct mei_me_hw *hw = to_me_hw(dev);
	u32 reg;

	reg = mei_me_d0i3c_read(dev);
	if (reg & H_D0I3C_I3) {
		 
		dev_dbg(dev->dev, "already d0i3 : set not needed\n");
		goto on;
	}

	mei_me_d0i3_set(dev, false);
on:
	hw->pg_state = MEI_PG_ON;
	dev->pg_event = MEI_PG_EVENT_IDLE;
	dev_dbg(dev->dev, "d0i3 enter\n");
	return 0;
}

 
static int mei_me_d0i3_exit_sync(struct mei_device *dev)
{
	struct mei_me_hw *hw = to_me_hw(dev);
	int ret;
	u32 reg;

	dev->pg_event = MEI_PG_EVENT_INTR_WAIT;

	reg = mei_me_d0i3c_read(dev);
	if (!(reg & H_D0I3C_I3)) {
		 
		dev_dbg(dev->dev, "d0i3 exit not needed\n");
		ret = 0;
		goto off;
	}

	reg = mei_me_d0i3_unset(dev);
	if (!(reg & H_D0I3C_CIP)) {
		dev_dbg(dev->dev, "d0i3 exit wait not needed\n");
		ret = 0;
		goto off;
	}

	mutex_unlock(&dev->device_lock);
	wait_event_timeout(dev->wait_pg,
		dev->pg_event == MEI_PG_EVENT_INTR_RECEIVED,
		dev->timeouts.d0i3);
	mutex_lock(&dev->device_lock);

	if (dev->pg_event != MEI_PG_EVENT_INTR_RECEIVED) {
		reg = mei_me_d0i3c_read(dev);
		if (reg & H_D0I3C_I3) {
			ret = -ETIME;
			goto out;
		}
	}

	ret = 0;
off:
	hw->pg_state = MEI_PG_OFF;
out:
	dev->pg_event = MEI_PG_EVENT_IDLE;

	dev_dbg(dev->dev, "d0i3 exit ret = %d\n", ret);
	return ret;
}

 
static void mei_me_pg_legacy_intr(struct mei_device *dev)
{
	struct mei_me_hw *hw = to_me_hw(dev);

	if (dev->pg_event != MEI_PG_EVENT_INTR_WAIT)
		return;

	dev->pg_event = MEI_PG_EVENT_INTR_RECEIVED;
	hw->pg_state = MEI_PG_OFF;
	if (waitqueue_active(&dev->wait_pg))
		wake_up(&dev->wait_pg);
}

 
static void mei_me_d0i3_intr(struct mei_device *dev, u32 intr_source)
{
	struct mei_me_hw *hw = to_me_hw(dev);

	if (dev->pg_event == MEI_PG_EVENT_INTR_WAIT &&
	    (intr_source & H_D0I3C_IS)) {
		dev->pg_event = MEI_PG_EVENT_INTR_RECEIVED;
		if (hw->pg_state == MEI_PG_ON) {
			hw->pg_state = MEI_PG_OFF;
			if (dev->hbm_state != MEI_HBM_IDLE) {
				 
				dev_dbg(dev->dev, "d0i3 set host ready\n");
				mei_me_host_set_ready(dev);
			}
		} else {
			hw->pg_state = MEI_PG_ON;
		}

		wake_up(&dev->wait_pg);
	}

	if (hw->pg_state == MEI_PG_ON && (intr_source & H_IS)) {
		 
		dev_dbg(dev->dev, "d0i3 want resume\n");
		mei_hbm_pg_resume(dev);
	}
}

 
static void mei_me_pg_intr(struct mei_device *dev, u32 intr_source)
{
	struct mei_me_hw *hw = to_me_hw(dev);

	if (hw->d0i3_supported)
		mei_me_d0i3_intr(dev, intr_source);
	else
		mei_me_pg_legacy_intr(dev);
}

 
int mei_me_pg_enter_sync(struct mei_device *dev)
{
	struct mei_me_hw *hw = to_me_hw(dev);

	if (hw->d0i3_supported)
		return mei_me_d0i3_enter_sync(dev);
	else
		return mei_me_pg_legacy_enter_sync(dev);
}

 
int mei_me_pg_exit_sync(struct mei_device *dev)
{
	struct mei_me_hw *hw = to_me_hw(dev);

	if (hw->d0i3_supported)
		return mei_me_d0i3_exit_sync(dev);
	else
		return mei_me_pg_legacy_exit_sync(dev);
}

 
static int mei_me_hw_reset(struct mei_device *dev, bool intr_enable)
{
	struct mei_me_hw *hw = to_me_hw(dev);
	int ret;
	u32 hcsr;

	if (intr_enable) {
		mei_me_intr_enable(dev);
		if (hw->d0i3_supported) {
			ret = mei_me_d0i3_exit_sync(dev);
			if (ret)
				return ret;
		} else {
			hw->pg_state = MEI_PG_OFF;
		}
	}

	pm_runtime_set_active(dev->dev);

	hcsr = mei_hcsr_read(dev);
	 
	if ((hcsr & H_RST) == H_RST) {
		dev_warn(dev->dev, "H_RST is set = 0x%08X", hcsr);
		hcsr &= ~H_RST;
		mei_hcsr_set(dev, hcsr);
		hcsr = mei_hcsr_read(dev);
	}

	hcsr |= H_RST | H_IG | H_CSR_IS_MASK;

	if (!intr_enable || mei_me_hw_use_polling(to_me_hw(dev)))
		hcsr &= ~H_CSR_IE_MASK;

	dev->recvd_hw_ready = false;
	mei_hcsr_write(dev, hcsr);

	 
	hcsr = mei_hcsr_read(dev);

	if ((hcsr & H_RST) == 0)
		dev_warn(dev->dev, "H_RST is not set = 0x%08X", hcsr);

	if ((hcsr & H_RDY) == H_RDY)
		dev_warn(dev->dev, "H_RDY is not cleared 0x%08X", hcsr);

	if (!intr_enable) {
		mei_me_hw_reset_release(dev);
		if (hw->d0i3_supported) {
			ret = mei_me_d0i3_enter(dev);
			if (ret)
				return ret;
		}
	}
	return 0;
}

 
irqreturn_t mei_me_irq_quick_handler(int irq, void *dev_id)
{
	struct mei_device *dev = (struct mei_device *)dev_id;
	u32 hcsr;

	hcsr = mei_hcsr_read(dev);
	if (!me_intr_src(hcsr))
		return IRQ_NONE;

	dev_dbg(dev->dev, "interrupt source 0x%08X\n", me_intr_src(hcsr));

	 
	me_intr_disable(dev, hcsr);
	return IRQ_WAKE_THREAD;
}
EXPORT_SYMBOL_GPL(mei_me_irq_quick_handler);

 
irqreturn_t mei_me_irq_thread_handler(int irq, void *dev_id)
{
	struct mei_device *dev = (struct mei_device *) dev_id;
	struct list_head cmpl_list;
	s32 slots;
	u32 hcsr;
	int rets = 0;

	dev_dbg(dev->dev, "function called after ISR to handle the interrupt processing.\n");
	 
	mutex_lock(&dev->device_lock);

	hcsr = mei_hcsr_read(dev);
	me_intr_clear(dev, hcsr);

	INIT_LIST_HEAD(&cmpl_list);

	 
	if (!mei_hw_is_ready(dev) && dev->dev_state != MEI_DEV_RESETTING) {
		dev_warn(dev->dev, "FW not ready: resetting: dev_state = %d pxp = %d\n",
			 dev->dev_state, dev->pxp_mode);
		if (dev->dev_state == MEI_DEV_POWERING_DOWN ||
		    dev->dev_state == MEI_DEV_POWER_DOWN)
			mei_cl_all_disconnect(dev);
		else if (dev->dev_state != MEI_DEV_DISABLED)
			schedule_work(&dev->reset_work);
		goto end;
	}

	if (mei_me_hw_is_resetting(dev))
		mei_hcsr_set_hig(dev);

	mei_me_pg_intr(dev, me_intr_src(hcsr));

	 
	if (!mei_host_is_ready(dev)) {
		if (mei_hw_is_ready(dev)) {
			dev_dbg(dev->dev, "we need to start the dev.\n");
			dev->recvd_hw_ready = true;
			wake_up(&dev->wait_hw_ready);
		} else {
			dev_dbg(dev->dev, "Spurious Interrupt\n");
		}
		goto end;
	}
	 
	slots = mei_count_full_read_slots(dev);
	while (slots > 0) {
		dev_dbg(dev->dev, "slots to read = %08x\n", slots);
		rets = mei_irq_read_handler(dev, &cmpl_list, &slots);
		 
		if (rets == -ENODATA)
			break;

		if (rets) {
			dev_err(dev->dev, "mei_irq_read_handler ret = %d, state = %d.\n",
				rets, dev->dev_state);
			if (dev->dev_state != MEI_DEV_RESETTING &&
			    dev->dev_state != MEI_DEV_DISABLED &&
			    dev->dev_state != MEI_DEV_POWERING_DOWN &&
			    dev->dev_state != MEI_DEV_POWER_DOWN)
				schedule_work(&dev->reset_work);
			goto end;
		}
	}

	dev->hbuf_is_ready = mei_hbuf_is_ready(dev);

	 
	if (dev->pg_event != MEI_PG_EVENT_WAIT &&
	    dev->pg_event != MEI_PG_EVENT_RECEIVED) {
		rets = mei_irq_write_handler(dev, &cmpl_list);
		dev->hbuf_is_ready = mei_hbuf_is_ready(dev);
	}

	mei_irq_compl_handler(dev, &cmpl_list);

end:
	dev_dbg(dev->dev, "interrupt thread end ret = %d\n", rets);
	mei_me_intr_enable(dev);
	mutex_unlock(&dev->device_lock);
	return IRQ_HANDLED;
}
EXPORT_SYMBOL_GPL(mei_me_irq_thread_handler);

#define MEI_POLLING_TIMEOUT_ACTIVE 100
#define MEI_POLLING_TIMEOUT_IDLE   500

 
int mei_me_polling_thread(void *_dev)
{
	struct mei_device *dev = _dev;
	irqreturn_t irq_ret;
	long polling_timeout = MEI_POLLING_TIMEOUT_ACTIVE;

	dev_dbg(dev->dev, "kernel thread is running\n");
	while (!kthread_should_stop()) {
		struct mei_me_hw *hw = to_me_hw(dev);
		u32 hcsr;

		wait_event_timeout(hw->wait_active,
				   hw->is_active || kthread_should_stop(),
				   msecs_to_jiffies(MEI_POLLING_TIMEOUT_IDLE));

		if (kthread_should_stop())
			break;

		hcsr = mei_hcsr_read(dev);
		if (me_intr_src(hcsr)) {
			polling_timeout = MEI_POLLING_TIMEOUT_ACTIVE;
			irq_ret = mei_me_irq_thread_handler(1, dev);
			if (irq_ret != IRQ_HANDLED)
				dev_err(dev->dev, "irq_ret %d\n", irq_ret);
		} else {
			 
			polling_timeout = clamp_val(polling_timeout + MEI_POLLING_TIMEOUT_ACTIVE,
						    MEI_POLLING_TIMEOUT_ACTIVE,
						    MEI_POLLING_TIMEOUT_IDLE);
		}

		schedule_timeout_interruptible(msecs_to_jiffies(polling_timeout));
	}

	return 0;
}
EXPORT_SYMBOL_GPL(mei_me_polling_thread);

static const struct mei_hw_ops mei_me_hw_ops = {

	.trc_status = mei_me_trc_status,
	.fw_status = mei_me_fw_status,
	.pg_state  = mei_me_pg_state,

	.host_is_ready = mei_me_host_is_ready,

	.hw_is_ready = mei_me_hw_is_ready,
	.hw_reset = mei_me_hw_reset,
	.hw_config = mei_me_hw_config,
	.hw_start = mei_me_hw_start,

	.pg_in_transition = mei_me_pg_in_transition,
	.pg_is_enabled = mei_me_pg_is_enabled,

	.intr_clear = mei_me_intr_clear,
	.intr_enable = mei_me_intr_enable,
	.intr_disable = mei_me_intr_disable,
	.synchronize_irq = mei_me_synchronize_irq,

	.hbuf_free_slots = mei_me_hbuf_empty_slots,
	.hbuf_is_ready = mei_me_hbuf_is_empty,
	.hbuf_depth = mei_me_hbuf_depth,

	.write = mei_me_hbuf_write,

	.rdbuf_full_slots = mei_me_count_full_read_slots,
	.read_hdr = mei_me_mecbrw_read,
	.read = mei_me_read_slots
};

 
static bool mei_me_fw_type_nm(const struct pci_dev *pdev)
{
	u32 reg;
	unsigned int devfn;

	devfn = PCI_DEVFN(PCI_SLOT(pdev->devfn), 0);
	pci_bus_read_config_dword(pdev->bus, devfn, PCI_CFG_HFS_2, &reg);
	trace_mei_pci_cfg_read(&pdev->dev, "PCI_CFG_HFS_2", PCI_CFG_HFS_2, reg);
	 
	return (reg & 0x600) == 0x200;
}

#define MEI_CFG_FW_NM                           \
	.quirk_probe = mei_me_fw_type_nm

 
static bool mei_me_fw_type_sps_4(const struct pci_dev *pdev)
{
	u32 reg;
	unsigned int devfn;

	devfn = PCI_DEVFN(PCI_SLOT(pdev->devfn), 0);
	pci_bus_read_config_dword(pdev->bus, devfn, PCI_CFG_HFS_1, &reg);
	trace_mei_pci_cfg_read(&pdev->dev, "PCI_CFG_HFS_1", PCI_CFG_HFS_1, reg);
	return (reg & PCI_CFG_HFS_1_OPMODE_MSK) == PCI_CFG_HFS_1_OPMODE_SPS;
}

#define MEI_CFG_FW_SPS_4                          \
	.quirk_probe = mei_me_fw_type_sps_4

 
static bool mei_me_fw_type_sps_ign(const struct pci_dev *pdev)
{
	u32 reg;
	u32 fw_type;
	unsigned int devfn;

	devfn = PCI_DEVFN(PCI_SLOT(pdev->devfn), 0);
	pci_bus_read_config_dword(pdev->bus, devfn, PCI_CFG_HFS_3, &reg);
	trace_mei_pci_cfg_read(&pdev->dev, "PCI_CFG_HFS_3", PCI_CFG_HFS_3, reg);
	fw_type = (reg & PCI_CFG_HFS_3_FW_SKU_MSK);

	dev_dbg(&pdev->dev, "fw type is %d\n", fw_type);

	return fw_type == PCI_CFG_HFS_3_FW_SKU_IGN ||
	       fw_type == PCI_CFG_HFS_3_FW_SKU_SPS;
}

#define MEI_CFG_KIND_ITOUCH                     \
	.kind = "itouch"

#define MEI_CFG_TYPE_GSC                        \
	.kind = "gsc"

#define MEI_CFG_TYPE_GSCFI                      \
	.kind = "gscfi"

#define MEI_CFG_FW_SPS_IGN                      \
	.quirk_probe = mei_me_fw_type_sps_ign

#define MEI_CFG_FW_VER_SUPP                     \
	.fw_ver_supported = 1

#define MEI_CFG_ICH_HFS                      \
	.fw_status.count = 0

#define MEI_CFG_ICH10_HFS                        \
	.fw_status.count = 1,                   \
	.fw_status.status[0] = PCI_CFG_HFS_1

#define MEI_CFG_PCH_HFS                         \
	.fw_status.count = 2,                   \
	.fw_status.status[0] = PCI_CFG_HFS_1,   \
	.fw_status.status[1] = PCI_CFG_HFS_2

#define MEI_CFG_PCH8_HFS                        \
	.fw_status.count = 6,                   \
	.fw_status.status[0] = PCI_CFG_HFS_1,   \
	.fw_status.status[1] = PCI_CFG_HFS_2,   \
	.fw_status.status[2] = PCI_CFG_HFS_3,   \
	.fw_status.status[3] = PCI_CFG_HFS_4,   \
	.fw_status.status[4] = PCI_CFG_HFS_5,   \
	.fw_status.status[5] = PCI_CFG_HFS_6

#define MEI_CFG_DMA_128 \
	.dma_size[DMA_DSCR_HOST] = SZ_128K, \
	.dma_size[DMA_DSCR_DEVICE] = SZ_128K, \
	.dma_size[DMA_DSCR_CTRL] = PAGE_SIZE

#define MEI_CFG_TRC \
	.hw_trc_supported = 1

 
static const struct mei_cfg mei_me_ich_cfg = {
	MEI_CFG_ICH_HFS,
};

 
static const struct mei_cfg mei_me_ich10_cfg = {
	MEI_CFG_ICH10_HFS,
};

 
static const struct mei_cfg mei_me_pch6_cfg = {
	MEI_CFG_PCH_HFS,
};

 
static const struct mei_cfg mei_me_pch7_cfg = {
	MEI_CFG_PCH_HFS,
	MEI_CFG_FW_VER_SUPP,
};

 
static const struct mei_cfg mei_me_pch_cpt_pbg_cfg = {
	MEI_CFG_PCH_HFS,
	MEI_CFG_FW_VER_SUPP,
	MEI_CFG_FW_NM,
};

 
static const struct mei_cfg mei_me_pch8_cfg = {
	MEI_CFG_PCH8_HFS,
	MEI_CFG_FW_VER_SUPP,
};

 
static const struct mei_cfg mei_me_pch8_itouch_cfg = {
	MEI_CFG_KIND_ITOUCH,
	MEI_CFG_PCH8_HFS,
	MEI_CFG_FW_VER_SUPP,
};

 
static const struct mei_cfg mei_me_pch8_sps_4_cfg = {
	MEI_CFG_PCH8_HFS,
	MEI_CFG_FW_VER_SUPP,
	MEI_CFG_FW_SPS_4,
};

 
static const struct mei_cfg mei_me_pch12_sps_4_cfg = {
	MEI_CFG_PCH8_HFS,
	MEI_CFG_FW_VER_SUPP,
	MEI_CFG_FW_SPS_4,
};

 
static const struct mei_cfg mei_me_pch12_cfg = {
	MEI_CFG_PCH8_HFS,
	MEI_CFG_FW_VER_SUPP,
	MEI_CFG_DMA_128,
};

 
static const struct mei_cfg mei_me_pch12_sps_cfg = {
	MEI_CFG_PCH8_HFS,
	MEI_CFG_FW_VER_SUPP,
	MEI_CFG_DMA_128,
	MEI_CFG_FW_SPS_IGN,
};

 
static const struct mei_cfg mei_me_pch12_itouch_sps_cfg = {
	MEI_CFG_KIND_ITOUCH,
	MEI_CFG_PCH8_HFS,
	MEI_CFG_FW_VER_SUPP,
	MEI_CFG_FW_SPS_IGN,
};

 
static const struct mei_cfg mei_me_pch15_cfg = {
	MEI_CFG_PCH8_HFS,
	MEI_CFG_FW_VER_SUPP,
	MEI_CFG_DMA_128,
	MEI_CFG_TRC,
};

 
static const struct mei_cfg mei_me_pch15_sps_cfg = {
	MEI_CFG_PCH8_HFS,
	MEI_CFG_FW_VER_SUPP,
	MEI_CFG_DMA_128,
	MEI_CFG_TRC,
	MEI_CFG_FW_SPS_IGN,
};

 
static const struct mei_cfg mei_me_gsc_cfg = {
	MEI_CFG_TYPE_GSC,
	MEI_CFG_PCH8_HFS,
	MEI_CFG_FW_VER_SUPP,
};

 
static const struct mei_cfg mei_me_gscfi_cfg = {
	MEI_CFG_TYPE_GSCFI,
	MEI_CFG_PCH8_HFS,
	MEI_CFG_FW_VER_SUPP,
};

 
static const struct mei_cfg *const mei_cfg_list[] = {
	[MEI_ME_UNDEF_CFG] = NULL,
	[MEI_ME_ICH_CFG] = &mei_me_ich_cfg,
	[MEI_ME_ICH10_CFG] = &mei_me_ich10_cfg,
	[MEI_ME_PCH6_CFG] = &mei_me_pch6_cfg,
	[MEI_ME_PCH7_CFG] = &mei_me_pch7_cfg,
	[MEI_ME_PCH_CPT_PBG_CFG] = &mei_me_pch_cpt_pbg_cfg,
	[MEI_ME_PCH8_CFG] = &mei_me_pch8_cfg,
	[MEI_ME_PCH8_ITOUCH_CFG] = &mei_me_pch8_itouch_cfg,
	[MEI_ME_PCH8_SPS_4_CFG] = &mei_me_pch8_sps_4_cfg,
	[MEI_ME_PCH12_CFG] = &mei_me_pch12_cfg,
	[MEI_ME_PCH12_SPS_4_CFG] = &mei_me_pch12_sps_4_cfg,
	[MEI_ME_PCH12_SPS_CFG] = &mei_me_pch12_sps_cfg,
	[MEI_ME_PCH12_SPS_ITOUCH_CFG] = &mei_me_pch12_itouch_sps_cfg,
	[MEI_ME_PCH15_CFG] = &mei_me_pch15_cfg,
	[MEI_ME_PCH15_SPS_CFG] = &mei_me_pch15_sps_cfg,
	[MEI_ME_GSC_CFG] = &mei_me_gsc_cfg,
	[MEI_ME_GSCFI_CFG] = &mei_me_gscfi_cfg,
};

const struct mei_cfg *mei_me_get_cfg(kernel_ulong_t idx)
{
	BUILD_BUG_ON(ARRAY_SIZE(mei_cfg_list) != MEI_ME_NUM_CFG);

	if (idx >= MEI_ME_NUM_CFG)
		return NULL;

	return mei_cfg_list[idx];
}
EXPORT_SYMBOL_GPL(mei_me_get_cfg);

 
struct mei_device *mei_me_dev_init(struct device *parent,
				   const struct mei_cfg *cfg, bool slow_fw)
{
	struct mei_device *dev;
	struct mei_me_hw *hw;
	int i;

	dev = devm_kzalloc(parent, sizeof(*dev) + sizeof(*hw), GFP_KERNEL);
	if (!dev)
		return NULL;

	hw = to_me_hw(dev);

	for (i = 0; i < DMA_DSCR_NUM; i++)
		dev->dr_dscr[i].size = cfg->dma_size[i];

	mei_device_init(dev, parent, slow_fw, &mei_me_hw_ops);
	hw->cfg = cfg;

	dev->fw_f_fw_ver_supported = cfg->fw_ver_supported;

	dev->kind = cfg->kind;

	return dev;
}
EXPORT_SYMBOL_GPL(mei_me_dev_init);
