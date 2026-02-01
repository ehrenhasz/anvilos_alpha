
 
#include <linux/delay.h>
#include <linux/iopoll.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include "adf_accel_devices.h"
#include "adf_common_drv.h"
#include "adf_gen2_pfvf.h"
#include "adf_pfvf_msg.h"
#include "adf_pfvf_pf_proto.h"
#include "adf_pfvf_vf_proto.h"
#include "adf_pfvf_utils.h"

  
#define ADF_GEN2_VF_MSK			0xFFFF
#define ADF_GEN2_ERR_REG_VF2PF(vf_src)	(((vf_src) & 0x01FFFE00) >> 9)
#define ADF_GEN2_ERR_MSK_VF2PF(vf_mask)	(((vf_mask) & ADF_GEN2_VF_MSK) << 9)

#define ADF_GEN2_PF_PF2VF_OFFSET(i)	(0x3A000 + 0x280 + ((i) * 0x04))
#define ADF_GEN2_VF_PF2VF_OFFSET	0x200

#define ADF_GEN2_CSR_IN_USE		0x6AC2
#define ADF_GEN2_CSR_IN_USE_MASK	0xFFFE

enum gen2_csr_pos {
	ADF_GEN2_CSR_PF2VF_OFFSET	=  0,
	ADF_GEN2_CSR_VF2PF_OFFSET	= 16,
};

#define ADF_PFVF_GEN2_MSGTYPE_SHIFT	2
#define ADF_PFVF_GEN2_MSGTYPE_MASK	0x0F
#define ADF_PFVF_GEN2_MSGDATA_SHIFT	6
#define ADF_PFVF_GEN2_MSGDATA_MASK	0x3FF

static const struct pfvf_csr_format csr_gen2_fmt = {
	{ ADF_PFVF_GEN2_MSGTYPE_SHIFT, ADF_PFVF_GEN2_MSGTYPE_MASK },
	{ ADF_PFVF_GEN2_MSGDATA_SHIFT, ADF_PFVF_GEN2_MSGDATA_MASK },
};

#define ADF_PFVF_MSG_RETRY_DELAY	5
#define ADF_PFVF_MSG_MAX_RETRIES	3

static u32 adf_gen2_pf_get_pfvf_offset(u32 i)
{
	return ADF_GEN2_PF_PF2VF_OFFSET(i);
}

static u32 adf_gen2_vf_get_pfvf_offset(u32 i)
{
	return ADF_GEN2_VF_PF2VF_OFFSET;
}

static void adf_gen2_enable_vf2pf_interrupts(void __iomem *pmisc_addr, u32 vf_mask)
{
	 
	if (vf_mask & ADF_GEN2_VF_MSK) {
		u32 val = ADF_CSR_RD(pmisc_addr, ADF_GEN2_ERRMSK3)
			  & ~ADF_GEN2_ERR_MSK_VF2PF(vf_mask);
		ADF_CSR_WR(pmisc_addr, ADF_GEN2_ERRMSK3, val);
	}
}

static void adf_gen2_disable_all_vf2pf_interrupts(void __iomem *pmisc_addr)
{
	 
	u32 val = ADF_CSR_RD(pmisc_addr, ADF_GEN2_ERRMSK3)
		  | ADF_GEN2_ERR_MSK_VF2PF(ADF_GEN2_VF_MSK);
	ADF_CSR_WR(pmisc_addr, ADF_GEN2_ERRMSK3, val);
}

static u32 adf_gen2_disable_pending_vf2pf_interrupts(void __iomem *pmisc_addr)
{
	u32 sources, disabled, pending;
	u32 errsou3, errmsk3;

	 
	errsou3 = ADF_CSR_RD(pmisc_addr, ADF_GEN2_ERRSOU3);
	sources = ADF_GEN2_ERR_REG_VF2PF(errsou3);

	if (!sources)
		return 0;

	 
	errmsk3 = ADF_CSR_RD(pmisc_addr, ADF_GEN2_ERRMSK3);
	disabled = ADF_GEN2_ERR_REG_VF2PF(errmsk3);

	pending = sources & ~disabled;
	if (!pending)
		return 0;

	 
	errmsk3 |= ADF_GEN2_ERR_MSK_VF2PF(ADF_GEN2_VF_MSK);
	ADF_CSR_WR(pmisc_addr, ADF_GEN2_ERRMSK3, errmsk3);

	errmsk3 &= ADF_GEN2_ERR_MSK_VF2PF(sources | disabled);
	ADF_CSR_WR(pmisc_addr, ADF_GEN2_ERRMSK3, errmsk3);

	 
	return pending;
}

static u32 gen2_csr_get_int_bit(enum gen2_csr_pos offset)
{
	return ADF_PFVF_INT << offset;
}

static u32 gen2_csr_msg_to_position(u32 csr_msg, enum gen2_csr_pos offset)
{
	return (csr_msg & 0xFFFF) << offset;
}

static u32 gen2_csr_msg_from_position(u32 csr_val, enum gen2_csr_pos offset)
{
	return (csr_val >> offset) & 0xFFFF;
}

static bool gen2_csr_is_in_use(u32 msg, enum gen2_csr_pos offset)
{
	return ((msg >> offset) & ADF_GEN2_CSR_IN_USE_MASK) == ADF_GEN2_CSR_IN_USE;
}

static void gen2_csr_clear_in_use(u32 *msg, enum gen2_csr_pos offset)
{
	*msg &= ~(ADF_GEN2_CSR_IN_USE_MASK << offset);
}

static void gen2_csr_set_in_use(u32 *msg, enum gen2_csr_pos offset)
{
	*msg |= (ADF_GEN2_CSR_IN_USE << offset);
}

static bool is_legacy_user_pfvf_message(u32 msg)
{
	return !(msg & ADF_PFVF_MSGORIGIN_SYSTEM);
}

static bool is_pf2vf_notification(u8 msg_type)
{
	switch (msg_type) {
	case ADF_PF2VF_MSGTYPE_RESTARTING:
		return true;
	default:
		return false;
	}
}

static bool is_vf2pf_notification(u8 msg_type)
{
	switch (msg_type) {
	case ADF_VF2PF_MSGTYPE_INIT:
	case ADF_VF2PF_MSGTYPE_SHUTDOWN:
		return true;
	default:
		return false;
	}
}

struct pfvf_gen2_params {
	u32 pfvf_offset;
	struct mutex *csr_lock;  
	enum gen2_csr_pos local_offset;
	enum gen2_csr_pos remote_offset;
	bool (*is_notification_message)(u8 msg_type);
	u8 compat_ver;
};

static int adf_gen2_pfvf_send(struct adf_accel_dev *accel_dev,
			      struct pfvf_message msg,
			      struct pfvf_gen2_params *params)
{
	void __iomem *pmisc_addr = adf_get_pmisc_base(accel_dev);
	enum gen2_csr_pos remote_offset = params->remote_offset;
	enum gen2_csr_pos local_offset = params->local_offset;
	unsigned int retries = ADF_PFVF_MSG_MAX_RETRIES;
	struct mutex *lock = params->csr_lock;
	u32 pfvf_offset = params->pfvf_offset;
	u32 int_bit;
	u32 csr_val;
	u32 csr_msg;
	int ret;

	 

	int_bit = gen2_csr_get_int_bit(local_offset);

	csr_msg = adf_pfvf_csr_msg_of(accel_dev, msg, &csr_gen2_fmt);
	if (unlikely(!csr_msg))
		return -EINVAL;

	 
	csr_msg = gen2_csr_msg_to_position(csr_msg, local_offset);
	gen2_csr_set_in_use(&csr_msg, remote_offset);

	mutex_lock(lock);

start:
	 
	csr_val = ADF_CSR_RD(pmisc_addr, pfvf_offset);
	if (gen2_csr_is_in_use(csr_val, local_offset)) {
		dev_dbg(&GET_DEV(accel_dev),
			"PFVF CSR in use by remote function\n");
		goto retry;
	}

	 
	ADF_CSR_WR(pmisc_addr, pfvf_offset, csr_msg | int_bit);

	 
	ret = read_poll_timeout(ADF_CSR_RD, csr_val, !(csr_val & int_bit),
				ADF_PFVF_MSG_ACK_DELAY_US,
				ADF_PFVF_MSG_ACK_MAX_DELAY_US,
				true, pmisc_addr, pfvf_offset);
	if (unlikely(ret < 0)) {
		dev_dbg(&GET_DEV(accel_dev), "ACK not received from remote\n");
		csr_val &= ~int_bit;
	}

	 
	if (params->is_notification_message(msg.type) && csr_val != csr_msg) {
		 
		dev_err(&GET_DEV(accel_dev),
			"Collision on notification - PFVF CSR overwritten by remote function\n");
		goto retry;
	}

	 
	if (gen2_csr_is_in_use(csr_val, remote_offset)) {
		gen2_csr_clear_in_use(&csr_val, remote_offset);
		ADF_CSR_WR(pmisc_addr, pfvf_offset, csr_val);
	}

out:
	mutex_unlock(lock);
	return ret;

retry:
	if (--retries) {
		msleep(ADF_PFVF_MSG_RETRY_DELAY);
		goto start;
	} else {
		ret = -EBUSY;
		goto out;
	}
}

static struct pfvf_message adf_gen2_pfvf_recv(struct adf_accel_dev *accel_dev,
					      struct pfvf_gen2_params *params)
{
	void __iomem *pmisc_addr = adf_get_pmisc_base(accel_dev);
	enum gen2_csr_pos remote_offset = params->remote_offset;
	enum gen2_csr_pos local_offset = params->local_offset;
	u32 pfvf_offset = params->pfvf_offset;
	struct pfvf_message msg = { 0 };
	u32 int_bit;
	u32 csr_val;
	u16 csr_msg;

	int_bit = gen2_csr_get_int_bit(local_offset);

	 
	csr_val = ADF_CSR_RD(pmisc_addr, pfvf_offset);
	if (!(csr_val & int_bit)) {
		dev_info(&GET_DEV(accel_dev),
			 "Spurious PFVF interrupt, msg 0x%.8x. Ignored\n", csr_val);
		return msg;
	}

	 
	csr_msg = gen2_csr_msg_from_position(csr_val, local_offset);

	 
	if (unlikely(is_legacy_user_pfvf_message(csr_msg))) {
		dev_dbg(&GET_DEV(accel_dev),
			"Ignored non-system message (0x%.8x);\n", csr_val);
		 
		return msg;
	}

	 
	msg = adf_pfvf_message_of(accel_dev, csr_msg, &csr_gen2_fmt);

	 
	if (params->compat_ver >= ADF_PFVF_COMPAT_FAST_ACK &&
	    !params->is_notification_message(msg.type))
		gen2_csr_clear_in_use(&csr_val, remote_offset);

	 
	csr_val &= ~int_bit;
	ADF_CSR_WR(pmisc_addr, pfvf_offset, csr_val);

	return msg;
}

static int adf_gen2_pf2vf_send(struct adf_accel_dev *accel_dev, struct pfvf_message msg,
			       u32 pfvf_offset, struct mutex *csr_lock)
{
	struct pfvf_gen2_params params = {
		.csr_lock = csr_lock,
		.pfvf_offset = pfvf_offset,
		.local_offset = ADF_GEN2_CSR_PF2VF_OFFSET,
		.remote_offset = ADF_GEN2_CSR_VF2PF_OFFSET,
		.is_notification_message = is_pf2vf_notification,
	};

	return adf_gen2_pfvf_send(accel_dev, msg, &params);
}

static int adf_gen2_vf2pf_send(struct adf_accel_dev *accel_dev, struct pfvf_message msg,
			       u32 pfvf_offset, struct mutex *csr_lock)
{
	struct pfvf_gen2_params params = {
		.csr_lock = csr_lock,
		.pfvf_offset = pfvf_offset,
		.local_offset = ADF_GEN2_CSR_VF2PF_OFFSET,
		.remote_offset = ADF_GEN2_CSR_PF2VF_OFFSET,
		.is_notification_message = is_vf2pf_notification,
	};

	return adf_gen2_pfvf_send(accel_dev, msg, &params);
}

static struct pfvf_message adf_gen2_pf2vf_recv(struct adf_accel_dev *accel_dev,
					       u32 pfvf_offset, u8 compat_ver)
{
	struct pfvf_gen2_params params = {
		.pfvf_offset = pfvf_offset,
		.local_offset = ADF_GEN2_CSR_PF2VF_OFFSET,
		.remote_offset = ADF_GEN2_CSR_VF2PF_OFFSET,
		.is_notification_message = is_pf2vf_notification,
		.compat_ver = compat_ver,
	};

	return adf_gen2_pfvf_recv(accel_dev, &params);
}

static struct pfvf_message adf_gen2_vf2pf_recv(struct adf_accel_dev *accel_dev,
					       u32 pfvf_offset, u8 compat_ver)
{
	struct pfvf_gen2_params params = {
		.pfvf_offset = pfvf_offset,
		.local_offset = ADF_GEN2_CSR_VF2PF_OFFSET,
		.remote_offset = ADF_GEN2_CSR_PF2VF_OFFSET,
		.is_notification_message = is_vf2pf_notification,
		.compat_ver = compat_ver,
	};

	return adf_gen2_pfvf_recv(accel_dev, &params);
}

void adf_gen2_init_pf_pfvf_ops(struct adf_pfvf_ops *pfvf_ops)
{
	pfvf_ops->enable_comms = adf_enable_pf2vf_comms;
	pfvf_ops->get_pf2vf_offset = adf_gen2_pf_get_pfvf_offset;
	pfvf_ops->get_vf2pf_offset = adf_gen2_pf_get_pfvf_offset;
	pfvf_ops->enable_vf2pf_interrupts = adf_gen2_enable_vf2pf_interrupts;
	pfvf_ops->disable_all_vf2pf_interrupts = adf_gen2_disable_all_vf2pf_interrupts;
	pfvf_ops->disable_pending_vf2pf_interrupts = adf_gen2_disable_pending_vf2pf_interrupts;
	pfvf_ops->send_msg = adf_gen2_pf2vf_send;
	pfvf_ops->recv_msg = adf_gen2_vf2pf_recv;
}
EXPORT_SYMBOL_GPL(adf_gen2_init_pf_pfvf_ops);

void adf_gen2_init_vf_pfvf_ops(struct adf_pfvf_ops *pfvf_ops)
{
	pfvf_ops->enable_comms = adf_enable_vf2pf_comms;
	pfvf_ops->get_pf2vf_offset = adf_gen2_vf_get_pfvf_offset;
	pfvf_ops->get_vf2pf_offset = adf_gen2_vf_get_pfvf_offset;
	pfvf_ops->send_msg = adf_gen2_vf2pf_send;
	pfvf_ops->recv_msg = adf_gen2_pf2vf_recv;
}
EXPORT_SYMBOL_GPL(adf_gen2_init_vf_pfvf_ops);
