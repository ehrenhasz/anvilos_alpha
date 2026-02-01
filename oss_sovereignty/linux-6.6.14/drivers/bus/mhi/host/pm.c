
 

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/dma-direction.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/mhi.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include "internal.h"

 
static const struct mhi_pm_transitions dev_state_transitions[] = {
	 
	{
		MHI_PM_DISABLE,
		MHI_PM_POR
	},
	{
		MHI_PM_POR,
		MHI_PM_POR | MHI_PM_DISABLE | MHI_PM_M0 |
		MHI_PM_SYS_ERR_DETECT | MHI_PM_SHUTDOWN_PROCESS |
		MHI_PM_LD_ERR_FATAL_DETECT | MHI_PM_FW_DL_ERR
	},
	{
		MHI_PM_M0,
		MHI_PM_M0 | MHI_PM_M2 | MHI_PM_M3_ENTER |
		MHI_PM_SYS_ERR_DETECT | MHI_PM_SHUTDOWN_PROCESS |
		MHI_PM_LD_ERR_FATAL_DETECT | MHI_PM_FW_DL_ERR
	},
	{
		MHI_PM_M2,
		MHI_PM_M0 | MHI_PM_SYS_ERR_DETECT | MHI_PM_SHUTDOWN_PROCESS |
		MHI_PM_LD_ERR_FATAL_DETECT
	},
	{
		MHI_PM_M3_ENTER,
		MHI_PM_M3 | MHI_PM_SYS_ERR_DETECT | MHI_PM_SHUTDOWN_PROCESS |
		MHI_PM_LD_ERR_FATAL_DETECT
	},
	{
		MHI_PM_M3,
		MHI_PM_M3_EXIT | MHI_PM_SYS_ERR_DETECT |
		MHI_PM_LD_ERR_FATAL_DETECT
	},
	{
		MHI_PM_M3_EXIT,
		MHI_PM_M0 | MHI_PM_SYS_ERR_DETECT | MHI_PM_SHUTDOWN_PROCESS |
		MHI_PM_LD_ERR_FATAL_DETECT
	},
	{
		MHI_PM_FW_DL_ERR,
		MHI_PM_FW_DL_ERR | MHI_PM_SYS_ERR_DETECT |
		MHI_PM_SHUTDOWN_PROCESS | MHI_PM_LD_ERR_FATAL_DETECT
	},
	 
	{
		MHI_PM_SYS_ERR_DETECT,
		MHI_PM_SYS_ERR_PROCESS | MHI_PM_SHUTDOWN_PROCESS |
		MHI_PM_LD_ERR_FATAL_DETECT
	},
	{
		MHI_PM_SYS_ERR_PROCESS,
		MHI_PM_POR | MHI_PM_SHUTDOWN_PROCESS |
		MHI_PM_LD_ERR_FATAL_DETECT
	},
	 
	{
		MHI_PM_SHUTDOWN_PROCESS,
		MHI_PM_DISABLE | MHI_PM_LD_ERR_FATAL_DETECT
	},
	 
	{
		MHI_PM_LD_ERR_FATAL_DETECT,
		MHI_PM_LD_ERR_FATAL_DETECT | MHI_PM_DISABLE
	},
};

enum mhi_pm_state __must_check mhi_tryset_pm_state(struct mhi_controller *mhi_cntrl,
						   enum mhi_pm_state state)
{
	unsigned long cur_state = mhi_cntrl->pm_state;
	int index = find_last_bit(&cur_state, 32);

	if (unlikely(index >= ARRAY_SIZE(dev_state_transitions)))
		return cur_state;

	if (unlikely(dev_state_transitions[index].from_state != cur_state))
		return cur_state;

	if (unlikely(!(dev_state_transitions[index].to_states & state)))
		return cur_state;

	mhi_cntrl->pm_state = state;
	return mhi_cntrl->pm_state;
}

void mhi_set_mhi_state(struct mhi_controller *mhi_cntrl, enum mhi_state state)
{
	struct device *dev = &mhi_cntrl->mhi_dev->dev;
	int ret;

	if (state == MHI_STATE_RESET) {
		ret = mhi_write_reg_field(mhi_cntrl, mhi_cntrl->regs, MHICTRL,
					  MHICTRL_RESET_MASK, 1);
	} else {
		ret = mhi_write_reg_field(mhi_cntrl, mhi_cntrl->regs, MHICTRL,
					  MHICTRL_MHISTATE_MASK, state);
	}

	if (ret)
		dev_err(dev, "Failed to set MHI state to: %s\n",
			mhi_state_str(state));
}

 
static void mhi_toggle_dev_wake_nop(struct mhi_controller *mhi_cntrl)
{
}

static void mhi_toggle_dev_wake(struct mhi_controller *mhi_cntrl)
{
	mhi_cntrl->wake_get(mhi_cntrl, false);
	mhi_cntrl->wake_put(mhi_cntrl, true);
}

 
int mhi_ready_state_transition(struct mhi_controller *mhi_cntrl)
{
	struct mhi_event *mhi_event;
	enum mhi_pm_state cur_state;
	struct device *dev = &mhi_cntrl->mhi_dev->dev;
	u32 interval_us = 25000;  
	int ret, i;

	 
	if (MHI_PM_IN_FATAL_STATE(mhi_cntrl->pm_state)) {
		dev_err(dev, "Device link is not accessible\n");
		return -EIO;
	}

	 
	ret = mhi_poll_reg_field(mhi_cntrl, mhi_cntrl->regs, MHICTRL,
				 MHICTRL_RESET_MASK, 0, interval_us);
	if (ret) {
		dev_err(dev, "Device failed to clear MHI Reset\n");
		return ret;
	}

	ret = mhi_poll_reg_field(mhi_cntrl, mhi_cntrl->regs, MHISTATUS,
				 MHISTATUS_READY_MASK, 1, interval_us);
	if (ret) {
		dev_err(dev, "Device failed to enter MHI Ready\n");
		return ret;
	}

	dev_dbg(dev, "Device in READY State\n");
	write_lock_irq(&mhi_cntrl->pm_lock);
	cur_state = mhi_tryset_pm_state(mhi_cntrl, MHI_PM_POR);
	mhi_cntrl->dev_state = MHI_STATE_READY;
	write_unlock_irq(&mhi_cntrl->pm_lock);

	if (cur_state != MHI_PM_POR) {
		dev_err(dev, "Error moving to state %s from %s\n",
			to_mhi_pm_state_str(MHI_PM_POR),
			to_mhi_pm_state_str(cur_state));
		return -EIO;
	}

	read_lock_bh(&mhi_cntrl->pm_lock);
	if (!MHI_REG_ACCESS_VALID(mhi_cntrl->pm_state)) {
		dev_err(dev, "Device registers not accessible\n");
		goto error_mmio;
	}

	 
	ret = mhi_init_mmio(mhi_cntrl);
	if (ret) {
		dev_err(dev, "Error configuring MMIO registers\n");
		goto error_mmio;
	}

	 
	mhi_event = mhi_cntrl->mhi_event;
	for (i = 0; i < mhi_cntrl->total_ev_rings; i++, mhi_event++) {
		struct mhi_ring *ring = &mhi_event->ring;

		 
		if (mhi_event->offload_ev || mhi_event->hw_ring)
			continue;

		ring->wp = ring->base + ring->len - ring->el_size;
		*ring->ctxt_wp = cpu_to_le64(ring->iommu_base + ring->len - ring->el_size);
		 
		smp_wmb();

		 
		spin_lock_irq(&mhi_event->lock);
		mhi_ring_er_db(mhi_event);
		spin_unlock_irq(&mhi_event->lock);
	}

	 
	mhi_set_mhi_state(mhi_cntrl, MHI_STATE_M0);
	read_unlock_bh(&mhi_cntrl->pm_lock);

	return 0;

error_mmio:
	read_unlock_bh(&mhi_cntrl->pm_lock);

	return -EIO;
}

int mhi_pm_m0_transition(struct mhi_controller *mhi_cntrl)
{
	enum mhi_pm_state cur_state;
	struct mhi_chan *mhi_chan;
	struct device *dev = &mhi_cntrl->mhi_dev->dev;
	int i;

	write_lock_irq(&mhi_cntrl->pm_lock);
	mhi_cntrl->dev_state = MHI_STATE_M0;
	cur_state = mhi_tryset_pm_state(mhi_cntrl, MHI_PM_M0);
	write_unlock_irq(&mhi_cntrl->pm_lock);
	if (unlikely(cur_state != MHI_PM_M0)) {
		dev_err(dev, "Unable to transition to M0 state\n");
		return -EIO;
	}
	mhi_cntrl->M0++;

	 
	read_lock_bh(&mhi_cntrl->pm_lock);
	mhi_cntrl->wake_get(mhi_cntrl, true);

	 
	if (MHI_IN_MISSION_MODE(mhi_cntrl->ee)) {
		struct mhi_event *mhi_event = mhi_cntrl->mhi_event;
		struct mhi_cmd *mhi_cmd =
			&mhi_cntrl->mhi_cmd[PRIMARY_CMD_RING];

		for (i = 0; i < mhi_cntrl->total_ev_rings; i++, mhi_event++) {
			if (mhi_event->offload_ev)
				continue;

			spin_lock_irq(&mhi_event->lock);
			mhi_ring_er_db(mhi_event);
			spin_unlock_irq(&mhi_event->lock);
		}

		 
		spin_lock_irq(&mhi_cmd->lock);
		if (mhi_cmd->ring.rp != mhi_cmd->ring.wp)
			mhi_ring_cmd_db(mhi_cntrl, mhi_cmd);
		spin_unlock_irq(&mhi_cmd->lock);
	}

	 
	mhi_chan = mhi_cntrl->mhi_chan;
	for (i = 0; i < mhi_cntrl->max_chan; i++, mhi_chan++) {
		struct mhi_ring *tre_ring = &mhi_chan->tre_ring;

		if (mhi_chan->db_cfg.reset_req) {
			write_lock_irq(&mhi_chan->lock);
			mhi_chan->db_cfg.db_mode = true;
			write_unlock_irq(&mhi_chan->lock);
		}

		read_lock_irq(&mhi_chan->lock);

		 
		if (tre_ring->base && tre_ring->wp  != tre_ring->rp &&
		    mhi_chan->ch_state == MHI_CH_STATE_ENABLED)
			mhi_ring_chan_db(mhi_cntrl, mhi_chan);
		read_unlock_irq(&mhi_chan->lock);
	}

	mhi_cntrl->wake_put(mhi_cntrl, false);
	read_unlock_bh(&mhi_cntrl->pm_lock);
	wake_up_all(&mhi_cntrl->state_event);

	return 0;
}

 
void mhi_pm_m1_transition(struct mhi_controller *mhi_cntrl)
{
	enum mhi_pm_state state;
	struct device *dev = &mhi_cntrl->mhi_dev->dev;

	write_lock_irq(&mhi_cntrl->pm_lock);
	state = mhi_tryset_pm_state(mhi_cntrl, MHI_PM_M2);
	if (state == MHI_PM_M2) {
		mhi_set_mhi_state(mhi_cntrl, MHI_STATE_M2);
		mhi_cntrl->dev_state = MHI_STATE_M2;

		write_unlock_irq(&mhi_cntrl->pm_lock);

		mhi_cntrl->M2++;
		wake_up_all(&mhi_cntrl->state_event);

		 
		if (unlikely(atomic_read(&mhi_cntrl->pending_pkts) ||
			     atomic_read(&mhi_cntrl->dev_wake))) {
			dev_dbg(dev,
				"Exiting M2, pending_pkts: %d dev_wake: %d\n",
				atomic_read(&mhi_cntrl->pending_pkts),
				atomic_read(&mhi_cntrl->dev_wake));
			read_lock_bh(&mhi_cntrl->pm_lock);
			mhi_cntrl->wake_get(mhi_cntrl, true);
			mhi_cntrl->wake_put(mhi_cntrl, true);
			read_unlock_bh(&mhi_cntrl->pm_lock);
		} else {
			mhi_cntrl->status_cb(mhi_cntrl, MHI_CB_IDLE);
		}
	} else {
		write_unlock_irq(&mhi_cntrl->pm_lock);
	}
}

 
int mhi_pm_m3_transition(struct mhi_controller *mhi_cntrl)
{
	enum mhi_pm_state state;
	struct device *dev = &mhi_cntrl->mhi_dev->dev;

	write_lock_irq(&mhi_cntrl->pm_lock);
	mhi_cntrl->dev_state = MHI_STATE_M3;
	state = mhi_tryset_pm_state(mhi_cntrl, MHI_PM_M3);
	write_unlock_irq(&mhi_cntrl->pm_lock);
	if (state != MHI_PM_M3) {
		dev_err(dev, "Unable to transition to M3 state\n");
		return -EIO;
	}

	mhi_cntrl->M3++;
	wake_up_all(&mhi_cntrl->state_event);

	return 0;
}

 
static int mhi_pm_mission_mode_transition(struct mhi_controller *mhi_cntrl)
{
	struct mhi_event *mhi_event;
	struct device *dev = &mhi_cntrl->mhi_dev->dev;
	enum mhi_ee_type ee = MHI_EE_MAX, current_ee = mhi_cntrl->ee;
	int i, ret;

	dev_dbg(dev, "Processing Mission Mode transition\n");

	write_lock_irq(&mhi_cntrl->pm_lock);
	if (MHI_REG_ACCESS_VALID(mhi_cntrl->pm_state))
		ee = mhi_get_exec_env(mhi_cntrl);

	if (!MHI_IN_MISSION_MODE(ee)) {
		mhi_cntrl->pm_state = MHI_PM_LD_ERR_FATAL_DETECT;
		write_unlock_irq(&mhi_cntrl->pm_lock);
		wake_up_all(&mhi_cntrl->state_event);
		return -EIO;
	}
	mhi_cntrl->ee = ee;
	write_unlock_irq(&mhi_cntrl->pm_lock);

	wake_up_all(&mhi_cntrl->state_event);

	device_for_each_child(&mhi_cntrl->mhi_dev->dev, &current_ee,
			      mhi_destroy_device);
	mhi_cntrl->status_cb(mhi_cntrl, MHI_CB_EE_MISSION_MODE);

	 
	ret = __mhi_device_get_sync(mhi_cntrl);
	if (ret)
		return ret;

	read_lock_bh(&mhi_cntrl->pm_lock);

	if (MHI_PM_IN_ERROR_STATE(mhi_cntrl->pm_state)) {
		ret = -EIO;
		goto error_mission_mode;
	}

	 
	mhi_event = mhi_cntrl->mhi_event;
	for (i = 0; i < mhi_cntrl->total_ev_rings; i++, mhi_event++) {
		struct mhi_ring *ring = &mhi_event->ring;

		if (mhi_event->offload_ev || !mhi_event->hw_ring)
			continue;

		ring->wp = ring->base + ring->len - ring->el_size;
		*ring->ctxt_wp = cpu_to_le64(ring->iommu_base + ring->len - ring->el_size);
		 
		smp_wmb();

		spin_lock_irq(&mhi_event->lock);
		if (MHI_DB_ACCESS_VALID(mhi_cntrl))
			mhi_ring_er_db(mhi_event);
		spin_unlock_irq(&mhi_event->lock);
	}

	read_unlock_bh(&mhi_cntrl->pm_lock);

	 
	mhi_create_devices(mhi_cntrl);

	read_lock_bh(&mhi_cntrl->pm_lock);

error_mission_mode:
	mhi_cntrl->wake_put(mhi_cntrl, false);
	read_unlock_bh(&mhi_cntrl->pm_lock);

	return ret;
}

 
static void mhi_pm_disable_transition(struct mhi_controller *mhi_cntrl)
{
	enum mhi_pm_state cur_state;
	struct mhi_event *mhi_event;
	struct mhi_cmd_ctxt *cmd_ctxt;
	struct mhi_cmd *mhi_cmd;
	struct mhi_event_ctxt *er_ctxt;
	struct device *dev = &mhi_cntrl->mhi_dev->dev;
	int ret, i;

	dev_dbg(dev, "Processing disable transition with PM state: %s\n",
		to_mhi_pm_state_str(mhi_cntrl->pm_state));

	mutex_lock(&mhi_cntrl->pm_mutex);

	 
	if (!MHI_PM_IN_FATAL_STATE(mhi_cntrl->pm_state)) {
		 
		if (mhi_cntrl->rddm_image && mhi_get_exec_env(mhi_cntrl) == MHI_EE_RDDM)
			goto skip_mhi_reset;

		dev_dbg(dev, "Triggering MHI Reset in device\n");
		mhi_set_mhi_state(mhi_cntrl, MHI_STATE_RESET);

		 
		ret = mhi_poll_reg_field(mhi_cntrl, mhi_cntrl->regs, MHICTRL,
				 MHICTRL_RESET_MASK, 0, 25000);
		if (ret)
			dev_err(dev, "Device failed to clear MHI Reset\n");

		 
		mhi_write_reg(mhi_cntrl, mhi_cntrl->bhi, BHI_INTVEC, 0);

		if (!MHI_IN_PBL(mhi_get_exec_env(mhi_cntrl))) {
			 
			ret = mhi_poll_reg_field(mhi_cntrl, mhi_cntrl->regs,
						 MHISTATUS,
						 MHISTATUS_READY_MASK, 1, 25000);
			if (ret)
				dev_err(dev, "Device failed to enter READY state\n");
		}
	}

skip_mhi_reset:
	dev_dbg(dev,
		 "Waiting for all pending event ring processing to complete\n");
	mhi_event = mhi_cntrl->mhi_event;
	for (i = 0; i < mhi_cntrl->total_ev_rings; i++, mhi_event++) {
		if (mhi_event->offload_ev)
			continue;
		disable_irq(mhi_cntrl->irq[mhi_event->irq]);
		tasklet_kill(&mhi_event->task);
	}

	 
	mutex_unlock(&mhi_cntrl->pm_mutex);
	dev_dbg(dev, "Waiting for all pending threads to complete\n");
	wake_up_all(&mhi_cntrl->state_event);

	dev_dbg(dev, "Reset all active channels and remove MHI devices\n");
	device_for_each_child(&mhi_cntrl->mhi_dev->dev, NULL, mhi_destroy_device);

	mutex_lock(&mhi_cntrl->pm_mutex);

	WARN_ON(atomic_read(&mhi_cntrl->dev_wake));
	WARN_ON(atomic_read(&mhi_cntrl->pending_pkts));

	 
	dev_dbg(dev, "Resetting EV CTXT and CMD CTXT\n");
	mhi_cmd = mhi_cntrl->mhi_cmd;
	cmd_ctxt = mhi_cntrl->mhi_ctxt->cmd_ctxt;
	for (i = 0; i < NR_OF_CMD_RINGS; i++, mhi_cmd++, cmd_ctxt++) {
		struct mhi_ring *ring = &mhi_cmd->ring;

		ring->rp = ring->base;
		ring->wp = ring->base;
		cmd_ctxt->rp = cmd_ctxt->rbase;
		cmd_ctxt->wp = cmd_ctxt->rbase;
	}

	mhi_event = mhi_cntrl->mhi_event;
	er_ctxt = mhi_cntrl->mhi_ctxt->er_ctxt;
	for (i = 0; i < mhi_cntrl->total_ev_rings; i++, er_ctxt++,
		     mhi_event++) {
		struct mhi_ring *ring = &mhi_event->ring;

		 
		if (mhi_event->offload_ev)
			continue;

		ring->rp = ring->base;
		ring->wp = ring->base;
		er_ctxt->rp = er_ctxt->rbase;
		er_ctxt->wp = er_ctxt->rbase;
	}

	 
	write_lock_irq(&mhi_cntrl->pm_lock);
	cur_state = mhi_tryset_pm_state(mhi_cntrl, MHI_PM_DISABLE);
	write_unlock_irq(&mhi_cntrl->pm_lock);
	if (unlikely(cur_state != MHI_PM_DISABLE))
		dev_err(dev, "Error moving from PM state: %s to: %s\n",
			to_mhi_pm_state_str(cur_state),
			to_mhi_pm_state_str(MHI_PM_DISABLE));

	dev_dbg(dev, "Exiting with PM state: %s, MHI state: %s\n",
		to_mhi_pm_state_str(mhi_cntrl->pm_state),
		mhi_state_str(mhi_cntrl->dev_state));

	mutex_unlock(&mhi_cntrl->pm_mutex);
}

 
static void mhi_pm_sys_error_transition(struct mhi_controller *mhi_cntrl)
{
	enum mhi_pm_state cur_state, prev_state;
	enum dev_st_transition next_state;
	struct mhi_event *mhi_event;
	struct mhi_cmd_ctxt *cmd_ctxt;
	struct mhi_cmd *mhi_cmd;
	struct mhi_event_ctxt *er_ctxt;
	struct device *dev = &mhi_cntrl->mhi_dev->dev;
	int ret, i;

	dev_dbg(dev, "Transitioning from PM state: %s to: %s\n",
		to_mhi_pm_state_str(mhi_cntrl->pm_state),
		to_mhi_pm_state_str(MHI_PM_SYS_ERR_PROCESS));

	 
	mhi_cntrl->status_cb(mhi_cntrl, MHI_CB_SYS_ERROR);

	mutex_lock(&mhi_cntrl->pm_mutex);
	write_lock_irq(&mhi_cntrl->pm_lock);
	prev_state = mhi_cntrl->pm_state;
	cur_state = mhi_tryset_pm_state(mhi_cntrl, MHI_PM_SYS_ERR_PROCESS);
	write_unlock_irq(&mhi_cntrl->pm_lock);

	if (cur_state != MHI_PM_SYS_ERR_PROCESS) {
		dev_err(dev, "Failed to transition from PM state: %s to: %s\n",
			to_mhi_pm_state_str(cur_state),
			to_mhi_pm_state_str(MHI_PM_SYS_ERR_PROCESS));
		goto exit_sys_error_transition;
	}

	mhi_cntrl->ee = MHI_EE_DISABLE_TRANSITION;
	mhi_cntrl->dev_state = MHI_STATE_RESET;

	 
	wake_up_all(&mhi_cntrl->state_event);

	 
	if (MHI_REG_ACCESS_VALID(prev_state)) {
		u32 in_reset = -1;
		unsigned long timeout = msecs_to_jiffies(mhi_cntrl->timeout_ms);

		dev_dbg(dev, "Triggering MHI Reset in device\n");
		mhi_set_mhi_state(mhi_cntrl, MHI_STATE_RESET);

		 
		ret = wait_event_timeout(mhi_cntrl->state_event,
					 mhi_read_reg_field(mhi_cntrl,
							    mhi_cntrl->regs,
							    MHICTRL,
							    MHICTRL_RESET_MASK,
							    &in_reset) ||
					!in_reset, timeout);
		if (!ret || in_reset) {
			dev_err(dev, "Device failed to exit MHI Reset state\n");
			goto exit_sys_error_transition;
		}

		 
		mhi_write_reg(mhi_cntrl, mhi_cntrl->bhi, BHI_INTVEC, 0);
	}

	dev_dbg(dev,
		"Waiting for all pending event ring processing to complete\n");
	mhi_event = mhi_cntrl->mhi_event;
	for (i = 0; i < mhi_cntrl->total_ev_rings; i++, mhi_event++) {
		if (mhi_event->offload_ev)
			continue;
		tasklet_kill(&mhi_event->task);
	}

	 
	mutex_unlock(&mhi_cntrl->pm_mutex);
	dev_dbg(dev, "Waiting for all pending threads to complete\n");
	wake_up_all(&mhi_cntrl->state_event);

	dev_dbg(dev, "Reset all active channels and remove MHI devices\n");
	device_for_each_child(&mhi_cntrl->mhi_dev->dev, NULL, mhi_destroy_device);

	mutex_lock(&mhi_cntrl->pm_mutex);

	WARN_ON(atomic_read(&mhi_cntrl->dev_wake));
	WARN_ON(atomic_read(&mhi_cntrl->pending_pkts));

	 
	dev_dbg(dev, "Resetting EV CTXT and CMD CTXT\n");
	mhi_cmd = mhi_cntrl->mhi_cmd;
	cmd_ctxt = mhi_cntrl->mhi_ctxt->cmd_ctxt;
	for (i = 0; i < NR_OF_CMD_RINGS; i++, mhi_cmd++, cmd_ctxt++) {
		struct mhi_ring *ring = &mhi_cmd->ring;

		ring->rp = ring->base;
		ring->wp = ring->base;
		cmd_ctxt->rp = cmd_ctxt->rbase;
		cmd_ctxt->wp = cmd_ctxt->rbase;
	}

	mhi_event = mhi_cntrl->mhi_event;
	er_ctxt = mhi_cntrl->mhi_ctxt->er_ctxt;
	for (i = 0; i < mhi_cntrl->total_ev_rings; i++, er_ctxt++,
	     mhi_event++) {
		struct mhi_ring *ring = &mhi_event->ring;

		 
		if (mhi_event->offload_ev)
			continue;

		ring->rp = ring->base;
		ring->wp = ring->base;
		er_ctxt->rp = er_ctxt->rbase;
		er_ctxt->wp = er_ctxt->rbase;
	}

	 
	if (MHI_IN_PBL(mhi_get_exec_env(mhi_cntrl))) {
		write_lock_irq(&mhi_cntrl->pm_lock);
		cur_state = mhi_tryset_pm_state(mhi_cntrl, MHI_PM_POR);
		write_unlock_irq(&mhi_cntrl->pm_lock);
		if (cur_state != MHI_PM_POR) {
			dev_err(dev, "Error moving to state %s from %s\n",
				to_mhi_pm_state_str(MHI_PM_POR),
				to_mhi_pm_state_str(cur_state));
			goto exit_sys_error_transition;
		}
		next_state = DEV_ST_TRANSITION_PBL;
	} else {
		next_state = DEV_ST_TRANSITION_READY;
	}

	mhi_queue_state_transition(mhi_cntrl, next_state);

exit_sys_error_transition:
	dev_dbg(dev, "Exiting with PM state: %s, MHI state: %s\n",
		to_mhi_pm_state_str(mhi_cntrl->pm_state),
		mhi_state_str(mhi_cntrl->dev_state));

	mutex_unlock(&mhi_cntrl->pm_mutex);
}

 
int mhi_queue_state_transition(struct mhi_controller *mhi_cntrl,
			       enum dev_st_transition state)
{
	struct state_transition *item = kmalloc(sizeof(*item), GFP_ATOMIC);
	unsigned long flags;

	if (!item)
		return -ENOMEM;

	item->state = state;
	spin_lock_irqsave(&mhi_cntrl->transition_lock, flags);
	list_add_tail(&item->node, &mhi_cntrl->transition_list);
	spin_unlock_irqrestore(&mhi_cntrl->transition_lock, flags);

	queue_work(mhi_cntrl->hiprio_wq, &mhi_cntrl->st_worker);

	return 0;
}

 
void mhi_pm_sys_err_handler(struct mhi_controller *mhi_cntrl)
{
	struct device *dev = &mhi_cntrl->mhi_dev->dev;

	 
	if (mhi_cntrl->rddm_image) {
		dev_dbg(dev, "Controller supports RDDM, skip SYS_ERROR\n");
		return;
	}

	mhi_queue_state_transition(mhi_cntrl, DEV_ST_TRANSITION_SYS_ERR);
}

 
void mhi_pm_st_worker(struct work_struct *work)
{
	struct state_transition *itr, *tmp;
	LIST_HEAD(head);
	struct mhi_controller *mhi_cntrl = container_of(work,
							struct mhi_controller,
							st_worker);
	struct device *dev = &mhi_cntrl->mhi_dev->dev;

	spin_lock_irq(&mhi_cntrl->transition_lock);
	list_splice_tail_init(&mhi_cntrl->transition_list, &head);
	spin_unlock_irq(&mhi_cntrl->transition_lock);

	list_for_each_entry_safe(itr, tmp, &head, node) {
		list_del(&itr->node);
		dev_dbg(dev, "Handling state transition: %s\n",
			TO_DEV_STATE_TRANS_STR(itr->state));

		switch (itr->state) {
		case DEV_ST_TRANSITION_PBL:
			write_lock_irq(&mhi_cntrl->pm_lock);
			if (MHI_REG_ACCESS_VALID(mhi_cntrl->pm_state))
				mhi_cntrl->ee = mhi_get_exec_env(mhi_cntrl);
			write_unlock_irq(&mhi_cntrl->pm_lock);
			mhi_fw_load_handler(mhi_cntrl);
			break;
		case DEV_ST_TRANSITION_SBL:
			write_lock_irq(&mhi_cntrl->pm_lock);
			mhi_cntrl->ee = MHI_EE_SBL;
			write_unlock_irq(&mhi_cntrl->pm_lock);
			 
			mhi_create_devices(mhi_cntrl);
			if (mhi_cntrl->fbc_download)
				mhi_download_amss_image(mhi_cntrl);
			break;
		case DEV_ST_TRANSITION_MISSION_MODE:
			mhi_pm_mission_mode_transition(mhi_cntrl);
			break;
		case DEV_ST_TRANSITION_FP:
			write_lock_irq(&mhi_cntrl->pm_lock);
			mhi_cntrl->ee = MHI_EE_FP;
			write_unlock_irq(&mhi_cntrl->pm_lock);
			mhi_create_devices(mhi_cntrl);
			break;
		case DEV_ST_TRANSITION_READY:
			mhi_ready_state_transition(mhi_cntrl);
			break;
		case DEV_ST_TRANSITION_SYS_ERR:
			mhi_pm_sys_error_transition(mhi_cntrl);
			break;
		case DEV_ST_TRANSITION_DISABLE:
			mhi_pm_disable_transition(mhi_cntrl);
			break;
		default:
			break;
		}
		kfree(itr);
	}
}

int mhi_pm_suspend(struct mhi_controller *mhi_cntrl)
{
	struct mhi_chan *itr, *tmp;
	struct device *dev = &mhi_cntrl->mhi_dev->dev;
	enum mhi_pm_state new_state;
	int ret;

	if (mhi_cntrl->pm_state == MHI_PM_DISABLE)
		return -EINVAL;

	if (MHI_PM_IN_ERROR_STATE(mhi_cntrl->pm_state))
		return -EIO;

	 
	if (atomic_read(&mhi_cntrl->dev_wake) ||
	    atomic_read(&mhi_cntrl->pending_pkts))
		return -EBUSY;

	 
	read_lock_bh(&mhi_cntrl->pm_lock);
	mhi_cntrl->wake_get(mhi_cntrl, false);
	read_unlock_bh(&mhi_cntrl->pm_lock);

	ret = wait_event_timeout(mhi_cntrl->state_event,
				 mhi_cntrl->dev_state == MHI_STATE_M0 ||
				 mhi_cntrl->dev_state == MHI_STATE_M1 ||
				 MHI_PM_IN_ERROR_STATE(mhi_cntrl->pm_state),
				 msecs_to_jiffies(mhi_cntrl->timeout_ms));

	read_lock_bh(&mhi_cntrl->pm_lock);
	mhi_cntrl->wake_put(mhi_cntrl, false);
	read_unlock_bh(&mhi_cntrl->pm_lock);

	if (!ret || MHI_PM_IN_ERROR_STATE(mhi_cntrl->pm_state)) {
		dev_err(dev,
			"Could not enter M0/M1 state");
		return -EIO;
	}

	write_lock_irq(&mhi_cntrl->pm_lock);

	if (atomic_read(&mhi_cntrl->dev_wake) ||
	    atomic_read(&mhi_cntrl->pending_pkts)) {
		write_unlock_irq(&mhi_cntrl->pm_lock);
		return -EBUSY;
	}

	dev_dbg(dev, "Allowing M3 transition\n");
	new_state = mhi_tryset_pm_state(mhi_cntrl, MHI_PM_M3_ENTER);
	if (new_state != MHI_PM_M3_ENTER) {
		write_unlock_irq(&mhi_cntrl->pm_lock);
		dev_err(dev,
			"Error setting to PM state: %s from: %s\n",
			to_mhi_pm_state_str(MHI_PM_M3_ENTER),
			to_mhi_pm_state_str(mhi_cntrl->pm_state));
		return -EIO;
	}

	 
	mhi_set_mhi_state(mhi_cntrl, MHI_STATE_M3);
	write_unlock_irq(&mhi_cntrl->pm_lock);
	dev_dbg(dev, "Waiting for M3 completion\n");

	ret = wait_event_timeout(mhi_cntrl->state_event,
				 mhi_cntrl->dev_state == MHI_STATE_M3 ||
				 MHI_PM_IN_ERROR_STATE(mhi_cntrl->pm_state),
				 msecs_to_jiffies(mhi_cntrl->timeout_ms));

	if (!ret || MHI_PM_IN_ERROR_STATE(mhi_cntrl->pm_state)) {
		dev_err(dev,
			"Did not enter M3 state, MHI state: %s, PM state: %s\n",
			mhi_state_str(mhi_cntrl->dev_state),
			to_mhi_pm_state_str(mhi_cntrl->pm_state));
		return -EIO;
	}

	 
	list_for_each_entry_safe(itr, tmp, &mhi_cntrl->lpm_chans, node) {
		mutex_lock(&itr->mutex);
		if (itr->mhi_dev)
			mhi_notify(itr->mhi_dev, MHI_CB_LPM_ENTER);
		mutex_unlock(&itr->mutex);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(mhi_pm_suspend);

static int __mhi_pm_resume(struct mhi_controller *mhi_cntrl, bool force)
{
	struct mhi_chan *itr, *tmp;
	struct device *dev = &mhi_cntrl->mhi_dev->dev;
	enum mhi_pm_state cur_state;
	int ret;

	dev_dbg(dev, "Entered with PM state: %s, MHI state: %s\n",
		to_mhi_pm_state_str(mhi_cntrl->pm_state),
		mhi_state_str(mhi_cntrl->dev_state));

	if (mhi_cntrl->pm_state == MHI_PM_DISABLE)
		return 0;

	if (MHI_PM_IN_ERROR_STATE(mhi_cntrl->pm_state))
		return -EIO;

	if (mhi_get_mhi_state(mhi_cntrl) != MHI_STATE_M3) {
		dev_warn(dev, "Resuming from non M3 state (%s)\n",
			 mhi_state_str(mhi_get_mhi_state(mhi_cntrl)));
		if (!force)
			return -EINVAL;
	}

	 
	list_for_each_entry_safe(itr, tmp, &mhi_cntrl->lpm_chans, node) {
		mutex_lock(&itr->mutex);
		if (itr->mhi_dev)
			mhi_notify(itr->mhi_dev, MHI_CB_LPM_EXIT);
		mutex_unlock(&itr->mutex);
	}

	write_lock_irq(&mhi_cntrl->pm_lock);
	cur_state = mhi_tryset_pm_state(mhi_cntrl, MHI_PM_M3_EXIT);
	if (cur_state != MHI_PM_M3_EXIT) {
		write_unlock_irq(&mhi_cntrl->pm_lock);
		dev_info(dev,
			 "Error setting to PM state: %s from: %s\n",
			 to_mhi_pm_state_str(MHI_PM_M3_EXIT),
			 to_mhi_pm_state_str(mhi_cntrl->pm_state));
		return -EIO;
	}

	 
	mhi_set_mhi_state(mhi_cntrl, MHI_STATE_M0);
	write_unlock_irq(&mhi_cntrl->pm_lock);

	ret = wait_event_timeout(mhi_cntrl->state_event,
				 mhi_cntrl->dev_state == MHI_STATE_M0 ||
				 mhi_cntrl->dev_state == MHI_STATE_M2 ||
				 MHI_PM_IN_ERROR_STATE(mhi_cntrl->pm_state),
				 msecs_to_jiffies(mhi_cntrl->timeout_ms));

	if (!ret || MHI_PM_IN_ERROR_STATE(mhi_cntrl->pm_state)) {
		dev_err(dev,
			"Did not enter M0 state, MHI state: %s, PM state: %s\n",
			mhi_state_str(mhi_cntrl->dev_state),
			to_mhi_pm_state_str(mhi_cntrl->pm_state));
		return -EIO;
	}

	return 0;
}

int mhi_pm_resume(struct mhi_controller *mhi_cntrl)
{
	return __mhi_pm_resume(mhi_cntrl, false);
}
EXPORT_SYMBOL_GPL(mhi_pm_resume);

int mhi_pm_resume_force(struct mhi_controller *mhi_cntrl)
{
	return __mhi_pm_resume(mhi_cntrl, true);
}
EXPORT_SYMBOL_GPL(mhi_pm_resume_force);

int __mhi_device_get_sync(struct mhi_controller *mhi_cntrl)
{
	int ret;

	 
	read_lock_bh(&mhi_cntrl->pm_lock);
	if (MHI_PM_IN_ERROR_STATE(mhi_cntrl->pm_state)) {
		read_unlock_bh(&mhi_cntrl->pm_lock);
		return -EIO;
	}
	mhi_cntrl->wake_get(mhi_cntrl, true);
	if (MHI_PM_IN_SUSPEND_STATE(mhi_cntrl->pm_state))
		mhi_trigger_resume(mhi_cntrl);
	read_unlock_bh(&mhi_cntrl->pm_lock);

	ret = wait_event_timeout(mhi_cntrl->state_event,
				 mhi_cntrl->pm_state == MHI_PM_M0 ||
				 MHI_PM_IN_ERROR_STATE(mhi_cntrl->pm_state),
				 msecs_to_jiffies(mhi_cntrl->timeout_ms));

	if (!ret || MHI_PM_IN_ERROR_STATE(mhi_cntrl->pm_state)) {
		read_lock_bh(&mhi_cntrl->pm_lock);
		mhi_cntrl->wake_put(mhi_cntrl, false);
		read_unlock_bh(&mhi_cntrl->pm_lock);
		return -EIO;
	}

	return 0;
}

 
static void mhi_assert_dev_wake(struct mhi_controller *mhi_cntrl, bool force)
{
	unsigned long flags;

	 
	if (unlikely(force)) {
		spin_lock_irqsave(&mhi_cntrl->wlock, flags);
		atomic_inc(&mhi_cntrl->dev_wake);
		if (MHI_WAKE_DB_FORCE_SET_VALID(mhi_cntrl->pm_state) &&
		    !mhi_cntrl->wake_set) {
			mhi_write_db(mhi_cntrl, mhi_cntrl->wake_db, 1);
			mhi_cntrl->wake_set = true;
		}
		spin_unlock_irqrestore(&mhi_cntrl->wlock, flags);
	} else {
		 
		if (likely(atomic_add_unless(&mhi_cntrl->dev_wake, 1, 0)))
			return;

		spin_lock_irqsave(&mhi_cntrl->wlock, flags);
		if ((atomic_inc_return(&mhi_cntrl->dev_wake) == 1) &&
		    MHI_WAKE_DB_SET_VALID(mhi_cntrl->pm_state) &&
		    !mhi_cntrl->wake_set) {
			mhi_write_db(mhi_cntrl, mhi_cntrl->wake_db, 1);
			mhi_cntrl->wake_set = true;
		}
		spin_unlock_irqrestore(&mhi_cntrl->wlock, flags);
	}
}

 
static void mhi_deassert_dev_wake(struct mhi_controller *mhi_cntrl,
				  bool override)
{
	unsigned long flags;

	 
	if (likely(atomic_add_unless(&mhi_cntrl->dev_wake, -1, 1)))
		return;

	spin_lock_irqsave(&mhi_cntrl->wlock, flags);
	if ((atomic_dec_return(&mhi_cntrl->dev_wake) == 0) &&
	    MHI_WAKE_DB_CLEAR_VALID(mhi_cntrl->pm_state) && !override &&
	    mhi_cntrl->wake_set) {
		mhi_write_db(mhi_cntrl, mhi_cntrl->wake_db, 0);
		mhi_cntrl->wake_set = false;
	}
	spin_unlock_irqrestore(&mhi_cntrl->wlock, flags);
}

int mhi_async_power_up(struct mhi_controller *mhi_cntrl)
{
	struct mhi_event *mhi_event = mhi_cntrl->mhi_event;
	enum mhi_state state;
	enum mhi_ee_type current_ee;
	enum dev_st_transition next_state;
	struct device *dev = &mhi_cntrl->mhi_dev->dev;
	u32 interval_us = 25000;  
	int ret, i;

	dev_info(dev, "Requested to power ON\n");

	 
	if (!mhi_cntrl->wake_get || !mhi_cntrl->wake_put ||
	    !mhi_cntrl->wake_toggle) {
		mhi_cntrl->wake_get = mhi_assert_dev_wake;
		mhi_cntrl->wake_put = mhi_deassert_dev_wake;
		mhi_cntrl->wake_toggle = (mhi_cntrl->db_access & MHI_PM_M2) ?
			mhi_toggle_dev_wake_nop : mhi_toggle_dev_wake;
	}

	mutex_lock(&mhi_cntrl->pm_mutex);
	mhi_cntrl->pm_state = MHI_PM_DISABLE;

	 
	write_lock_irq(&mhi_cntrl->pm_lock);
	mhi_write_reg(mhi_cntrl, mhi_cntrl->bhi, BHI_INTVEC, 0);
	mhi_cntrl->pm_state = MHI_PM_POR;
	mhi_cntrl->ee = MHI_EE_MAX;
	current_ee = mhi_get_exec_env(mhi_cntrl);
	write_unlock_irq(&mhi_cntrl->pm_lock);

	 
	if (!MHI_POWER_UP_CAPABLE(current_ee)) {
		dev_err(dev, "%s is not a valid EE for power on\n",
			TO_MHI_EXEC_STR(current_ee));
		ret = -EIO;
		goto error_exit;
	}

	state = mhi_get_mhi_state(mhi_cntrl);
	dev_dbg(dev, "Attempting power on with EE: %s, state: %s\n",
		TO_MHI_EXEC_STR(current_ee), mhi_state_str(state));

	if (state == MHI_STATE_SYS_ERR) {
		mhi_set_mhi_state(mhi_cntrl, MHI_STATE_RESET);
		ret = mhi_poll_reg_field(mhi_cntrl, mhi_cntrl->regs, MHICTRL,
				 MHICTRL_RESET_MASK, 0, interval_us);
		if (ret) {
			dev_info(dev, "Failed to reset MHI due to syserr state\n");
			goto error_exit;
		}

		 
		mhi_write_reg(mhi_cntrl, mhi_cntrl->bhi, BHI_INTVEC, 0);
	}

	 
	enable_irq(mhi_cntrl->irq[0]);

	for (i = 0; i < mhi_cntrl->total_ev_rings; i++, mhi_event++) {
		if (mhi_event->offload_ev)
			continue;

		enable_irq(mhi_cntrl->irq[mhi_event->irq]);
	}

	 
	next_state = MHI_IN_PBL(current_ee) ?
		DEV_ST_TRANSITION_PBL : DEV_ST_TRANSITION_READY;

	mhi_queue_state_transition(mhi_cntrl, next_state);

	mutex_unlock(&mhi_cntrl->pm_mutex);

	dev_info(dev, "Power on setup success\n");

	return 0;

error_exit:
	mhi_cntrl->pm_state = MHI_PM_DISABLE;
	mutex_unlock(&mhi_cntrl->pm_mutex);

	return ret;
}
EXPORT_SYMBOL_GPL(mhi_async_power_up);

void mhi_power_down(struct mhi_controller *mhi_cntrl, bool graceful)
{
	enum mhi_pm_state cur_state, transition_state;
	struct device *dev = &mhi_cntrl->mhi_dev->dev;

	mutex_lock(&mhi_cntrl->pm_mutex);
	write_lock_irq(&mhi_cntrl->pm_lock);
	cur_state = mhi_cntrl->pm_state;
	if (cur_state == MHI_PM_DISABLE) {
		write_unlock_irq(&mhi_cntrl->pm_lock);
		mutex_unlock(&mhi_cntrl->pm_mutex);
		return;  
	}

	 
	transition_state = (graceful) ? MHI_PM_SHUTDOWN_PROCESS :
			   MHI_PM_LD_ERR_FATAL_DETECT;

	cur_state = mhi_tryset_pm_state(mhi_cntrl, transition_state);
	if (cur_state != transition_state) {
		dev_err(dev, "Failed to move to state: %s from: %s\n",
			to_mhi_pm_state_str(transition_state),
			to_mhi_pm_state_str(mhi_cntrl->pm_state));
		 
		mhi_cntrl->pm_state = MHI_PM_LD_ERR_FATAL_DETECT;
	}

	 
	mhi_cntrl->ee = MHI_EE_DISABLE_TRANSITION;
	mhi_cntrl->dev_state = MHI_STATE_RESET;

	wake_up_all(&mhi_cntrl->state_event);

	write_unlock_irq(&mhi_cntrl->pm_lock);
	mutex_unlock(&mhi_cntrl->pm_mutex);

	mhi_queue_state_transition(mhi_cntrl, DEV_ST_TRANSITION_DISABLE);

	 
	flush_work(&mhi_cntrl->st_worker);

	disable_irq(mhi_cntrl->irq[0]);
}
EXPORT_SYMBOL_GPL(mhi_power_down);

int mhi_sync_power_up(struct mhi_controller *mhi_cntrl)
{
	int ret = mhi_async_power_up(mhi_cntrl);

	if (ret)
		return ret;

	wait_event_timeout(mhi_cntrl->state_event,
			   MHI_IN_MISSION_MODE(mhi_cntrl->ee) ||
			   MHI_PM_IN_ERROR_STATE(mhi_cntrl->pm_state),
			   msecs_to_jiffies(mhi_cntrl->timeout_ms));

	ret = (MHI_IN_MISSION_MODE(mhi_cntrl->ee)) ? 0 : -ETIMEDOUT;
	if (ret)
		mhi_power_down(mhi_cntrl, false);

	return ret;
}
EXPORT_SYMBOL(mhi_sync_power_up);

int mhi_force_rddm_mode(struct mhi_controller *mhi_cntrl)
{
	struct device *dev = &mhi_cntrl->mhi_dev->dev;
	int ret;

	 
	if (mhi_cntrl->ee == MHI_EE_RDDM)
		return 0;

	dev_dbg(dev, "Triggering SYS_ERR to force RDDM state\n");
	mhi_set_mhi_state(mhi_cntrl, MHI_STATE_SYS_ERR);

	 
	ret = wait_event_timeout(mhi_cntrl->state_event,
				 mhi_cntrl->ee == MHI_EE_RDDM,
				 msecs_to_jiffies(mhi_cntrl->timeout_ms));
	ret = ret ? 0 : -EIO;

	return ret;
}
EXPORT_SYMBOL_GPL(mhi_force_rddm_mode);

void mhi_device_get(struct mhi_device *mhi_dev)
{
	struct mhi_controller *mhi_cntrl = mhi_dev->mhi_cntrl;

	mhi_dev->dev_wake++;
	read_lock_bh(&mhi_cntrl->pm_lock);
	if (MHI_PM_IN_SUSPEND_STATE(mhi_cntrl->pm_state))
		mhi_trigger_resume(mhi_cntrl);

	mhi_cntrl->wake_get(mhi_cntrl, true);
	read_unlock_bh(&mhi_cntrl->pm_lock);
}
EXPORT_SYMBOL_GPL(mhi_device_get);

int mhi_device_get_sync(struct mhi_device *mhi_dev)
{
	struct mhi_controller *mhi_cntrl = mhi_dev->mhi_cntrl;
	int ret;

	ret = __mhi_device_get_sync(mhi_cntrl);
	if (!ret)
		mhi_dev->dev_wake++;

	return ret;
}
EXPORT_SYMBOL_GPL(mhi_device_get_sync);

void mhi_device_put(struct mhi_device *mhi_dev)
{
	struct mhi_controller *mhi_cntrl = mhi_dev->mhi_cntrl;

	mhi_dev->dev_wake--;
	read_lock_bh(&mhi_cntrl->pm_lock);
	if (MHI_PM_IN_SUSPEND_STATE(mhi_cntrl->pm_state))
		mhi_trigger_resume(mhi_cntrl);

	mhi_cntrl->wake_put(mhi_cntrl, false);
	read_unlock_bh(&mhi_cntrl->pm_lock);
}
EXPORT_SYMBOL_GPL(mhi_device_put);
