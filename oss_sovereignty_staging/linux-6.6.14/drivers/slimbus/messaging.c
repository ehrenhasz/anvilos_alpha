
 

#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include "slimbus.h"

 
void slim_msg_response(struct slim_controller *ctrl, u8 *reply, u8 tid, u8 len)
{
	struct slim_msg_txn *txn;
	struct slim_val_inf *msg;
	unsigned long flags;

	spin_lock_irqsave(&ctrl->txn_lock, flags);
	txn = idr_find(&ctrl->tid_idr, tid);
	spin_unlock_irqrestore(&ctrl->txn_lock, flags);

	if (txn == NULL)
		return;

	msg = txn->msg;
	if (msg == NULL || msg->rbuf == NULL) {
		dev_err(ctrl->dev, "Got response to invalid TID:%d, len:%d\n",
				tid, len);
		return;
	}

	slim_free_txn_tid(ctrl, txn);
	memcpy(msg->rbuf, reply, len);
	if (txn->comp)
		complete(txn->comp);

	 
	pm_runtime_mark_last_busy(ctrl->dev);
	pm_runtime_put_autosuspend(ctrl->dev);
}
EXPORT_SYMBOL_GPL(slim_msg_response);

 
int slim_alloc_txn_tid(struct slim_controller *ctrl, struct slim_msg_txn *txn)
{
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&ctrl->txn_lock, flags);
	ret = idr_alloc_cyclic(&ctrl->tid_idr, txn, 1,
				SLIM_MAX_TIDS, GFP_ATOMIC);
	if (ret < 0) {
		spin_unlock_irqrestore(&ctrl->txn_lock, flags);
		return ret;
	}
	txn->tid = ret;
	spin_unlock_irqrestore(&ctrl->txn_lock, flags);
	return 0;
}
EXPORT_SYMBOL_GPL(slim_alloc_txn_tid);

 
void slim_free_txn_tid(struct slim_controller *ctrl, struct slim_msg_txn *txn)
{
	unsigned long flags;

	spin_lock_irqsave(&ctrl->txn_lock, flags);
	idr_remove(&ctrl->tid_idr, txn->tid);
	spin_unlock_irqrestore(&ctrl->txn_lock, flags);
}
EXPORT_SYMBOL_GPL(slim_free_txn_tid);

 
int slim_do_transfer(struct slim_controller *ctrl, struct slim_msg_txn *txn)
{
	DECLARE_COMPLETION_ONSTACK(done);
	bool need_tid = false, clk_pause_msg = false;
	int ret, timeout;

	 
	if (ctrl->sched.clk_state == SLIM_CLK_ENTERING_PAUSE &&
		(txn->mt == SLIM_MSG_MT_CORE &&
		 txn->mc >= SLIM_MSG_MC_BEGIN_RECONFIGURATION &&
		 txn->mc <= SLIM_MSG_MC_RECONFIGURE_NOW))
		clk_pause_msg = true;

	if (!clk_pause_msg) {
		ret = pm_runtime_get_sync(ctrl->dev);
		if (ctrl->sched.clk_state != SLIM_CLK_ACTIVE) {
			dev_err(ctrl->dev, "ctrl wrong state:%d, ret:%d\n",
				ctrl->sched.clk_state, ret);
			goto slim_xfer_err;
		}
	}
	 
	txn->tid = 0;
	need_tid = slim_tid_txn(txn->mt, txn->mc);

	if (need_tid) {
		ret = slim_alloc_txn_tid(ctrl, txn);
		if (ret)
			return ret;

		if (!txn->msg->comp)
			txn->comp = &done;
		else
			txn->comp = txn->comp;
	}

	ret = ctrl->xfer_msg(ctrl, txn);

	if (!ret && need_tid && !txn->msg->comp) {
		unsigned long ms = txn->rl + HZ;

		timeout = wait_for_completion_timeout(txn->comp,
						      msecs_to_jiffies(ms));
		if (!timeout) {
			ret = -ETIMEDOUT;
			slim_free_txn_tid(ctrl, txn);
		}
	}

	if (ret)
		dev_err(ctrl->dev, "Tx:MT:0x%x, MC:0x%x, LA:0x%x failed:%d\n",
			txn->mt, txn->mc, txn->la, ret);

slim_xfer_err:
	if (!clk_pause_msg && (txn->tid == 0  || ret == -ETIMEDOUT)) {
		 
		pm_runtime_mark_last_busy(ctrl->dev);
		pm_runtime_put_autosuspend(ctrl->dev);
	}
	return ret;
}
EXPORT_SYMBOL_GPL(slim_do_transfer);

static int slim_val_inf_sanity(struct slim_controller *ctrl,
			       struct slim_val_inf *msg, u8 mc)
{
	if (!msg || msg->num_bytes > 16 ||
	    (msg->start_offset + msg->num_bytes) > 0xC00)
		goto reterr;
	switch (mc) {
	case SLIM_MSG_MC_REQUEST_VALUE:
	case SLIM_MSG_MC_REQUEST_INFORMATION:
		if (msg->rbuf != NULL)
			return 0;
		break;

	case SLIM_MSG_MC_CHANGE_VALUE:
	case SLIM_MSG_MC_CLEAR_INFORMATION:
		if (msg->wbuf != NULL)
			return 0;
		break;

	case SLIM_MSG_MC_REQUEST_CHANGE_VALUE:
	case SLIM_MSG_MC_REQUEST_CLEAR_INFORMATION:
		if (msg->rbuf != NULL && msg->wbuf != NULL)
			return 0;
		break;
	}
reterr:
	if (msg)
		dev_err(ctrl->dev, "Sanity check failed:msg:offset:0x%x, mc:%d\n",
			msg->start_offset, mc);
	return -EINVAL;
}

static u16 slim_slicesize(int code)
{
	static const u8 sizetocode[16] = {
		0, 1, 2, 3, 3, 4, 4, 5, 5, 5, 5, 6, 6, 6, 6, 7
	};

	code = clamp(code, 1, (int)ARRAY_SIZE(sizetocode));

	return sizetocode[code - 1];
}

 
int slim_xfer_msg(struct slim_device *sbdev, struct slim_val_inf *msg,
		  u8 mc)
{
	DEFINE_SLIM_LDEST_TXN(txn_stack, mc, 6, sbdev->laddr, msg);
	struct slim_msg_txn *txn = &txn_stack;
	struct slim_controller *ctrl = sbdev->ctrl;
	int ret;
	u16 sl;

	if (!ctrl)
		return -EINVAL;

	ret = slim_val_inf_sanity(ctrl, msg, mc);
	if (ret)
		return ret;

	sl = slim_slicesize(msg->num_bytes);

	dev_dbg(ctrl->dev, "SB xfer msg:os:%x, len:%d, MC:%x, sl:%x\n",
		msg->start_offset, msg->num_bytes, mc, sl);

	txn->ec = ((sl | (1 << 3)) | ((msg->start_offset & 0xFFF) << 4));

	switch (mc) {
	case SLIM_MSG_MC_REQUEST_CHANGE_VALUE:
	case SLIM_MSG_MC_CHANGE_VALUE:
	case SLIM_MSG_MC_REQUEST_CLEAR_INFORMATION:
	case SLIM_MSG_MC_CLEAR_INFORMATION:
		txn->rl += msg->num_bytes;
		break;
	default:
		break;
	}

	if (slim_tid_txn(txn->mt, txn->mc))
		txn->rl++;

	return slim_do_transfer(ctrl, txn);
}
EXPORT_SYMBOL_GPL(slim_xfer_msg);

static void slim_fill_msg(struct slim_val_inf *msg, u32 addr,
			 size_t count, u8 *rbuf, u8 *wbuf)
{
	msg->start_offset = addr;
	msg->num_bytes = count;
	msg->rbuf = rbuf;
	msg->wbuf = wbuf;
	msg->comp = NULL;
}

 
int slim_read(struct slim_device *sdev, u32 addr, size_t count, u8 *val)
{
	struct slim_val_inf msg;

	slim_fill_msg(&msg, addr, count, val, NULL);

	return slim_xfer_msg(sdev, &msg, SLIM_MSG_MC_REQUEST_VALUE);
}
EXPORT_SYMBOL_GPL(slim_read);

 
int slim_readb(struct slim_device *sdev, u32 addr)
{
	int ret;
	u8 buf;

	ret = slim_read(sdev, addr, 1, &buf);
	if (ret < 0)
		return ret;
	else
		return buf;
}
EXPORT_SYMBOL_GPL(slim_readb);

 
int slim_write(struct slim_device *sdev, u32 addr, size_t count, u8 *val)
{
	struct slim_val_inf msg;

	slim_fill_msg(&msg, addr, count,  NULL, val);

	return slim_xfer_msg(sdev, &msg, SLIM_MSG_MC_CHANGE_VALUE);
}
EXPORT_SYMBOL_GPL(slim_write);

 
int slim_writeb(struct slim_device *sdev, u32 addr, u8 value)
{
	return slim_write(sdev, addr, 1, &value);
}
EXPORT_SYMBOL_GPL(slim_writeb);
