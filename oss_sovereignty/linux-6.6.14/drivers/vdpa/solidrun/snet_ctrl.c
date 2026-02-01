
 

#include <linux/iopoll.h>

#include "snet_vdpa.h"

enum snet_ctrl_opcodes {
	SNET_CTRL_OP_DESTROY = 1,
	SNET_CTRL_OP_READ_VQ_STATE,
	SNET_CTRL_OP_SUSPEND,
	SNET_CTRL_OP_RESUME,
};

#define SNET_CTRL_TIMEOUT	        2000000

#define SNET_CTRL_DATA_SIZE_MASK	0x0000FFFF
#define SNET_CTRL_IN_PROCESS_MASK	0x00010000
#define SNET_CTRL_CHUNK_RDY_MASK	0x00020000
#define SNET_CTRL_ERROR_MASK		0x0FFC0000

#define SNET_VAL_TO_ERR(val)		(-(((val) & SNET_CTRL_ERROR_MASK) >> 18))
#define SNET_EMPTY_CTRL(val)		(((val) & SNET_CTRL_ERROR_MASK) || \
						!((val) & SNET_CTRL_IN_PROCESS_MASK))
#define SNET_DATA_READY(val)		((val) & (SNET_CTRL_ERROR_MASK | SNET_CTRL_CHUNK_RDY_MASK))

 
struct snet_ctrl_reg_ctrl {
	 
	u16 data_size;
	 
	u16 in_process:1;
	 
	u16 chunk_ready:1;
	 
	u16 error:10;
	 
	u16 rsvd:4;
};

 
struct snet_ctrl_reg_op {
	u16 opcode;
	 
	u16 vq_idx;
};

struct snet_ctrl_regs {
	struct snet_ctrl_reg_op op;
	struct snet_ctrl_reg_ctrl ctrl;
	u32 rsvd;
	u32 data[];
};

static struct snet_ctrl_regs __iomem *snet_get_ctrl(struct snet *snet)
{
	return snet->bar + snet->psnet->cfg.ctrl_off;
}

static int snet_wait_for_empty_ctrl(struct snet_ctrl_regs __iomem *regs)
{
	u32 val;

	return readx_poll_timeout(ioread32, &regs->ctrl, val, SNET_EMPTY_CTRL(val), 10,
				  SNET_CTRL_TIMEOUT);
}

static int snet_wait_for_empty_op(struct snet_ctrl_regs __iomem *regs)
{
	u32 val;

	return readx_poll_timeout(ioread32, &regs->op, val, !val, 10, SNET_CTRL_TIMEOUT);
}

static int snet_wait_for_data(struct snet_ctrl_regs __iomem *regs)
{
	u32 val;

	return readx_poll_timeout(ioread32, &regs->ctrl, val, SNET_DATA_READY(val), 10,
				  SNET_CTRL_TIMEOUT);
}

static u32 snet_read32_word(struct snet_ctrl_regs __iomem *ctrl_regs, u16 word_idx)
{
	return ioread32(&ctrl_regs->data[word_idx]);
}

static u32 snet_read_ctrl(struct snet_ctrl_regs __iomem *ctrl_regs)
{
	return ioread32(&ctrl_regs->ctrl);
}

static void snet_write_ctrl(struct snet_ctrl_regs __iomem *ctrl_regs, u32 val)
{
	iowrite32(val, &ctrl_regs->ctrl);
}

static void snet_write_op(struct snet_ctrl_regs __iomem *ctrl_regs, u32 val)
{
	iowrite32(val, &ctrl_regs->op);
}

static int snet_wait_for_dpu_completion(struct snet_ctrl_regs __iomem *ctrl_regs)
{
	 
	return snet_wait_for_empty_op(ctrl_regs);
}

 
static int snet_ctrl_read_from_dpu(struct snet *snet, u16 opcode, u16 vq_idx, void *buffer,
				   u32 buf_size)
{
	struct pci_dev *pdev = snet->pdev;
	struct snet_ctrl_regs __iomem *regs = snet_get_ctrl(snet);
	u32 *bfr_ptr = (u32 *)buffer;
	u32 val;
	u16 buf_words;
	int ret;
	u16 words, i, tot_words = 0;

	 
	if (!SNET_CFG_VER(snet, 2))
		return -EOPNOTSUPP;

	if (!IS_ALIGNED(buf_size, 4))
		return -EINVAL;

	mutex_lock(&snet->ctrl_lock);

	buf_words = buf_size / 4;

	 
	ret = snet_wait_for_empty_ctrl(regs);
	if (ret) {
		SNET_WARN(pdev, "Timeout waiting for previous control data to be consumed\n");
		goto exit;
	}

	 
	spin_lock(&snet->ctrl_spinlock);

	snet_write_ctrl(regs, buf_words);
	snet_write_op(regs, opcode | (vq_idx << 16));

	spin_unlock(&snet->ctrl_spinlock);

	while (buf_words != tot_words) {
		ret = snet_wait_for_data(regs);
		if (ret) {
			SNET_WARN(pdev, "Timeout waiting for control data\n");
			goto exit;
		}

		val = snet_read_ctrl(regs);

		 
		if (val & SNET_CTRL_ERROR_MASK) {
			ret = SNET_VAL_TO_ERR(val);
			SNET_WARN(pdev, "Error while reading control data from DPU, err %d\n", ret);
			goto exit;
		}

		words = min_t(u16, val & SNET_CTRL_DATA_SIZE_MASK, buf_words - tot_words);

		for (i = 0; i < words; i++) {
			*bfr_ptr = snet_read32_word(regs, i);
			bfr_ptr++;
		}

		tot_words += words;

		 
		if (!(val & SNET_CTRL_IN_PROCESS_MASK))
			break;

		 
		val &= ~SNET_CTRL_CHUNK_RDY_MASK;
		snet_write_ctrl(regs, val);
	}

	ret = snet_wait_for_dpu_completion(regs);
	if (ret)
		SNET_WARN(pdev, "Timeout waiting for the DPU to complete a control command\n");

exit:
	mutex_unlock(&snet->ctrl_lock);
	return ret;
}

 
static int snet_send_ctrl_msg_old(struct snet *snet, u32 opcode)
{
	struct pci_dev *pdev = snet->pdev;
	struct snet_ctrl_regs __iomem *regs = snet_get_ctrl(snet);
	int ret;

	mutex_lock(&snet->ctrl_lock);

	 
	ret = snet_wait_for_empty_op(regs);
	if (ret) {
		SNET_WARN(pdev, "Timeout waiting for previous control message to be ACKed\n");
		goto exit;
	}

	 
	snet_write_op(regs, opcode);

	 
	ret = snet_wait_for_empty_op(regs);
	if (ret)
		SNET_WARN(pdev, "Timeout waiting for a control message to be ACKed\n");

exit:
	mutex_unlock(&snet->ctrl_lock);
	return ret;
}

 
static int snet_send_ctrl_msg(struct snet *snet, u16 opcode, u16 vq_idx)
{
	struct pci_dev *pdev = snet->pdev;
	struct snet_ctrl_regs __iomem *regs = snet_get_ctrl(snet);
	u32 val;
	int ret;

	 
	if (!SNET_CFG_VER(snet, 2))
		return snet_send_ctrl_msg_old(snet, opcode);

	mutex_lock(&snet->ctrl_lock);

	 
	ret = snet_wait_for_empty_ctrl(regs);
	if (ret) {
		SNET_WARN(pdev, "Timeout waiting for previous control data to be consumed\n");
		goto exit;
	}

	 
	spin_lock(&snet->ctrl_spinlock);

	snet_write_ctrl(regs, 0);
	snet_write_op(regs, opcode | (vq_idx << 16));

	spin_unlock(&snet->ctrl_spinlock);

	 
	ret = snet_wait_for_data(regs);
	if (ret) {
		SNET_WARN(pdev, "Timeout waiting for control message to be ACKed\n");
		goto exit;
	}

	 
	val = snet_read_ctrl(regs);
	ret = SNET_VAL_TO_ERR(val);

	 
	val &= ~SNET_CTRL_CHUNK_RDY_MASK;
	snet_write_ctrl(regs, val);

	ret = snet_wait_for_dpu_completion(regs);
	if (ret)
		SNET_WARN(pdev, "Timeout waiting for DPU to complete a control command, err %d\n",
			  ret);

exit:
	mutex_unlock(&snet->ctrl_lock);
	return ret;
}

void snet_ctrl_clear(struct snet *snet)
{
	struct snet_ctrl_regs __iomem *regs = snet_get_ctrl(snet);

	snet_write_op(regs, 0);
}

int snet_destroy_dev(struct snet *snet)
{
	return snet_send_ctrl_msg(snet, SNET_CTRL_OP_DESTROY, 0);
}

int snet_read_vq_state(struct snet *snet, u16 idx, struct vdpa_vq_state *state)
{
	return snet_ctrl_read_from_dpu(snet, SNET_CTRL_OP_READ_VQ_STATE, idx, state,
				       sizeof(*state));
}

int snet_suspend_dev(struct snet *snet)
{
	return snet_send_ctrl_msg(snet, SNET_CTRL_OP_SUSPEND, 0);
}

int snet_resume_dev(struct snet *snet)
{
	return snet_send_ctrl_msg(snet, SNET_CTRL_OP_RESUME, 0);
}
