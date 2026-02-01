
 

#include <linux/blkdev.h>
#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/iopoll.h>
#include <linux/ioport.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/prandom.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/sd.h>
#include <linux/mmc/sdio.h>
#include <linux/bitops.h>
#include <linux/regulator/consumer.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/mmc/slot-gpio.h>

#include "dw_mmc.h"

 
#define DW_MCI_DATA_ERROR_FLAGS	(SDMMC_INT_DRTO | SDMMC_INT_DCRC | \
				 SDMMC_INT_HTO | SDMMC_INT_SBE  | \
				 SDMMC_INT_EBE | SDMMC_INT_HLE)
#define DW_MCI_CMD_ERROR_FLAGS	(SDMMC_INT_RTO | SDMMC_INT_RCRC | \
				 SDMMC_INT_RESP_ERR | SDMMC_INT_HLE)
#define DW_MCI_ERROR_FLAGS	(DW_MCI_DATA_ERROR_FLAGS | \
				 DW_MCI_CMD_ERROR_FLAGS)
#define DW_MCI_SEND_STATUS	1
#define DW_MCI_RECV_STATUS	2
#define DW_MCI_DMA_THRESHOLD	16

#define DW_MCI_FREQ_MAX	200000000	 
#define DW_MCI_FREQ_MIN	100000		 

#define IDMAC_INT_CLR		(SDMMC_IDMAC_INT_AI | SDMMC_IDMAC_INT_NI | \
				 SDMMC_IDMAC_INT_CES | SDMMC_IDMAC_INT_DU | \
				 SDMMC_IDMAC_INT_FBE | SDMMC_IDMAC_INT_RI | \
				 SDMMC_IDMAC_INT_TI)

#define DESC_RING_BUF_SZ	PAGE_SIZE

struct idmac_desc_64addr {
	u32		des0;	 
#define IDMAC_OWN_CLR64(x) \
	!((x) & cpu_to_le32(IDMAC_DES0_OWN))

	u32		des1;	 

	u32		des2;	 
#define IDMAC_64ADDR_SET_BUFFER1_SIZE(d, s) \
	((d)->des2 = ((d)->des2 & cpu_to_le32(0x03ffe000)) | \
	 ((cpu_to_le32(s)) & cpu_to_le32(0x1fff)))

	u32		des3;	 

	u32		des4;	 
	u32		des5;	 

	u32		des6;	 
	u32		des7;	 
};

struct idmac_desc {
	__le32		des0;	 
#define IDMAC_DES0_DIC	BIT(1)
#define IDMAC_DES0_LD	BIT(2)
#define IDMAC_DES0_FD	BIT(3)
#define IDMAC_DES0_CH	BIT(4)
#define IDMAC_DES0_ER	BIT(5)
#define IDMAC_DES0_CES	BIT(30)
#define IDMAC_DES0_OWN	BIT(31)

	__le32		des1;	 
#define IDMAC_SET_BUFFER1_SIZE(d, s) \
	((d)->des1 = ((d)->des1 & cpu_to_le32(0x03ffe000)) | (cpu_to_le32((s) & 0x1fff)))

	__le32		des2;	 

	__le32		des3;	 
};

 
#define DW_MCI_DESC_DATA_LENGTH	0x1000

#if defined(CONFIG_DEBUG_FS)
static int dw_mci_req_show(struct seq_file *s, void *v)
{
	struct dw_mci_slot *slot = s->private;
	struct mmc_request *mrq;
	struct mmc_command *cmd;
	struct mmc_command *stop;
	struct mmc_data	*data;

	 
	spin_lock_bh(&slot->host->lock);
	mrq = slot->mrq;

	if (mrq) {
		cmd = mrq->cmd;
		data = mrq->data;
		stop = mrq->stop;

		if (cmd)
			seq_printf(s,
				   "CMD%u(0x%x) flg %x rsp %x %x %x %x err %d\n",
				   cmd->opcode, cmd->arg, cmd->flags,
				   cmd->resp[0], cmd->resp[1], cmd->resp[2],
				   cmd->resp[2], cmd->error);
		if (data)
			seq_printf(s, "DATA %u / %u * %u flg %x err %d\n",
				   data->bytes_xfered, data->blocks,
				   data->blksz, data->flags, data->error);
		if (stop)
			seq_printf(s,
				   "CMD%u(0x%x) flg %x rsp %x %x %x %x err %d\n",
				   stop->opcode, stop->arg, stop->flags,
				   stop->resp[0], stop->resp[1], stop->resp[2],
				   stop->resp[2], stop->error);
	}

	spin_unlock_bh(&slot->host->lock);

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(dw_mci_req);

static int dw_mci_regs_show(struct seq_file *s, void *v)
{
	struct dw_mci *host = s->private;

	pm_runtime_get_sync(host->dev);

	seq_printf(s, "STATUS:\t0x%08x\n", mci_readl(host, STATUS));
	seq_printf(s, "RINTSTS:\t0x%08x\n", mci_readl(host, RINTSTS));
	seq_printf(s, "CMD:\t0x%08x\n", mci_readl(host, CMD));
	seq_printf(s, "CTRL:\t0x%08x\n", mci_readl(host, CTRL));
	seq_printf(s, "INTMASK:\t0x%08x\n", mci_readl(host, INTMASK));
	seq_printf(s, "CLKENA:\t0x%08x\n", mci_readl(host, CLKENA));

	pm_runtime_put_autosuspend(host->dev);

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(dw_mci_regs);

static void dw_mci_init_debugfs(struct dw_mci_slot *slot)
{
	struct mmc_host	*mmc = slot->mmc;
	struct dw_mci *host = slot->host;
	struct dentry *root;

	root = mmc->debugfs_root;
	if (!root)
		return;

	debugfs_create_file("regs", S_IRUSR, root, host, &dw_mci_regs_fops);
	debugfs_create_file("req", S_IRUSR, root, slot, &dw_mci_req_fops);
	debugfs_create_u32("state", S_IRUSR, root, &host->state);
	debugfs_create_xul("pending_events", S_IRUSR, root,
			   &host->pending_events);
	debugfs_create_xul("completed_events", S_IRUSR, root,
			   &host->completed_events);
#ifdef CONFIG_FAULT_INJECTION
	fault_create_debugfs_attr("fail_data_crc", root, &host->fail_data_crc);
#endif
}
#endif  

static bool dw_mci_ctrl_reset(struct dw_mci *host, u32 reset)
{
	u32 ctrl;

	ctrl = mci_readl(host, CTRL);
	ctrl |= reset;
	mci_writel(host, CTRL, ctrl);

	 
	if (readl_poll_timeout_atomic(host->regs + SDMMC_CTRL, ctrl,
				      !(ctrl & reset),
				      1, 500 * USEC_PER_MSEC)) {
		dev_err(host->dev,
			"Timeout resetting block (ctrl reset %#x)\n",
			ctrl & reset);
		return false;
	}

	return true;
}

static void dw_mci_wait_while_busy(struct dw_mci *host, u32 cmd_flags)
{
	u32 status;

	 
	if ((cmd_flags & SDMMC_CMD_PRV_DAT_WAIT) &&
	    !(cmd_flags & SDMMC_CMD_VOLT_SWITCH)) {
		if (readl_poll_timeout_atomic(host->regs + SDMMC_STATUS,
					      status,
					      !(status & SDMMC_STATUS_BUSY),
					      10, 500 * USEC_PER_MSEC))
			dev_err(host->dev, "Busy; trying anyway\n");
	}
}

static void mci_send_cmd(struct dw_mci_slot *slot, u32 cmd, u32 arg)
{
	struct dw_mci *host = slot->host;
	unsigned int cmd_status = 0;

	mci_writel(host, CMDARG, arg);
	wmb();  
	dw_mci_wait_while_busy(host, cmd);
	mci_writel(host, CMD, SDMMC_CMD_START | cmd);

	if (readl_poll_timeout_atomic(host->regs + SDMMC_CMD, cmd_status,
				      !(cmd_status & SDMMC_CMD_START),
				      1, 500 * USEC_PER_MSEC))
		dev_err(&slot->mmc->class_dev,
			"Timeout sending command (cmd %#x arg %#x status %#x)\n",
			cmd, arg, cmd_status);
}

static u32 dw_mci_prepare_command(struct mmc_host *mmc, struct mmc_command *cmd)
{
	struct dw_mci_slot *slot = mmc_priv(mmc);
	struct dw_mci *host = slot->host;
	u32 cmdr;

	cmd->error = -EINPROGRESS;
	cmdr = cmd->opcode;

	if (cmd->opcode == MMC_STOP_TRANSMISSION ||
	    cmd->opcode == MMC_GO_IDLE_STATE ||
	    cmd->opcode == MMC_GO_INACTIVE_STATE ||
	    (cmd->opcode == SD_IO_RW_DIRECT &&
	     ((cmd->arg >> 9) & 0x1FFFF) == SDIO_CCCR_ABORT))
		cmdr |= SDMMC_CMD_STOP;
	else if (cmd->opcode != MMC_SEND_STATUS && cmd->data)
		cmdr |= SDMMC_CMD_PRV_DAT_WAIT;

	if (cmd->opcode == SD_SWITCH_VOLTAGE) {
		u32 clk_en_a;

		 
		cmdr |= SDMMC_CMD_VOLT_SWITCH;

		 
		WARN_ON(slot->host->state != STATE_SENDING_CMD);
		slot->host->state = STATE_SENDING_CMD11;

		 
		clk_en_a = mci_readl(host, CLKENA);
		clk_en_a &= ~(SDMMC_CLKEN_LOW_PWR << slot->id);
		mci_writel(host, CLKENA, clk_en_a);
		mci_send_cmd(slot, SDMMC_CMD_UPD_CLK |
			     SDMMC_CMD_PRV_DAT_WAIT, 0);
	}

	if (cmd->flags & MMC_RSP_PRESENT) {
		 
		cmdr |= SDMMC_CMD_RESP_EXP;
		if (cmd->flags & MMC_RSP_136)
			cmdr |= SDMMC_CMD_RESP_LONG;
	}

	if (cmd->flags & MMC_RSP_CRC)
		cmdr |= SDMMC_CMD_RESP_CRC;

	if (cmd->data) {
		cmdr |= SDMMC_CMD_DAT_EXP;
		if (cmd->data->flags & MMC_DATA_WRITE)
			cmdr |= SDMMC_CMD_DAT_WR;
	}

	if (!test_bit(DW_MMC_CARD_NO_USE_HOLD, &slot->flags))
		cmdr |= SDMMC_CMD_USE_HOLD_REG;

	return cmdr;
}

static u32 dw_mci_prep_stop_abort(struct dw_mci *host, struct mmc_command *cmd)
{
	struct mmc_command *stop;
	u32 cmdr;

	if (!cmd->data)
		return 0;

	stop = &host->stop_abort;
	cmdr = cmd->opcode;
	memset(stop, 0, sizeof(struct mmc_command));

	if (cmdr == MMC_READ_SINGLE_BLOCK ||
	    cmdr == MMC_READ_MULTIPLE_BLOCK ||
	    cmdr == MMC_WRITE_BLOCK ||
	    cmdr == MMC_WRITE_MULTIPLE_BLOCK ||
	    mmc_op_tuning(cmdr) ||
	    cmdr == MMC_GEN_CMD) {
		stop->opcode = MMC_STOP_TRANSMISSION;
		stop->arg = 0;
		stop->flags = MMC_RSP_R1B | MMC_CMD_AC;
	} else if (cmdr == SD_IO_RW_EXTENDED) {
		stop->opcode = SD_IO_RW_DIRECT;
		stop->arg |= (1 << 31) | (0 << 28) | (SDIO_CCCR_ABORT << 9) |
			     ((cmd->arg >> 28) & 0x7);
		stop->flags = MMC_RSP_SPI_R5 | MMC_RSP_R5 | MMC_CMD_AC;
	} else {
		return 0;
	}

	cmdr = stop->opcode | SDMMC_CMD_STOP |
		SDMMC_CMD_RESP_CRC | SDMMC_CMD_RESP_EXP;

	if (!test_bit(DW_MMC_CARD_NO_USE_HOLD, &host->slot->flags))
		cmdr |= SDMMC_CMD_USE_HOLD_REG;

	return cmdr;
}

static inline void dw_mci_set_cto(struct dw_mci *host)
{
	unsigned int cto_clks;
	unsigned int cto_div;
	unsigned int cto_ms;
	unsigned long irqflags;

	cto_clks = mci_readl(host, TMOUT) & 0xff;
	cto_div = (mci_readl(host, CLKDIV) & 0xff) * 2;
	if (cto_div == 0)
		cto_div = 1;

	cto_ms = DIV_ROUND_UP_ULL((u64)MSEC_PER_SEC * cto_clks * cto_div,
				  host->bus_hz);

	 
	cto_ms += 10;

	 
	spin_lock_irqsave(&host->irq_lock, irqflags);
	if (!test_bit(EVENT_CMD_COMPLETE, &host->pending_events))
		mod_timer(&host->cto_timer,
			jiffies + msecs_to_jiffies(cto_ms) + 1);
	spin_unlock_irqrestore(&host->irq_lock, irqflags);
}

static void dw_mci_start_command(struct dw_mci *host,
				 struct mmc_command *cmd, u32 cmd_flags)
{
	host->cmd = cmd;
	dev_vdbg(host->dev,
		 "start command: ARGR=0x%08x CMDR=0x%08x\n",
		 cmd->arg, cmd_flags);

	mci_writel(host, CMDARG, cmd->arg);
	wmb();  
	dw_mci_wait_while_busy(host, cmd_flags);

	mci_writel(host, CMD, cmd_flags | SDMMC_CMD_START);

	 
	if (cmd_flags & SDMMC_CMD_RESP_EXP)
		dw_mci_set_cto(host);
}

static inline void send_stop_abort(struct dw_mci *host, struct mmc_data *data)
{
	struct mmc_command *stop = &host->stop_abort;

	dw_mci_start_command(host, stop, host->stop_cmdr);
}

 
static void dw_mci_stop_dma(struct dw_mci *host)
{
	if (host->using_dma) {
		host->dma_ops->stop(host);
		host->dma_ops->cleanup(host);
	}

	 
	set_bit(EVENT_XFER_COMPLETE, &host->pending_events);
}

static void dw_mci_dma_cleanup(struct dw_mci *host)
{
	struct mmc_data *data = host->data;

	if (data && data->host_cookie == COOKIE_MAPPED) {
		dma_unmap_sg(host->dev,
			     data->sg,
			     data->sg_len,
			     mmc_get_dma_dir(data));
		data->host_cookie = COOKIE_UNMAPPED;
	}
}

static void dw_mci_idmac_reset(struct dw_mci *host)
{
	u32 bmod = mci_readl(host, BMOD);
	 
	bmod |= SDMMC_IDMAC_SWRESET;
	mci_writel(host, BMOD, bmod);
}

static void dw_mci_idmac_stop_dma(struct dw_mci *host)
{
	u32 temp;

	 
	temp = mci_readl(host, CTRL);
	temp &= ~SDMMC_CTRL_USE_IDMAC;
	temp |= SDMMC_CTRL_DMA_RESET;
	mci_writel(host, CTRL, temp);

	 
	temp = mci_readl(host, BMOD);
	temp &= ~(SDMMC_IDMAC_ENABLE | SDMMC_IDMAC_FB);
	temp |= SDMMC_IDMAC_SWRESET;
	mci_writel(host, BMOD, temp);
}

static void dw_mci_dmac_complete_dma(void *arg)
{
	struct dw_mci *host = arg;
	struct mmc_data *data = host->data;

	dev_vdbg(host->dev, "DMA complete\n");

	if ((host->use_dma == TRANS_MODE_EDMAC) &&
	    data && (data->flags & MMC_DATA_READ))
		 
		dma_sync_sg_for_cpu(mmc_dev(host->slot->mmc),
				    data->sg,
				    data->sg_len,
				    DMA_FROM_DEVICE);

	host->dma_ops->cleanup(host);

	 
	if (data) {
		set_bit(EVENT_XFER_COMPLETE, &host->pending_events);
		tasklet_schedule(&host->tasklet);
	}
}

static int dw_mci_idmac_init(struct dw_mci *host)
{
	int i;

	if (host->dma_64bit_address == 1) {
		struct idmac_desc_64addr *p;
		 
		host->ring_size =
			DESC_RING_BUF_SZ / sizeof(struct idmac_desc_64addr);

		 
		for (i = 0, p = host->sg_cpu; i < host->ring_size - 1;
								i++, p++) {
			p->des6 = (host->sg_dma +
					(sizeof(struct idmac_desc_64addr) *
							(i + 1))) & 0xffffffff;

			p->des7 = (u64)(host->sg_dma +
					(sizeof(struct idmac_desc_64addr) *
							(i + 1))) >> 32;
			 
			p->des0 = 0;
			p->des1 = 0;
			p->des2 = 0;
			p->des3 = 0;
		}

		 
		p->des6 = host->sg_dma & 0xffffffff;
		p->des7 = (u64)host->sg_dma >> 32;
		p->des0 = IDMAC_DES0_ER;

	} else {
		struct idmac_desc *p;
		 
		host->ring_size =
			DESC_RING_BUF_SZ / sizeof(struct idmac_desc);

		 
		for (i = 0, p = host->sg_cpu;
		     i < host->ring_size - 1;
		     i++, p++) {
			p->des3 = cpu_to_le32(host->sg_dma +
					(sizeof(struct idmac_desc) * (i + 1)));
			p->des0 = 0;
			p->des1 = 0;
		}

		 
		p->des3 = cpu_to_le32(host->sg_dma);
		p->des0 = cpu_to_le32(IDMAC_DES0_ER);
	}

	dw_mci_idmac_reset(host);

	if (host->dma_64bit_address == 1) {
		 
		mci_writel(host, IDSTS64, IDMAC_INT_CLR);
		mci_writel(host, IDINTEN64, SDMMC_IDMAC_INT_NI |
				SDMMC_IDMAC_INT_RI | SDMMC_IDMAC_INT_TI);

		 
		mci_writel(host, DBADDRL, host->sg_dma & 0xffffffff);
		mci_writel(host, DBADDRU, (u64)host->sg_dma >> 32);

	} else {
		 
		mci_writel(host, IDSTS, IDMAC_INT_CLR);
		mci_writel(host, IDINTEN, SDMMC_IDMAC_INT_NI |
				SDMMC_IDMAC_INT_RI | SDMMC_IDMAC_INT_TI);

		 
		mci_writel(host, DBADDR, host->sg_dma);
	}

	return 0;
}

static inline int dw_mci_prepare_desc64(struct dw_mci *host,
					 struct mmc_data *data,
					 unsigned int sg_len)
{
	unsigned int desc_len;
	struct idmac_desc_64addr *desc_first, *desc_last, *desc;
	u32 val;
	int i;

	desc_first = desc_last = desc = host->sg_cpu;

	for (i = 0; i < sg_len; i++) {
		unsigned int length = sg_dma_len(&data->sg[i]);

		u64 mem_addr = sg_dma_address(&data->sg[i]);

		for ( ; length ; desc++) {
			desc_len = (length <= DW_MCI_DESC_DATA_LENGTH) ?
				   length : DW_MCI_DESC_DATA_LENGTH;

			length -= desc_len;

			 
			if (readl_poll_timeout_atomic(&desc->des0, val,
						!(val & IDMAC_DES0_OWN),
						10, 100 * USEC_PER_MSEC))
				goto err_own_bit;

			 
			desc->des0 = IDMAC_DES0_OWN | IDMAC_DES0_DIC |
						IDMAC_DES0_CH;

			 
			IDMAC_64ADDR_SET_BUFFER1_SIZE(desc, desc_len);

			 
			desc->des4 = mem_addr & 0xffffffff;
			desc->des5 = mem_addr >> 32;

			 
			mem_addr += desc_len;

			 
			desc_last = desc;
		}
	}

	 
	desc_first->des0 |= IDMAC_DES0_FD;

	 
	desc_last->des0 &= ~(IDMAC_DES0_CH | IDMAC_DES0_DIC);
	desc_last->des0 |= IDMAC_DES0_LD;

	return 0;
err_own_bit:
	 
	dev_dbg(host->dev, "descriptor is still owned by IDMAC.\n");
	memset(host->sg_cpu, 0, DESC_RING_BUF_SZ);
	dw_mci_idmac_init(host);
	return -EINVAL;
}


static inline int dw_mci_prepare_desc32(struct dw_mci *host,
					 struct mmc_data *data,
					 unsigned int sg_len)
{
	unsigned int desc_len;
	struct idmac_desc *desc_first, *desc_last, *desc;
	u32 val;
	int i;

	desc_first = desc_last = desc = host->sg_cpu;

	for (i = 0; i < sg_len; i++) {
		unsigned int length = sg_dma_len(&data->sg[i]);

		u32 mem_addr = sg_dma_address(&data->sg[i]);

		for ( ; length ; desc++) {
			desc_len = (length <= DW_MCI_DESC_DATA_LENGTH) ?
				   length : DW_MCI_DESC_DATA_LENGTH;

			length -= desc_len;

			 
			if (readl_poll_timeout_atomic(&desc->des0, val,
						      IDMAC_OWN_CLR64(val),
						      10,
						      100 * USEC_PER_MSEC))
				goto err_own_bit;

			 
			desc->des0 = cpu_to_le32(IDMAC_DES0_OWN |
						 IDMAC_DES0_DIC |
						 IDMAC_DES0_CH);

			 
			IDMAC_SET_BUFFER1_SIZE(desc, desc_len);

			 
			desc->des2 = cpu_to_le32(mem_addr);

			 
			mem_addr += desc_len;

			 
			desc_last = desc;
		}
	}

	 
	desc_first->des0 |= cpu_to_le32(IDMAC_DES0_FD);

	 
	desc_last->des0 &= cpu_to_le32(~(IDMAC_DES0_CH |
				       IDMAC_DES0_DIC));
	desc_last->des0 |= cpu_to_le32(IDMAC_DES0_LD);

	return 0;
err_own_bit:
	 
	dev_dbg(host->dev, "descriptor is still owned by IDMAC.\n");
	memset(host->sg_cpu, 0, DESC_RING_BUF_SZ);
	dw_mci_idmac_init(host);
	return -EINVAL;
}

static int dw_mci_idmac_start_dma(struct dw_mci *host, unsigned int sg_len)
{
	u32 temp;
	int ret;

	if (host->dma_64bit_address == 1)
		ret = dw_mci_prepare_desc64(host, host->data, sg_len);
	else
		ret = dw_mci_prepare_desc32(host, host->data, sg_len);

	if (ret)
		goto out;

	 
	wmb();

	 
	dw_mci_ctrl_reset(host, SDMMC_CTRL_DMA_RESET);
	dw_mci_idmac_reset(host);

	 
	temp = mci_readl(host, CTRL);
	temp |= SDMMC_CTRL_USE_IDMAC;
	mci_writel(host, CTRL, temp);

	 
	wmb();

	 
	temp = mci_readl(host, BMOD);
	temp |= SDMMC_IDMAC_ENABLE | SDMMC_IDMAC_FB;
	mci_writel(host, BMOD, temp);

	 
	mci_writel(host, PLDMND, 1);

out:
	return ret;
}

static const struct dw_mci_dma_ops dw_mci_idmac_ops = {
	.init = dw_mci_idmac_init,
	.start = dw_mci_idmac_start_dma,
	.stop = dw_mci_idmac_stop_dma,
	.complete = dw_mci_dmac_complete_dma,
	.cleanup = dw_mci_dma_cleanup,
};

static void dw_mci_edmac_stop_dma(struct dw_mci *host)
{
	dmaengine_terminate_async(host->dms->ch);
}

static int dw_mci_edmac_start_dma(struct dw_mci *host,
					    unsigned int sg_len)
{
	struct dma_slave_config cfg;
	struct dma_async_tx_descriptor *desc = NULL;
	struct scatterlist *sgl = host->data->sg;
	static const u32 mszs[] = {1, 4, 8, 16, 32, 64, 128, 256};
	u32 sg_elems = host->data->sg_len;
	u32 fifoth_val;
	u32 fifo_offset = host->fifo_reg - host->regs;
	int ret = 0;

	 
	memset(&cfg, 0, sizeof(cfg));
	cfg.dst_addr = host->phy_regs + fifo_offset;
	cfg.src_addr = cfg.dst_addr;
	cfg.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	cfg.src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;

	 
	fifoth_val = mci_readl(host, FIFOTH);
	cfg.dst_maxburst = mszs[(fifoth_val >> 28) & 0x7];
	cfg.src_maxburst = cfg.dst_maxburst;

	if (host->data->flags & MMC_DATA_WRITE)
		cfg.direction = DMA_MEM_TO_DEV;
	else
		cfg.direction = DMA_DEV_TO_MEM;

	ret = dmaengine_slave_config(host->dms->ch, &cfg);
	if (ret) {
		dev_err(host->dev, "Failed to config edmac.\n");
		return -EBUSY;
	}

	desc = dmaengine_prep_slave_sg(host->dms->ch, sgl,
				       sg_len, cfg.direction,
				       DMA_PREP_INTERRUPT | DMA_CTRL_ACK);
	if (!desc) {
		dev_err(host->dev, "Can't prepare slave sg.\n");
		return -EBUSY;
	}

	 
	desc->callback = dw_mci_dmac_complete_dma;
	desc->callback_param = (void *)host;
	dmaengine_submit(desc);

	 
	if (host->data->flags & MMC_DATA_WRITE)
		dma_sync_sg_for_device(mmc_dev(host->slot->mmc), sgl,
				       sg_elems, DMA_TO_DEVICE);

	dma_async_issue_pending(host->dms->ch);

	return 0;
}

static int dw_mci_edmac_init(struct dw_mci *host)
{
	 
	host->dms = kzalloc(sizeof(struct dw_mci_dma_slave), GFP_KERNEL);
	if (!host->dms)
		return -ENOMEM;

	host->dms->ch = dma_request_chan(host->dev, "rx-tx");
	if (IS_ERR(host->dms->ch)) {
		int ret = PTR_ERR(host->dms->ch);

		dev_err(host->dev, "Failed to get external DMA channel.\n");
		kfree(host->dms);
		host->dms = NULL;
		return ret;
	}

	return 0;
}

static void dw_mci_edmac_exit(struct dw_mci *host)
{
	if (host->dms) {
		if (host->dms->ch) {
			dma_release_channel(host->dms->ch);
			host->dms->ch = NULL;
		}
		kfree(host->dms);
		host->dms = NULL;
	}
}

static const struct dw_mci_dma_ops dw_mci_edmac_ops = {
	.init = dw_mci_edmac_init,
	.exit = dw_mci_edmac_exit,
	.start = dw_mci_edmac_start_dma,
	.stop = dw_mci_edmac_stop_dma,
	.complete = dw_mci_dmac_complete_dma,
	.cleanup = dw_mci_dma_cleanup,
};

static int dw_mci_pre_dma_transfer(struct dw_mci *host,
				   struct mmc_data *data,
				   int cookie)
{
	struct scatterlist *sg;
	unsigned int i, sg_len;

	if (data->host_cookie == COOKIE_PRE_MAPPED)
		return data->sg_len;

	 
	if (data->blocks * data->blksz < DW_MCI_DMA_THRESHOLD)
		return -EINVAL;

	if (data->blksz & 3)
		return -EINVAL;

	for_each_sg(data->sg, sg, data->sg_len, i) {
		if (sg->offset & 3 || sg->length & 3)
			return -EINVAL;
	}

	sg_len = dma_map_sg(host->dev,
			    data->sg,
			    data->sg_len,
			    mmc_get_dma_dir(data));
	if (sg_len == 0)
		return -EINVAL;

	data->host_cookie = cookie;

	return sg_len;
}

static void dw_mci_pre_req(struct mmc_host *mmc,
			   struct mmc_request *mrq)
{
	struct dw_mci_slot *slot = mmc_priv(mmc);
	struct mmc_data *data = mrq->data;

	if (!slot->host->use_dma || !data)
		return;

	 
	data->host_cookie = COOKIE_UNMAPPED;

	if (dw_mci_pre_dma_transfer(slot->host, mrq->data,
				COOKIE_PRE_MAPPED) < 0)
		data->host_cookie = COOKIE_UNMAPPED;
}

static void dw_mci_post_req(struct mmc_host *mmc,
			    struct mmc_request *mrq,
			    int err)
{
	struct dw_mci_slot *slot = mmc_priv(mmc);
	struct mmc_data *data = mrq->data;

	if (!slot->host->use_dma || !data)
		return;

	if (data->host_cookie != COOKIE_UNMAPPED)
		dma_unmap_sg(slot->host->dev,
			     data->sg,
			     data->sg_len,
			     mmc_get_dma_dir(data));
	data->host_cookie = COOKIE_UNMAPPED;
}

static int dw_mci_get_cd(struct mmc_host *mmc)
{
	int present;
	struct dw_mci_slot *slot = mmc_priv(mmc);
	struct dw_mci *host = slot->host;
	int gpio_cd = mmc_gpio_get_cd(mmc);

	 
	if (((mmc->caps & MMC_CAP_NEEDS_POLL)
				|| !mmc_card_is_removable(mmc))) {
		present = 1;

		if (!test_bit(DW_MMC_CARD_PRESENT, &slot->flags)) {
			if (mmc->caps & MMC_CAP_NEEDS_POLL) {
				dev_info(&mmc->class_dev,
					"card is polling.\n");
			} else {
				dev_info(&mmc->class_dev,
					"card is non-removable.\n");
			}
			set_bit(DW_MMC_CARD_PRESENT, &slot->flags);
		}

		return present;
	} else if (gpio_cd >= 0)
		present = gpio_cd;
	else
		present = (mci_readl(slot->host, CDETECT) & (1 << slot->id))
			== 0 ? 1 : 0;

	spin_lock_bh(&host->lock);
	if (present && !test_and_set_bit(DW_MMC_CARD_PRESENT, &slot->flags))
		dev_dbg(&mmc->class_dev, "card is present\n");
	else if (!present &&
			!test_and_clear_bit(DW_MMC_CARD_PRESENT, &slot->flags))
		dev_dbg(&mmc->class_dev, "card is not present\n");
	spin_unlock_bh(&host->lock);

	return present;
}

static void dw_mci_adjust_fifoth(struct dw_mci *host, struct mmc_data *data)
{
	unsigned int blksz = data->blksz;
	static const u32 mszs[] = {1, 4, 8, 16, 32, 64, 128, 256};
	u32 fifo_width = 1 << host->data_shift;
	u32 blksz_depth = blksz / fifo_width, fifoth_val;
	u32 msize = 0, rx_wmark = 1, tx_wmark, tx_wmark_invers;
	int idx = ARRAY_SIZE(mszs) - 1;

	 
	if (!host->use_dma)
		return;

	tx_wmark = (host->fifo_depth) / 2;
	tx_wmark_invers = host->fifo_depth - tx_wmark;

	 
	if (blksz % fifo_width)
		goto done;

	do {
		if (!((blksz_depth % mszs[idx]) ||
		     (tx_wmark_invers % mszs[idx]))) {
			msize = idx;
			rx_wmark = mszs[idx] - 1;
			break;
		}
	} while (--idx > 0);
	 
done:
	fifoth_val = SDMMC_SET_FIFOTH(msize, rx_wmark, tx_wmark);
	mci_writel(host, FIFOTH, fifoth_val);
}

static void dw_mci_ctrl_thld(struct dw_mci *host, struct mmc_data *data)
{
	unsigned int blksz = data->blksz;
	u32 blksz_depth, fifo_depth;
	u16 thld_size;
	u8 enable;

	 
	if (host->verid < DW_MMC_240A ||
		(host->verid < DW_MMC_280A && data->flags & MMC_DATA_WRITE))
		return;

	 
	if (data->flags & MMC_DATA_WRITE &&
		host->timing != MMC_TIMING_MMC_HS400)
		goto disable;

	if (data->flags & MMC_DATA_WRITE)
		enable = SDMMC_CARD_WR_THR_EN;
	else
		enable = SDMMC_CARD_RD_THR_EN;

	if (host->timing != MMC_TIMING_MMC_HS200 &&
	    host->timing != MMC_TIMING_UHS_SDR104 &&
	    host->timing != MMC_TIMING_MMC_HS400)
		goto disable;

	blksz_depth = blksz / (1 << host->data_shift);
	fifo_depth = host->fifo_depth;

	if (blksz_depth > fifo_depth)
		goto disable;

	 
	thld_size = blksz;
	mci_writel(host, CDTHRCTL, SDMMC_SET_THLD(thld_size, enable));
	return;

disable:
	mci_writel(host, CDTHRCTL, 0);
}

static int dw_mci_submit_data_dma(struct dw_mci *host, struct mmc_data *data)
{
	unsigned long irqflags;
	int sg_len;
	u32 temp;

	host->using_dma = 0;

	 
	if (!host->use_dma)
		return -ENODEV;

	sg_len = dw_mci_pre_dma_transfer(host, data, COOKIE_MAPPED);
	if (sg_len < 0) {
		host->dma_ops->stop(host);
		return sg_len;
	}

	host->using_dma = 1;

	if (host->use_dma == TRANS_MODE_IDMAC)
		dev_vdbg(host->dev,
			 "sd sg_cpu: %#lx sg_dma: %#lx sg_len: %d\n",
			 (unsigned long)host->sg_cpu,
			 (unsigned long)host->sg_dma,
			 sg_len);

	 
	if (host->prev_blksz != data->blksz)
		dw_mci_adjust_fifoth(host, data);

	 
	temp = mci_readl(host, CTRL);
	temp |= SDMMC_CTRL_DMA_ENABLE;
	mci_writel(host, CTRL, temp);

	 
	spin_lock_irqsave(&host->irq_lock, irqflags);
	temp = mci_readl(host, INTMASK);
	temp  &= ~(SDMMC_INT_RXDR | SDMMC_INT_TXDR);
	mci_writel(host, INTMASK, temp);
	spin_unlock_irqrestore(&host->irq_lock, irqflags);

	if (host->dma_ops->start(host, sg_len)) {
		host->dma_ops->stop(host);
		 
		dev_dbg(host->dev,
			"%s: fall back to PIO mode for current transfer\n",
			__func__);
		return -ENODEV;
	}

	return 0;
}

static void dw_mci_submit_data(struct dw_mci *host, struct mmc_data *data)
{
	unsigned long irqflags;
	int flags = SG_MITER_ATOMIC;
	u32 temp;

	data->error = -EINPROGRESS;

	WARN_ON(host->data);
	host->sg = NULL;
	host->data = data;

	if (data->flags & MMC_DATA_READ)
		host->dir_status = DW_MCI_RECV_STATUS;
	else
		host->dir_status = DW_MCI_SEND_STATUS;

	dw_mci_ctrl_thld(host, data);

	if (dw_mci_submit_data_dma(host, data)) {
		if (host->data->flags & MMC_DATA_READ)
			flags |= SG_MITER_TO_SG;
		else
			flags |= SG_MITER_FROM_SG;

		sg_miter_start(&host->sg_miter, data->sg, data->sg_len, flags);
		host->sg = data->sg;
		host->part_buf_start = 0;
		host->part_buf_count = 0;

		mci_writel(host, RINTSTS, SDMMC_INT_TXDR | SDMMC_INT_RXDR);

		spin_lock_irqsave(&host->irq_lock, irqflags);
		temp = mci_readl(host, INTMASK);
		temp |= SDMMC_INT_TXDR | SDMMC_INT_RXDR;
		mci_writel(host, INTMASK, temp);
		spin_unlock_irqrestore(&host->irq_lock, irqflags);

		temp = mci_readl(host, CTRL);
		temp &= ~SDMMC_CTRL_DMA_ENABLE;
		mci_writel(host, CTRL, temp);

		 
		if (host->wm_aligned)
			dw_mci_adjust_fifoth(host, data);
		else
			mci_writel(host, FIFOTH, host->fifoth_val);
		host->prev_blksz = 0;
	} else {
		 
		host->prev_blksz = data->blksz;
	}
}

static void dw_mci_setup_bus(struct dw_mci_slot *slot, bool force_clkinit)
{
	struct dw_mci *host = slot->host;
	unsigned int clock = slot->clock;
	u32 div;
	u32 clk_en_a;
	u32 sdmmc_cmd_bits = SDMMC_CMD_UPD_CLK | SDMMC_CMD_PRV_DAT_WAIT;

	 
	if (host->state == STATE_WAITING_CMD11_DONE)
		sdmmc_cmd_bits |= SDMMC_CMD_VOLT_SWITCH;

	slot->mmc->actual_clock = 0;

	if (!clock) {
		mci_writel(host, CLKENA, 0);
		mci_send_cmd(slot, sdmmc_cmd_bits, 0);
	} else if (clock != host->current_speed || force_clkinit) {
		div = host->bus_hz / clock;
		if (host->bus_hz % clock && host->bus_hz > clock)
			 
			div += 1;

		div = (host->bus_hz != clock) ? DIV_ROUND_UP(div, 2) : 0;

		if ((clock != slot->__clk_old &&
			!test_bit(DW_MMC_CARD_NEEDS_POLL, &slot->flags)) ||
			force_clkinit) {
			 
			if (!force_clkinit)
				dev_info(&slot->mmc->class_dev,
					 "Bus speed (slot %d) = %dHz (slot req %dHz, actual %dHZ div = %d)\n",
					 slot->id, host->bus_hz, clock,
					 div ? ((host->bus_hz / div) >> 1) :
					 host->bus_hz, div);

			 
			if (slot->mmc->caps & MMC_CAP_NEEDS_POLL &&
					slot->mmc->f_min == clock)
				set_bit(DW_MMC_CARD_NEEDS_POLL, &slot->flags);
		}

		 
		mci_writel(host, CLKENA, 0);
		mci_writel(host, CLKSRC, 0);

		 
		mci_send_cmd(slot, sdmmc_cmd_bits, 0);

		 
		mci_writel(host, CLKDIV, div);

		 
		mci_send_cmd(slot, sdmmc_cmd_bits, 0);

		 
		clk_en_a = SDMMC_CLKEN_ENABLE << slot->id;
		if (!test_bit(DW_MMC_CARD_NO_LOW_PWR, &slot->flags))
			clk_en_a |= SDMMC_CLKEN_LOW_PWR << slot->id;
		mci_writel(host, CLKENA, clk_en_a);

		 
		mci_send_cmd(slot, sdmmc_cmd_bits, 0);

		 
		slot->__clk_old = clock;
		slot->mmc->actual_clock = div ? ((host->bus_hz / div) >> 1) :
					  host->bus_hz;
	}

	host->current_speed = clock;

	 
	mci_writel(host, CTYPE, (slot->ctype << slot->id));
}

static void dw_mci_set_data_timeout(struct dw_mci *host,
				    unsigned int timeout_ns)
{
	const struct dw_mci_drv_data *drv_data = host->drv_data;
	u32 clk_div, tmout;
	u64 tmp;

	if (drv_data && drv_data->set_data_timeout)
		return drv_data->set_data_timeout(host, timeout_ns);

	clk_div = (mci_readl(host, CLKDIV) & 0xFF) * 2;
	if (clk_div == 0)
		clk_div = 1;

	tmp = DIV_ROUND_UP_ULL((u64)timeout_ns * host->bus_hz, NSEC_PER_SEC);
	tmp = DIV_ROUND_UP_ULL(tmp, clk_div);

	 
	tmout = 0xFF;  

	 
	if (!tmp || tmp > 0xFFFFFF)
		tmout |= (0xFFFFFF << 8);
	else
		tmout |= (tmp & 0xFFFFFF) << 8;

	mci_writel(host, TMOUT, tmout);
	dev_dbg(host->dev, "timeout_ns: %u => TMOUT[31:8]: %#08x",
		timeout_ns, tmout >> 8);
}

static void __dw_mci_start_request(struct dw_mci *host,
				   struct dw_mci_slot *slot,
				   struct mmc_command *cmd)
{
	struct mmc_request *mrq;
	struct mmc_data	*data;
	u32 cmdflags;

	mrq = slot->mrq;

	host->mrq = mrq;

	host->pending_events = 0;
	host->completed_events = 0;
	host->cmd_status = 0;
	host->data_status = 0;
	host->dir_status = 0;

	data = cmd->data;
	if (data) {
		dw_mci_set_data_timeout(host, data->timeout_ns);
		mci_writel(host, BYTCNT, data->blksz*data->blocks);
		mci_writel(host, BLKSIZ, data->blksz);
	}

	cmdflags = dw_mci_prepare_command(slot->mmc, cmd);

	 
	if (test_and_clear_bit(DW_MMC_CARD_NEED_INIT, &slot->flags))
		cmdflags |= SDMMC_CMD_INIT;

	if (data) {
		dw_mci_submit_data(host, data);
		wmb();  
	}

	dw_mci_start_command(host, cmd, cmdflags);

	if (cmd->opcode == SD_SWITCH_VOLTAGE) {
		unsigned long irqflags;

		 
		spin_lock_irqsave(&host->irq_lock, irqflags);
		if (!test_bit(EVENT_CMD_COMPLETE, &host->pending_events))
			mod_timer(&host->cmd11_timer,
				jiffies + msecs_to_jiffies(500) + 1);
		spin_unlock_irqrestore(&host->irq_lock, irqflags);
	}

	host->stop_cmdr = dw_mci_prep_stop_abort(host, cmd);
}

static void dw_mci_start_request(struct dw_mci *host,
				 struct dw_mci_slot *slot)
{
	struct mmc_request *mrq = slot->mrq;
	struct mmc_command *cmd;

	cmd = mrq->sbc ? mrq->sbc : mrq->cmd;
	__dw_mci_start_request(host, slot, cmd);
}

 
static void dw_mci_queue_request(struct dw_mci *host, struct dw_mci_slot *slot,
				 struct mmc_request *mrq)
{
	dev_vdbg(&slot->mmc->class_dev, "queue request: state=%d\n",
		 host->state);

	slot->mrq = mrq;

	if (host->state == STATE_WAITING_CMD11_DONE) {
		dev_warn(&slot->mmc->class_dev,
			 "Voltage change didn't complete\n");
		 
		host->state = STATE_IDLE;
	}

	if (host->state == STATE_IDLE) {
		host->state = STATE_SENDING_CMD;
		dw_mci_start_request(host, slot);
	} else {
		list_add_tail(&slot->queue_node, &host->queue);
	}
}

static void dw_mci_request(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct dw_mci_slot *slot = mmc_priv(mmc);
	struct dw_mci *host = slot->host;

	WARN_ON(slot->mrq);

	 

	if (!dw_mci_get_cd(mmc)) {
		mrq->cmd->error = -ENOMEDIUM;
		mmc_request_done(mmc, mrq);
		return;
	}

	spin_lock_bh(&host->lock);

	dw_mci_queue_request(host, slot, mrq);

	spin_unlock_bh(&host->lock);
}

static void dw_mci_set_ios(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct dw_mci_slot *slot = mmc_priv(mmc);
	const struct dw_mci_drv_data *drv_data = slot->host->drv_data;
	u32 regs;
	int ret;

	switch (ios->bus_width) {
	case MMC_BUS_WIDTH_4:
		slot->ctype = SDMMC_CTYPE_4BIT;
		break;
	case MMC_BUS_WIDTH_8:
		slot->ctype = SDMMC_CTYPE_8BIT;
		break;
	default:
		 
		slot->ctype = SDMMC_CTYPE_1BIT;
	}

	regs = mci_readl(slot->host, UHS_REG);

	 
	if (ios->timing == MMC_TIMING_MMC_DDR52 ||
	    ios->timing == MMC_TIMING_UHS_DDR50 ||
	    ios->timing == MMC_TIMING_MMC_HS400)
		regs |= ((0x1 << slot->id) << 16);
	else
		regs &= ~((0x1 << slot->id) << 16);

	mci_writel(slot->host, UHS_REG, regs);
	slot->host->timing = ios->timing;

	 
	slot->clock = ios->clock;

	if (drv_data && drv_data->set_ios)
		drv_data->set_ios(slot->host, ios);

	switch (ios->power_mode) {
	case MMC_POWER_UP:
		if (!IS_ERR(mmc->supply.vmmc)) {
			ret = mmc_regulator_set_ocr(mmc, mmc->supply.vmmc,
					ios->vdd);
			if (ret) {
				dev_err(slot->host->dev,
					"failed to enable vmmc regulator\n");
				 
				return;
			}
		}
		set_bit(DW_MMC_CARD_NEED_INIT, &slot->flags);
		regs = mci_readl(slot->host, PWREN);
		regs |= (1 << slot->id);
		mci_writel(slot->host, PWREN, regs);
		break;
	case MMC_POWER_ON:
		if (!slot->host->vqmmc_enabled) {
			if (!IS_ERR(mmc->supply.vqmmc)) {
				ret = regulator_enable(mmc->supply.vqmmc);
				if (ret < 0)
					dev_err(slot->host->dev,
						"failed to enable vqmmc\n");
				else
					slot->host->vqmmc_enabled = true;

			} else {
				 
				slot->host->vqmmc_enabled = true;
			}

			 
			dw_mci_ctrl_reset(slot->host,
					  SDMMC_CTRL_ALL_RESET_FLAGS);
		}

		 
		dw_mci_setup_bus(slot, false);

		break;
	case MMC_POWER_OFF:
		 
		dw_mci_setup_bus(slot, false);

		if (!IS_ERR(mmc->supply.vmmc))
			mmc_regulator_set_ocr(mmc, mmc->supply.vmmc, 0);

		if (!IS_ERR(mmc->supply.vqmmc) && slot->host->vqmmc_enabled)
			regulator_disable(mmc->supply.vqmmc);
		slot->host->vqmmc_enabled = false;

		regs = mci_readl(slot->host, PWREN);
		regs &= ~(1 << slot->id);
		mci_writel(slot->host, PWREN, regs);
		break;
	default:
		break;
	}

	if (slot->host->state == STATE_WAITING_CMD11_DONE && ios->clock != 0)
		slot->host->state = STATE_IDLE;
}

static int dw_mci_card_busy(struct mmc_host *mmc)
{
	struct dw_mci_slot *slot = mmc_priv(mmc);
	u32 status;

	 
	status = mci_readl(slot->host, STATUS);

	return !!(status & SDMMC_STATUS_BUSY);
}

static int dw_mci_switch_voltage(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct dw_mci_slot *slot = mmc_priv(mmc);
	struct dw_mci *host = slot->host;
	const struct dw_mci_drv_data *drv_data = host->drv_data;
	u32 uhs;
	u32 v18 = SDMMC_UHS_18V << slot->id;
	int ret;

	if (drv_data && drv_data->switch_voltage)
		return drv_data->switch_voltage(mmc, ios);

	 
	uhs = mci_readl(host, UHS_REG);
	if (ios->signal_voltage == MMC_SIGNAL_VOLTAGE_330)
		uhs &= ~v18;
	else
		uhs |= v18;

	if (!IS_ERR(mmc->supply.vqmmc)) {
		ret = mmc_regulator_set_vqmmc(mmc, ios);
		if (ret < 0) {
			dev_dbg(&mmc->class_dev,
					 "Regulator set error %d - %s V\n",
					 ret, uhs & v18 ? "1.8" : "3.3");
			return ret;
		}
	}
	mci_writel(host, UHS_REG, uhs);

	return 0;
}

static int dw_mci_get_ro(struct mmc_host *mmc)
{
	int read_only;
	struct dw_mci_slot *slot = mmc_priv(mmc);
	int gpio_ro = mmc_gpio_get_ro(mmc);

	 
	if (gpio_ro >= 0)
		read_only = gpio_ro;
	else
		read_only =
			mci_readl(slot->host, WRTPRT) & (1 << slot->id) ? 1 : 0;

	dev_dbg(&mmc->class_dev, "card is %s\n",
		read_only ? "read-only" : "read-write");

	return read_only;
}

static void dw_mci_hw_reset(struct mmc_host *mmc)
{
	struct dw_mci_slot *slot = mmc_priv(mmc);
	struct dw_mci *host = slot->host;
	int reset;

	if (host->use_dma == TRANS_MODE_IDMAC)
		dw_mci_idmac_reset(host);

	if (!dw_mci_ctrl_reset(host, SDMMC_CTRL_DMA_RESET |
				     SDMMC_CTRL_FIFO_RESET))
		return;

	 
	reset = mci_readl(host, RST_N);
	reset &= ~(SDMMC_RST_HWACTIVE << slot->id);
	mci_writel(host, RST_N, reset);
	usleep_range(1, 2);
	reset |= SDMMC_RST_HWACTIVE << slot->id;
	mci_writel(host, RST_N, reset);
	usleep_range(200, 300);
}

static void dw_mci_prepare_sdio_irq(struct dw_mci_slot *slot, bool prepare)
{
	struct dw_mci *host = slot->host;
	const u32 clken_low_pwr = SDMMC_CLKEN_LOW_PWR << slot->id;
	u32 clk_en_a_old;
	u32 clk_en_a;

	 

	clk_en_a_old = mci_readl(host, CLKENA);
	if (prepare) {
		set_bit(DW_MMC_CARD_NO_LOW_PWR, &slot->flags);
		clk_en_a = clk_en_a_old & ~clken_low_pwr;
	} else {
		clear_bit(DW_MMC_CARD_NO_LOW_PWR, &slot->flags);
		clk_en_a = clk_en_a_old | clken_low_pwr;
	}

	if (clk_en_a != clk_en_a_old) {
		mci_writel(host, CLKENA, clk_en_a);
		mci_send_cmd(slot, SDMMC_CMD_UPD_CLK | SDMMC_CMD_PRV_DAT_WAIT,
			     0);
	}
}

static void __dw_mci_enable_sdio_irq(struct dw_mci_slot *slot, int enb)
{
	struct dw_mci *host = slot->host;
	unsigned long irqflags;
	u32 int_mask;

	spin_lock_irqsave(&host->irq_lock, irqflags);

	 
	int_mask = mci_readl(host, INTMASK);
	if (enb)
		int_mask |= SDMMC_INT_SDIO(slot->sdio_id);
	else
		int_mask &= ~SDMMC_INT_SDIO(slot->sdio_id);
	mci_writel(host, INTMASK, int_mask);

	spin_unlock_irqrestore(&host->irq_lock, irqflags);
}

static void dw_mci_enable_sdio_irq(struct mmc_host *mmc, int enb)
{
	struct dw_mci_slot *slot = mmc_priv(mmc);
	struct dw_mci *host = slot->host;

	dw_mci_prepare_sdio_irq(slot, enb);
	__dw_mci_enable_sdio_irq(slot, enb);

	 
	if (enb)
		pm_runtime_get_noresume(host->dev);
	else
		pm_runtime_put_noidle(host->dev);
}

static void dw_mci_ack_sdio_irq(struct mmc_host *mmc)
{
	struct dw_mci_slot *slot = mmc_priv(mmc);

	__dw_mci_enable_sdio_irq(slot, 1);
}

static int dw_mci_execute_tuning(struct mmc_host *mmc, u32 opcode)
{
	struct dw_mci_slot *slot = mmc_priv(mmc);
	struct dw_mci *host = slot->host;
	const struct dw_mci_drv_data *drv_data = host->drv_data;
	int err = -EINVAL;

	if (drv_data && drv_data->execute_tuning)
		err = drv_data->execute_tuning(slot, opcode);
	return err;
}

static int dw_mci_prepare_hs400_tuning(struct mmc_host *mmc,
				       struct mmc_ios *ios)
{
	struct dw_mci_slot *slot = mmc_priv(mmc);
	struct dw_mci *host = slot->host;
	const struct dw_mci_drv_data *drv_data = host->drv_data;

	if (drv_data && drv_data->prepare_hs400_tuning)
		return drv_data->prepare_hs400_tuning(host, ios);

	return 0;
}

static bool dw_mci_reset(struct dw_mci *host)
{
	u32 flags = SDMMC_CTRL_RESET | SDMMC_CTRL_FIFO_RESET;
	bool ret = false;
	u32 status = 0;

	 
	if (host->sg) {
		sg_miter_stop(&host->sg_miter);
		host->sg = NULL;
	}

	if (host->use_dma)
		flags |= SDMMC_CTRL_DMA_RESET;

	if (dw_mci_ctrl_reset(host, flags)) {
		 
		mci_writel(host, RINTSTS, 0xFFFFFFFF);

		if (!host->use_dma) {
			ret = true;
			goto ciu_out;
		}

		 
		if (readl_poll_timeout_atomic(host->regs + SDMMC_STATUS,
					      status,
					      !(status & SDMMC_STATUS_DMA_REQ),
					      1, 500 * USEC_PER_MSEC)) {
			dev_err(host->dev,
				"%s: Timeout waiting for dma_req to be cleared\n",
				__func__);
			goto ciu_out;
		}

		 
		if (!dw_mci_ctrl_reset(host, SDMMC_CTRL_FIFO_RESET))
			goto ciu_out;
	} else {
		 
		if (!(mci_readl(host, CTRL) & SDMMC_CTRL_RESET)) {
			dev_err(host->dev,
				"%s: fifo/dma reset bits didn't clear but ciu was reset, doing clock update\n",
				__func__);
			goto ciu_out;
		}
	}

	if (host->use_dma == TRANS_MODE_IDMAC)
		 
		dw_mci_idmac_init(host);

	ret = true;

ciu_out:
	 
	mci_send_cmd(host->slot, SDMMC_CMD_UPD_CLK, 0);

	return ret;
}

static const struct mmc_host_ops dw_mci_ops = {
	.request		= dw_mci_request,
	.pre_req		= dw_mci_pre_req,
	.post_req		= dw_mci_post_req,
	.set_ios		= dw_mci_set_ios,
	.get_ro			= dw_mci_get_ro,
	.get_cd			= dw_mci_get_cd,
	.card_hw_reset          = dw_mci_hw_reset,
	.enable_sdio_irq	= dw_mci_enable_sdio_irq,
	.ack_sdio_irq		= dw_mci_ack_sdio_irq,
	.execute_tuning		= dw_mci_execute_tuning,
	.card_busy		= dw_mci_card_busy,
	.start_signal_voltage_switch = dw_mci_switch_voltage,
	.prepare_hs400_tuning	= dw_mci_prepare_hs400_tuning,
};

#ifdef CONFIG_FAULT_INJECTION
static enum hrtimer_restart dw_mci_fault_timer(struct hrtimer *t)
{
	struct dw_mci *host = container_of(t, struct dw_mci, fault_timer);
	unsigned long flags;

	spin_lock_irqsave(&host->irq_lock, flags);

	 
	if (!host->data_status) {
		host->data_status = SDMMC_INT_DCRC;
		set_bit(EVENT_DATA_ERROR, &host->pending_events);
		tasklet_schedule(&host->tasklet);
	}

	spin_unlock_irqrestore(&host->irq_lock, flags);

	return HRTIMER_NORESTART;
}

static void dw_mci_start_fault_timer(struct dw_mci *host)
{
	struct mmc_data *data = host->data;

	if (!data || data->blocks <= 1)
		return;

	if (!should_fail(&host->fail_data_crc, 1))
		return;

	 
	hrtimer_start(&host->fault_timer,
		      ms_to_ktime(get_random_u32_below(25)),
		      HRTIMER_MODE_REL);
}

static void dw_mci_stop_fault_timer(struct dw_mci *host)
{
	hrtimer_cancel(&host->fault_timer);
}

static void dw_mci_init_fault(struct dw_mci *host)
{
	host->fail_data_crc = (struct fault_attr) FAULT_ATTR_INITIALIZER;

	hrtimer_init(&host->fault_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	host->fault_timer.function = dw_mci_fault_timer;
}
#else
static void dw_mci_init_fault(struct dw_mci *host)
{
}

static void dw_mci_start_fault_timer(struct dw_mci *host)
{
}

static void dw_mci_stop_fault_timer(struct dw_mci *host)
{
}
#endif

static void dw_mci_request_end(struct dw_mci *host, struct mmc_request *mrq)
	__releases(&host->lock)
	__acquires(&host->lock)
{
	struct dw_mci_slot *slot;
	struct mmc_host	*prev_mmc = host->slot->mmc;

	WARN_ON(host->cmd || host->data);

	host->slot->mrq = NULL;
	host->mrq = NULL;
	if (!list_empty(&host->queue)) {
		slot = list_entry(host->queue.next,
				  struct dw_mci_slot, queue_node);
		list_del(&slot->queue_node);
		dev_vdbg(host->dev, "list not empty: %s is next\n",
			 mmc_hostname(slot->mmc));
		host->state = STATE_SENDING_CMD;
		dw_mci_start_request(host, slot);
	} else {
		dev_vdbg(host->dev, "list empty\n");

		if (host->state == STATE_SENDING_CMD11)
			host->state = STATE_WAITING_CMD11_DONE;
		else
			host->state = STATE_IDLE;
	}

	spin_unlock(&host->lock);
	mmc_request_done(prev_mmc, mrq);
	spin_lock(&host->lock);
}

static int dw_mci_command_complete(struct dw_mci *host, struct mmc_command *cmd)
{
	u32 status = host->cmd_status;

	host->cmd_status = 0;

	 
	if (cmd->flags & MMC_RSP_PRESENT) {
		if (cmd->flags & MMC_RSP_136) {
			cmd->resp[3] = mci_readl(host, RESP0);
			cmd->resp[2] = mci_readl(host, RESP1);
			cmd->resp[1] = mci_readl(host, RESP2);
			cmd->resp[0] = mci_readl(host, RESP3);
		} else {
			cmd->resp[0] = mci_readl(host, RESP0);
			cmd->resp[1] = 0;
			cmd->resp[2] = 0;
			cmd->resp[3] = 0;
		}
	}

	if (status & SDMMC_INT_RTO)
		cmd->error = -ETIMEDOUT;
	else if ((cmd->flags & MMC_RSP_CRC) && (status & SDMMC_INT_RCRC))
		cmd->error = -EILSEQ;
	else if (status & SDMMC_INT_RESP_ERR)
		cmd->error = -EIO;
	else
		cmd->error = 0;

	return cmd->error;
}

static int dw_mci_data_complete(struct dw_mci *host, struct mmc_data *data)
{
	u32 status = host->data_status;

	if (status & DW_MCI_DATA_ERROR_FLAGS) {
		if (status & SDMMC_INT_DRTO) {
			data->error = -ETIMEDOUT;
		} else if (status & SDMMC_INT_DCRC) {
			data->error = -EILSEQ;
		} else if (status & SDMMC_INT_EBE) {
			if (host->dir_status ==
				DW_MCI_SEND_STATUS) {
				 
				data->bytes_xfered = 0;
				data->error = -ETIMEDOUT;
			} else if (host->dir_status ==
					DW_MCI_RECV_STATUS) {
				data->error = -EILSEQ;
			}
		} else {
			 
			data->error = -EILSEQ;
		}

		dev_dbg(host->dev, "data error, status 0x%08x\n", status);

		 
		dw_mci_reset(host);
	} else {
		data->bytes_xfered = data->blocks * data->blksz;
		data->error = 0;
	}

	return data->error;
}

static void dw_mci_set_drto(struct dw_mci *host)
{
	const struct dw_mci_drv_data *drv_data = host->drv_data;
	unsigned int drto_clks;
	unsigned int drto_div;
	unsigned int drto_ms;
	unsigned long irqflags;

	if (drv_data && drv_data->get_drto_clks)
		drto_clks = drv_data->get_drto_clks(host);
	else
		drto_clks = mci_readl(host, TMOUT) >> 8;
	drto_div = (mci_readl(host, CLKDIV) & 0xff) * 2;
	if (drto_div == 0)
		drto_div = 1;

	drto_ms = DIV_ROUND_UP_ULL((u64)MSEC_PER_SEC * drto_clks * drto_div,
				   host->bus_hz);

	dev_dbg(host->dev, "drto_ms: %u\n", drto_ms);

	 
	drto_ms += 10;

	spin_lock_irqsave(&host->irq_lock, irqflags);
	if (!test_bit(EVENT_DATA_COMPLETE, &host->pending_events))
		mod_timer(&host->dto_timer,
			  jiffies + msecs_to_jiffies(drto_ms));
	spin_unlock_irqrestore(&host->irq_lock, irqflags);
}

static bool dw_mci_clear_pending_cmd_complete(struct dw_mci *host)
{
	if (!test_bit(EVENT_CMD_COMPLETE, &host->pending_events))
		return false;

	 
	WARN_ON(del_timer_sync(&host->cto_timer));
	clear_bit(EVENT_CMD_COMPLETE, &host->pending_events);

	return true;
}

static bool dw_mci_clear_pending_data_complete(struct dw_mci *host)
{
	if (!test_bit(EVENT_DATA_COMPLETE, &host->pending_events))
		return false;

	 
	WARN_ON(del_timer_sync(&host->dto_timer));
	clear_bit(EVENT_DATA_COMPLETE, &host->pending_events);

	return true;
}

static void dw_mci_tasklet_func(struct tasklet_struct *t)
{
	struct dw_mci *host = from_tasklet(host, t, tasklet);
	struct mmc_data	*data;
	struct mmc_command *cmd;
	struct mmc_request *mrq;
	enum dw_mci_state state;
	enum dw_mci_state prev_state;
	unsigned int err;

	spin_lock(&host->lock);

	state = host->state;
	data = host->data;
	mrq = host->mrq;

	do {
		prev_state = state;

		switch (state) {
		case STATE_IDLE:
		case STATE_WAITING_CMD11_DONE:
			break;

		case STATE_SENDING_CMD11:
		case STATE_SENDING_CMD:
			if (!dw_mci_clear_pending_cmd_complete(host))
				break;

			cmd = host->cmd;
			host->cmd = NULL;
			set_bit(EVENT_CMD_COMPLETE, &host->completed_events);
			err = dw_mci_command_complete(host, cmd);
			if (cmd == mrq->sbc && !err) {
				__dw_mci_start_request(host, host->slot,
						       mrq->cmd);
				goto unlock;
			}

			if (cmd->data && err) {
				 
				if (err != -ETIMEDOUT &&
				    host->dir_status == DW_MCI_RECV_STATUS) {
					state = STATE_SENDING_DATA;
					continue;
				}

				send_stop_abort(host, data);
				dw_mci_stop_dma(host);
				state = STATE_SENDING_STOP;
				break;
			}

			if (!cmd->data || err) {
				dw_mci_request_end(host, mrq);
				goto unlock;
			}

			prev_state = state = STATE_SENDING_DATA;
			fallthrough;

		case STATE_SENDING_DATA:
			 
			if (test_and_clear_bit(EVENT_DATA_ERROR,
					       &host->pending_events)) {
				if (!(host->data_status & (SDMMC_INT_DRTO |
							   SDMMC_INT_EBE)))
					send_stop_abort(host, data);
				dw_mci_stop_dma(host);
				state = STATE_DATA_ERROR;
				break;
			}

			if (!test_and_clear_bit(EVENT_XFER_COMPLETE,
						&host->pending_events)) {
				 
				if (host->dir_status == DW_MCI_RECV_STATUS)
					dw_mci_set_drto(host);
				break;
			}

			set_bit(EVENT_XFER_COMPLETE, &host->completed_events);

			 
			if (test_and_clear_bit(EVENT_DATA_ERROR,
					       &host->pending_events)) {
				if (!(host->data_status & (SDMMC_INT_DRTO |
							   SDMMC_INT_EBE)))
					send_stop_abort(host, data);
				dw_mci_stop_dma(host);
				state = STATE_DATA_ERROR;
				break;
			}
			prev_state = state = STATE_DATA_BUSY;

			fallthrough;

		case STATE_DATA_BUSY:
			if (!dw_mci_clear_pending_data_complete(host)) {
				 
				if (host->dir_status == DW_MCI_RECV_STATUS)
					dw_mci_set_drto(host);
				break;
			}

			dw_mci_stop_fault_timer(host);
			host->data = NULL;
			set_bit(EVENT_DATA_COMPLETE, &host->completed_events);
			err = dw_mci_data_complete(host, data);

			if (!err) {
				if (!data->stop || mrq->sbc) {
					if (mrq->sbc && data->stop)
						data->stop->error = 0;
					dw_mci_request_end(host, mrq);
					goto unlock;
				}

				 
				if (data->stop)
					send_stop_abort(host, data);
			} else {
				 
				if (!test_bit(EVENT_CMD_COMPLETE,
					      &host->pending_events)) {
					host->cmd = NULL;
					dw_mci_request_end(host, mrq);
					goto unlock;
				}
			}

			 
			prev_state = state = STATE_SENDING_STOP;

			fallthrough;

		case STATE_SENDING_STOP:
			if (!dw_mci_clear_pending_cmd_complete(host))
				break;

			 
			if (mrq->cmd->error && mrq->data)
				dw_mci_reset(host);

			dw_mci_stop_fault_timer(host);
			host->cmd = NULL;
			host->data = NULL;

			if (!mrq->sbc && mrq->stop)
				dw_mci_command_complete(host, mrq->stop);
			else
				host->cmd_status = 0;

			dw_mci_request_end(host, mrq);
			goto unlock;

		case STATE_DATA_ERROR:
			if (!test_and_clear_bit(EVENT_XFER_COMPLETE,
						&host->pending_events))
				break;

			state = STATE_DATA_BUSY;
			break;
		}
	} while (state != prev_state);

	host->state = state;
unlock:
	spin_unlock(&host->lock);

}

 
static void dw_mci_set_part_bytes(struct dw_mci *host, void *buf, int cnt)
{
	memcpy((void *)&host->part_buf, buf, cnt);
	host->part_buf_count = cnt;
}

 
static int dw_mci_push_part_bytes(struct dw_mci *host, void *buf, int cnt)
{
	cnt = min(cnt, (1 << host->data_shift) - host->part_buf_count);
	memcpy((void *)&host->part_buf + host->part_buf_count, buf, cnt);
	host->part_buf_count += cnt;
	return cnt;
}

 
static int dw_mci_pull_part_bytes(struct dw_mci *host, void *buf, int cnt)
{
	cnt = min_t(int, cnt, host->part_buf_count);
	if (cnt) {
		memcpy(buf, (void *)&host->part_buf + host->part_buf_start,
		       cnt);
		host->part_buf_count -= cnt;
		host->part_buf_start += cnt;
	}
	return cnt;
}

 
static void dw_mci_pull_final_bytes(struct dw_mci *host, void *buf, int cnt)
{
	memcpy(buf, &host->part_buf, cnt);
	host->part_buf_start = cnt;
	host->part_buf_count = (1 << host->data_shift) - cnt;
}

static void dw_mci_push_data16(struct dw_mci *host, void *buf, int cnt)
{
	struct mmc_data *data = host->data;
	int init_cnt = cnt;

	 
	if (unlikely(host->part_buf_count)) {
		int len = dw_mci_push_part_bytes(host, buf, cnt);

		buf += len;
		cnt -= len;
		if (host->part_buf_count == 2) {
			mci_fifo_writew(host->fifo_reg, host->part_buf16);
			host->part_buf_count = 0;
		}
	}
#ifndef CONFIG_HAVE_EFFICIENT_UNALIGNED_ACCESS
	if (unlikely((unsigned long)buf & 0x1)) {
		while (cnt >= 2) {
			u16 aligned_buf[64];
			int len = min(cnt & -2, (int)sizeof(aligned_buf));
			int items = len >> 1;
			int i;
			 
			memcpy(aligned_buf, buf, len);
			buf += len;
			cnt -= len;
			 
			for (i = 0; i < items; ++i)
				mci_fifo_writew(host->fifo_reg, aligned_buf[i]);
		}
	} else
#endif
	{
		u16 *pdata = buf;

		for (; cnt >= 2; cnt -= 2)
			mci_fifo_writew(host->fifo_reg, *pdata++);
		buf = pdata;
	}
	 
	if (cnt) {
		dw_mci_set_part_bytes(host, buf, cnt);
		  
		if ((data->bytes_xfered + init_cnt) ==
		    (data->blksz * data->blocks))
			mci_fifo_writew(host->fifo_reg, host->part_buf16);
	}
}

static void dw_mci_pull_data16(struct dw_mci *host, void *buf, int cnt)
{
#ifndef CONFIG_HAVE_EFFICIENT_UNALIGNED_ACCESS
	if (unlikely((unsigned long)buf & 0x1)) {
		while (cnt >= 2) {
			 
			u16 aligned_buf[64];
			int len = min(cnt & -2, (int)sizeof(aligned_buf));
			int items = len >> 1;
			int i;

			for (i = 0; i < items; ++i)
				aligned_buf[i] = mci_fifo_readw(host->fifo_reg);
			 
			memcpy(buf, aligned_buf, len);
			buf += len;
			cnt -= len;
		}
	} else
#endif
	{
		u16 *pdata = buf;

		for (; cnt >= 2; cnt -= 2)
			*pdata++ = mci_fifo_readw(host->fifo_reg);
		buf = pdata;
	}
	if (cnt) {
		host->part_buf16 = mci_fifo_readw(host->fifo_reg);
		dw_mci_pull_final_bytes(host, buf, cnt);
	}
}

static void dw_mci_push_data32(struct dw_mci *host, void *buf, int cnt)
{
	struct mmc_data *data = host->data;
	int init_cnt = cnt;

	 
	if (unlikely(host->part_buf_count)) {
		int len = dw_mci_push_part_bytes(host, buf, cnt);

		buf += len;
		cnt -= len;
		if (host->part_buf_count == 4) {
			mci_fifo_writel(host->fifo_reg,	host->part_buf32);
			host->part_buf_count = 0;
		}
	}
#ifndef CONFIG_HAVE_EFFICIENT_UNALIGNED_ACCESS
	if (unlikely((unsigned long)buf & 0x3)) {
		while (cnt >= 4) {
			u32 aligned_buf[32];
			int len = min(cnt & -4, (int)sizeof(aligned_buf));
			int items = len >> 2;
			int i;
			 
			memcpy(aligned_buf, buf, len);
			buf += len;
			cnt -= len;
			 
			for (i = 0; i < items; ++i)
				mci_fifo_writel(host->fifo_reg,	aligned_buf[i]);
		}
	} else
#endif
	{
		u32 *pdata = buf;

		for (; cnt >= 4; cnt -= 4)
			mci_fifo_writel(host->fifo_reg, *pdata++);
		buf = pdata;
	}
	 
	if (cnt) {
		dw_mci_set_part_bytes(host, buf, cnt);
		  
		if ((data->bytes_xfered + init_cnt) ==
		    (data->blksz * data->blocks))
			mci_fifo_writel(host->fifo_reg, host->part_buf32);
	}
}

static void dw_mci_pull_data32(struct dw_mci *host, void *buf, int cnt)
{
#ifndef CONFIG_HAVE_EFFICIENT_UNALIGNED_ACCESS
	if (unlikely((unsigned long)buf & 0x3)) {
		while (cnt >= 4) {
			 
			u32 aligned_buf[32];
			int len = min(cnt & -4, (int)sizeof(aligned_buf));
			int items = len >> 2;
			int i;

			for (i = 0; i < items; ++i)
				aligned_buf[i] = mci_fifo_readl(host->fifo_reg);
			 
			memcpy(buf, aligned_buf, len);
			buf += len;
			cnt -= len;
		}
	} else
#endif
	{
		u32 *pdata = buf;

		for (; cnt >= 4; cnt -= 4)
			*pdata++ = mci_fifo_readl(host->fifo_reg);
		buf = pdata;
	}
	if (cnt) {
		host->part_buf32 = mci_fifo_readl(host->fifo_reg);
		dw_mci_pull_final_bytes(host, buf, cnt);
	}
}

static void dw_mci_push_data64(struct dw_mci *host, void *buf, int cnt)
{
	struct mmc_data *data = host->data;
	int init_cnt = cnt;

	 
	if (unlikely(host->part_buf_count)) {
		int len = dw_mci_push_part_bytes(host, buf, cnt);

		buf += len;
		cnt -= len;

		if (host->part_buf_count == 8) {
			mci_fifo_writeq(host->fifo_reg,	host->part_buf);
			host->part_buf_count = 0;
		}
	}
#ifndef CONFIG_HAVE_EFFICIENT_UNALIGNED_ACCESS
	if (unlikely((unsigned long)buf & 0x7)) {
		while (cnt >= 8) {
			u64 aligned_buf[16];
			int len = min(cnt & -8, (int)sizeof(aligned_buf));
			int items = len >> 3;
			int i;
			 
			memcpy(aligned_buf, buf, len);
			buf += len;
			cnt -= len;
			 
			for (i = 0; i < items; ++i)
				mci_fifo_writeq(host->fifo_reg,	aligned_buf[i]);
		}
	} else
#endif
	{
		u64 *pdata = buf;

		for (; cnt >= 8; cnt -= 8)
			mci_fifo_writeq(host->fifo_reg, *pdata++);
		buf = pdata;
	}
	 
	if (cnt) {
		dw_mci_set_part_bytes(host, buf, cnt);
		 
		if ((data->bytes_xfered + init_cnt) ==
		    (data->blksz * data->blocks))
			mci_fifo_writeq(host->fifo_reg, host->part_buf);
	}
}

static void dw_mci_pull_data64(struct dw_mci *host, void *buf, int cnt)
{
#ifndef CONFIG_HAVE_EFFICIENT_UNALIGNED_ACCESS
	if (unlikely((unsigned long)buf & 0x7)) {
		while (cnt >= 8) {
			 
			u64 aligned_buf[16];
			int len = min(cnt & -8, (int)sizeof(aligned_buf));
			int items = len >> 3;
			int i;

			for (i = 0; i < items; ++i)
				aligned_buf[i] = mci_fifo_readq(host->fifo_reg);

			 
			memcpy(buf, aligned_buf, len);
			buf += len;
			cnt -= len;
		}
	} else
#endif
	{
		u64 *pdata = buf;

		for (; cnt >= 8; cnt -= 8)
			*pdata++ = mci_fifo_readq(host->fifo_reg);
		buf = pdata;
	}
	if (cnt) {
		host->part_buf = mci_fifo_readq(host->fifo_reg);
		dw_mci_pull_final_bytes(host, buf, cnt);
	}
}

static void dw_mci_pull_data(struct dw_mci *host, void *buf, int cnt)
{
	int len;

	 
	len = dw_mci_pull_part_bytes(host, buf, cnt);
	if (unlikely(len == cnt))
		return;
	buf += len;
	cnt -= len;

	 
	host->pull_data(host, buf, cnt);
}

static void dw_mci_read_data_pio(struct dw_mci *host, bool dto)
{
	struct sg_mapping_iter *sg_miter = &host->sg_miter;
	void *buf;
	unsigned int offset;
	struct mmc_data	*data = host->data;
	int shift = host->data_shift;
	u32 status;
	unsigned int len;
	unsigned int remain, fcnt;

	do {
		if (!sg_miter_next(sg_miter))
			goto done;

		host->sg = sg_miter->piter.sg;
		buf = sg_miter->addr;
		remain = sg_miter->length;
		offset = 0;

		do {
			fcnt = (SDMMC_GET_FCNT(mci_readl(host, STATUS))
					<< shift) + host->part_buf_count;
			len = min(remain, fcnt);
			if (!len)
				break;
			dw_mci_pull_data(host, (void *)(buf + offset), len);
			data->bytes_xfered += len;
			offset += len;
			remain -= len;
		} while (remain);

		sg_miter->consumed = offset;
		status = mci_readl(host, MINTSTS);
		mci_writel(host, RINTSTS, SDMMC_INT_RXDR);
	 
	} while ((status & SDMMC_INT_RXDR) ||
		 (dto && SDMMC_GET_FCNT(mci_readl(host, STATUS))));

	if (!remain) {
		if (!sg_miter_next(sg_miter))
			goto done;
		sg_miter->consumed = 0;
	}
	sg_miter_stop(sg_miter);
	return;

done:
	sg_miter_stop(sg_miter);
	host->sg = NULL;
	smp_wmb();  
	set_bit(EVENT_XFER_COMPLETE, &host->pending_events);
}

static void dw_mci_write_data_pio(struct dw_mci *host)
{
	struct sg_mapping_iter *sg_miter = &host->sg_miter;
	void *buf;
	unsigned int offset;
	struct mmc_data	*data = host->data;
	int shift = host->data_shift;
	u32 status;
	unsigned int len;
	unsigned int fifo_depth = host->fifo_depth;
	unsigned int remain, fcnt;

	do {
		if (!sg_miter_next(sg_miter))
			goto done;

		host->sg = sg_miter->piter.sg;
		buf = sg_miter->addr;
		remain = sg_miter->length;
		offset = 0;

		do {
			fcnt = ((fifo_depth -
				 SDMMC_GET_FCNT(mci_readl(host, STATUS)))
					<< shift) - host->part_buf_count;
			len = min(remain, fcnt);
			if (!len)
				break;
			host->push_data(host, (void *)(buf + offset), len);
			data->bytes_xfered += len;
			offset += len;
			remain -= len;
		} while (remain);

		sg_miter->consumed = offset;
		status = mci_readl(host, MINTSTS);
		mci_writel(host, RINTSTS, SDMMC_INT_TXDR);
	} while (status & SDMMC_INT_TXDR);  

	if (!remain) {
		if (!sg_miter_next(sg_miter))
			goto done;
		sg_miter->consumed = 0;
	}
	sg_miter_stop(sg_miter);
	return;

done:
	sg_miter_stop(sg_miter);
	host->sg = NULL;
	smp_wmb();  
	set_bit(EVENT_XFER_COMPLETE, &host->pending_events);
}

static void dw_mci_cmd_interrupt(struct dw_mci *host, u32 status)
{
	del_timer(&host->cto_timer);

	if (!host->cmd_status)
		host->cmd_status = status;

	smp_wmb();  

	set_bit(EVENT_CMD_COMPLETE, &host->pending_events);
	tasklet_schedule(&host->tasklet);

	dw_mci_start_fault_timer(host);
}

static void dw_mci_handle_cd(struct dw_mci *host)
{
	struct dw_mci_slot *slot = host->slot;

	mmc_detect_change(slot->mmc,
		msecs_to_jiffies(host->pdata->detect_delay_ms));
}

static irqreturn_t dw_mci_interrupt(int irq, void *dev_id)
{
	struct dw_mci *host = dev_id;
	u32 pending;
	struct dw_mci_slot *slot = host->slot;

	pending = mci_readl(host, MINTSTS);  

	if (pending) {
		 
		if ((host->state == STATE_SENDING_CMD11) &&
		    (pending & SDMMC_INT_VOLT_SWITCH)) {
			mci_writel(host, RINTSTS, SDMMC_INT_VOLT_SWITCH);
			pending &= ~SDMMC_INT_VOLT_SWITCH;

			 
			spin_lock(&host->irq_lock);
			dw_mci_cmd_interrupt(host, pending);
			spin_unlock(&host->irq_lock);

			del_timer(&host->cmd11_timer);
		}

		if (pending & DW_MCI_CMD_ERROR_FLAGS) {
			spin_lock(&host->irq_lock);

			del_timer(&host->cto_timer);
			mci_writel(host, RINTSTS, DW_MCI_CMD_ERROR_FLAGS);
			host->cmd_status = pending;
			smp_wmb();  
			set_bit(EVENT_CMD_COMPLETE, &host->pending_events);

			spin_unlock(&host->irq_lock);
		}

		if (pending & DW_MCI_DATA_ERROR_FLAGS) {
			spin_lock(&host->irq_lock);

			if (host->quirks & DW_MMC_QUIRK_EXTENDED_TMOUT)
				del_timer(&host->dto_timer);

			 
			mci_writel(host, RINTSTS, DW_MCI_DATA_ERROR_FLAGS);
			host->data_status = pending;
			smp_wmb();  
			set_bit(EVENT_DATA_ERROR, &host->pending_events);

			if (host->quirks & DW_MMC_QUIRK_EXTENDED_TMOUT)
				 
				set_bit(EVENT_DATA_COMPLETE,
					&host->pending_events);

			tasklet_schedule(&host->tasklet);

			spin_unlock(&host->irq_lock);
		}

		if (pending & SDMMC_INT_DATA_OVER) {
			spin_lock(&host->irq_lock);

			del_timer(&host->dto_timer);

			mci_writel(host, RINTSTS, SDMMC_INT_DATA_OVER);
			if (!host->data_status)
				host->data_status = pending;
			smp_wmb();  
			if (host->dir_status == DW_MCI_RECV_STATUS) {
				if (host->sg != NULL)
					dw_mci_read_data_pio(host, true);
			}
			set_bit(EVENT_DATA_COMPLETE, &host->pending_events);
			tasklet_schedule(&host->tasklet);

			spin_unlock(&host->irq_lock);
		}

		if (pending & SDMMC_INT_RXDR) {
			mci_writel(host, RINTSTS, SDMMC_INT_RXDR);
			if (host->dir_status == DW_MCI_RECV_STATUS && host->sg)
				dw_mci_read_data_pio(host, false);
		}

		if (pending & SDMMC_INT_TXDR) {
			mci_writel(host, RINTSTS, SDMMC_INT_TXDR);
			if (host->dir_status == DW_MCI_SEND_STATUS && host->sg)
				dw_mci_write_data_pio(host);
		}

		if (pending & SDMMC_INT_CMD_DONE) {
			spin_lock(&host->irq_lock);

			mci_writel(host, RINTSTS, SDMMC_INT_CMD_DONE);
			dw_mci_cmd_interrupt(host, pending);

			spin_unlock(&host->irq_lock);
		}

		if (pending & SDMMC_INT_CD) {
			mci_writel(host, RINTSTS, SDMMC_INT_CD);
			dw_mci_handle_cd(host);
		}

		if (pending & SDMMC_INT_SDIO(slot->sdio_id)) {
			mci_writel(host, RINTSTS,
				   SDMMC_INT_SDIO(slot->sdio_id));
			__dw_mci_enable_sdio_irq(slot, 0);
			sdio_signal_irq(slot->mmc);
		}

	}

	if (host->use_dma != TRANS_MODE_IDMAC)
		return IRQ_HANDLED;

	 
	if (host->dma_64bit_address == 1) {
		pending = mci_readl(host, IDSTS64);
		if (pending & (SDMMC_IDMAC_INT_TI | SDMMC_IDMAC_INT_RI)) {
			mci_writel(host, IDSTS64, SDMMC_IDMAC_INT_TI |
							SDMMC_IDMAC_INT_RI);
			mci_writel(host, IDSTS64, SDMMC_IDMAC_INT_NI);
			if (!test_bit(EVENT_DATA_ERROR, &host->pending_events))
				host->dma_ops->complete((void *)host);
		}
	} else {
		pending = mci_readl(host, IDSTS);
		if (pending & (SDMMC_IDMAC_INT_TI | SDMMC_IDMAC_INT_RI)) {
			mci_writel(host, IDSTS, SDMMC_IDMAC_INT_TI |
							SDMMC_IDMAC_INT_RI);
			mci_writel(host, IDSTS, SDMMC_IDMAC_INT_NI);
			if (!test_bit(EVENT_DATA_ERROR, &host->pending_events))
				host->dma_ops->complete((void *)host);
		}
	}

	return IRQ_HANDLED;
}

static int dw_mci_init_slot_caps(struct dw_mci_slot *slot)
{
	struct dw_mci *host = slot->host;
	const struct dw_mci_drv_data *drv_data = host->drv_data;
	struct mmc_host *mmc = slot->mmc;
	int ctrl_id;

	if (host->pdata->caps)
		mmc->caps = host->pdata->caps;

	if (host->pdata->pm_caps)
		mmc->pm_caps = host->pdata->pm_caps;

	if (drv_data)
		mmc->caps |= drv_data->common_caps;

	if (host->dev->of_node) {
		ctrl_id = of_alias_get_id(host->dev->of_node, "mshc");
		if (ctrl_id < 0)
			ctrl_id = 0;
	} else {
		ctrl_id = to_platform_device(host->dev)->id;
	}

	if (drv_data && drv_data->caps) {
		if (ctrl_id >= drv_data->num_caps) {
			dev_err(host->dev, "invalid controller id %d\n",
				ctrl_id);
			return -EINVAL;
		}
		mmc->caps |= drv_data->caps[ctrl_id];
	}

	if (host->pdata->caps2)
		mmc->caps2 = host->pdata->caps2;

	 
	if (host->minimum_speed)
		mmc->f_min = host->minimum_speed;
	else
		mmc->f_min = DW_MCI_FREQ_MIN;

	if (!mmc->f_max)
		mmc->f_max = DW_MCI_FREQ_MAX;

	 
	if (mmc->caps & MMC_CAP_SDIO_IRQ)
		mmc->caps2 |= MMC_CAP2_SDIO_IRQ_NOTHREAD;

	return 0;
}

static int dw_mci_init_slot(struct dw_mci *host)
{
	struct mmc_host *mmc;
	struct dw_mci_slot *slot;
	int ret;

	mmc = mmc_alloc_host(sizeof(struct dw_mci_slot), host->dev);
	if (!mmc)
		return -ENOMEM;

	slot = mmc_priv(mmc);
	slot->id = 0;
	slot->sdio_id = host->sdio_id0 + slot->id;
	slot->mmc = mmc;
	slot->host = host;
	host->slot = slot;

	mmc->ops = &dw_mci_ops;

	 
	ret = mmc_regulator_get_supply(mmc);
	if (ret)
		goto err_host_allocated;

	if (!mmc->ocr_avail)
		mmc->ocr_avail = MMC_VDD_32_33 | MMC_VDD_33_34;

	ret = mmc_of_parse(mmc);
	if (ret)
		goto err_host_allocated;

	ret = dw_mci_init_slot_caps(slot);
	if (ret)
		goto err_host_allocated;

	 
	if (host->use_dma == TRANS_MODE_IDMAC) {
		mmc->max_segs = host->ring_size;
		mmc->max_blk_size = 65535;
		mmc->max_seg_size = 0x1000;
		mmc->max_req_size = mmc->max_seg_size * host->ring_size;
		mmc->max_blk_count = mmc->max_req_size / 512;
	} else if (host->use_dma == TRANS_MODE_EDMAC) {
		mmc->max_segs = 64;
		mmc->max_blk_size = 65535;
		mmc->max_blk_count = 65535;
		mmc->max_req_size =
				mmc->max_blk_size * mmc->max_blk_count;
		mmc->max_seg_size = mmc->max_req_size;
	} else {
		 
		mmc->max_segs = 64;
		mmc->max_blk_size = 65535;  
		mmc->max_blk_count = 512;
		mmc->max_req_size = mmc->max_blk_size *
				    mmc->max_blk_count;
		mmc->max_seg_size = mmc->max_req_size;
	}

	dw_mci_get_cd(mmc);

	ret = mmc_add_host(mmc);
	if (ret)
		goto err_host_allocated;

#if defined(CONFIG_DEBUG_FS)
	dw_mci_init_debugfs(slot);
#endif

	return 0;

err_host_allocated:
	mmc_free_host(mmc);
	return ret;
}

static void dw_mci_cleanup_slot(struct dw_mci_slot *slot)
{
	 
	mmc_remove_host(slot->mmc);
	slot->host->slot = NULL;
	mmc_free_host(slot->mmc);
}

static void dw_mci_init_dma(struct dw_mci *host)
{
	int addr_config;
	struct device *dev = host->dev;

	 
	host->use_dma = SDMMC_GET_TRANS_MODE(mci_readl(host, HCON));
	if (host->use_dma == DMA_INTERFACE_IDMA) {
		host->use_dma = TRANS_MODE_IDMAC;
	} else if (host->use_dma == DMA_INTERFACE_DWDMA ||
		   host->use_dma == DMA_INTERFACE_GDMA) {
		host->use_dma = TRANS_MODE_EDMAC;
	} else {
		goto no_dma;
	}

	 
	if (host->use_dma == TRANS_MODE_IDMAC) {
		 
		addr_config = SDMMC_GET_ADDR_CONFIG(mci_readl(host, HCON));

		if (addr_config == 1) {
			 
			host->dma_64bit_address = 1;
			dev_info(host->dev,
				 "IDMAC supports 64-bit address mode.\n");
			if (!dma_set_mask(host->dev, DMA_BIT_MASK(64)))
				dma_set_coherent_mask(host->dev,
						      DMA_BIT_MASK(64));
		} else {
			 
			host->dma_64bit_address = 0;
			dev_info(host->dev,
				 "IDMAC supports 32-bit address mode.\n");
		}

		 
		host->sg_cpu = dmam_alloc_coherent(host->dev,
						   DESC_RING_BUF_SZ,
						   &host->sg_dma, GFP_KERNEL);
		if (!host->sg_cpu) {
			dev_err(host->dev,
				"%s: could not alloc DMA memory\n",
				__func__);
			goto no_dma;
		}

		host->dma_ops = &dw_mci_idmac_ops;
		dev_info(host->dev, "Using internal DMA controller.\n");
	} else {
		 
		if ((device_property_string_array_count(dev, "dma-names") < 0) ||
		    !device_property_present(dev, "dmas")) {
			goto no_dma;
		}
		host->dma_ops = &dw_mci_edmac_ops;
		dev_info(host->dev, "Using external DMA controller.\n");
	}

	if (host->dma_ops->init && host->dma_ops->start &&
	    host->dma_ops->stop && host->dma_ops->cleanup) {
		if (host->dma_ops->init(host)) {
			dev_err(host->dev, "%s: Unable to initialize DMA Controller.\n",
				__func__);
			goto no_dma;
		}
	} else {
		dev_err(host->dev, "DMA initialization not found.\n");
		goto no_dma;
	}

	return;

no_dma:
	dev_info(host->dev, "Using PIO mode.\n");
	host->use_dma = TRANS_MODE_PIO;
}

static void dw_mci_cmd11_timer(struct timer_list *t)
{
	struct dw_mci *host = from_timer(host, t, cmd11_timer);

	if (host->state != STATE_SENDING_CMD11) {
		dev_warn(host->dev, "Unexpected CMD11 timeout\n");
		return;
	}

	host->cmd_status = SDMMC_INT_RTO;
	set_bit(EVENT_CMD_COMPLETE, &host->pending_events);
	tasklet_schedule(&host->tasklet);
}

static void dw_mci_cto_timer(struct timer_list *t)
{
	struct dw_mci *host = from_timer(host, t, cto_timer);
	unsigned long irqflags;
	u32 pending;

	spin_lock_irqsave(&host->irq_lock, irqflags);

	 
	pending = mci_readl(host, MINTSTS);  
	if (pending & (DW_MCI_CMD_ERROR_FLAGS | SDMMC_INT_CMD_DONE)) {
		 
		dev_warn(host->dev, "Unexpected interrupt latency\n");
		goto exit;
	}
	if (test_bit(EVENT_CMD_COMPLETE, &host->pending_events)) {
		 
		dev_warn(host->dev, "CTO timeout when already completed\n");
		goto exit;
	}

	 
	switch (host->state) {
	case STATE_SENDING_CMD11:
	case STATE_SENDING_CMD:
	case STATE_SENDING_STOP:
		 
		host->cmd_status = SDMMC_INT_RTO;
		set_bit(EVENT_CMD_COMPLETE, &host->pending_events);
		tasklet_schedule(&host->tasklet);
		break;
	default:
		dev_warn(host->dev, "Unexpected command timeout, state %d\n",
			 host->state);
		break;
	}

exit:
	spin_unlock_irqrestore(&host->irq_lock, irqflags);
}

static void dw_mci_dto_timer(struct timer_list *t)
{
	struct dw_mci *host = from_timer(host, t, dto_timer);
	unsigned long irqflags;
	u32 pending;

	spin_lock_irqsave(&host->irq_lock, irqflags);

	 
	pending = mci_readl(host, MINTSTS);  
	if (pending & SDMMC_INT_DATA_OVER) {
		 
		dev_warn(host->dev, "Unexpected data interrupt latency\n");
		goto exit;
	}
	if (test_bit(EVENT_DATA_COMPLETE, &host->pending_events)) {
		 
		dev_warn(host->dev, "DTO timeout when already completed\n");
		goto exit;
	}

	 
	switch (host->state) {
	case STATE_SENDING_DATA:
	case STATE_DATA_BUSY:
		 
		host->data_status = SDMMC_INT_DRTO;
		set_bit(EVENT_DATA_ERROR, &host->pending_events);
		set_bit(EVENT_DATA_COMPLETE, &host->pending_events);
		tasklet_schedule(&host->tasklet);
		break;
	default:
		dev_warn(host->dev, "Unexpected data timeout, state %d\n",
			 host->state);
		break;
	}

exit:
	spin_unlock_irqrestore(&host->irq_lock, irqflags);
}

#ifdef CONFIG_OF
static struct dw_mci_board *dw_mci_parse_dt(struct dw_mci *host)
{
	struct dw_mci_board *pdata;
	struct device *dev = host->dev;
	const struct dw_mci_drv_data *drv_data = host->drv_data;
	int ret;
	u32 clock_frequency;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return ERR_PTR(-ENOMEM);

	 
	pdata->rstc = devm_reset_control_get_optional_exclusive(dev, "reset");
	if (IS_ERR(pdata->rstc))
		return ERR_CAST(pdata->rstc);

	if (device_property_read_u32(dev, "fifo-depth", &pdata->fifo_depth))
		dev_info(dev,
			 "fifo-depth property not found, using value of FIFOTH register as default\n");

	device_property_read_u32(dev, "card-detect-delay",
				 &pdata->detect_delay_ms);

	device_property_read_u32(dev, "data-addr", &host->data_addr_override);

	if (device_property_present(dev, "fifo-watermark-aligned"))
		host->wm_aligned = true;

	if (!device_property_read_u32(dev, "clock-frequency", &clock_frequency))
		pdata->bus_hz = clock_frequency;

	if (drv_data && drv_data->parse_dt) {
		ret = drv_data->parse_dt(host);
		if (ret)
			return ERR_PTR(ret);
	}

	return pdata;
}

#else  
static struct dw_mci_board *dw_mci_parse_dt(struct dw_mci *host)
{
	return ERR_PTR(-EINVAL);
}
#endif  

static void dw_mci_enable_cd(struct dw_mci *host)
{
	unsigned long irqflags;
	u32 temp;

	 
	if (host->slot->mmc->caps & MMC_CAP_NEEDS_POLL)
		return;

	if (mmc_gpio_get_cd(host->slot->mmc) < 0) {
		spin_lock_irqsave(&host->irq_lock, irqflags);
		temp = mci_readl(host, INTMASK);
		temp  |= SDMMC_INT_CD;
		mci_writel(host, INTMASK, temp);
		spin_unlock_irqrestore(&host->irq_lock, irqflags);
	}
}

int dw_mci_probe(struct dw_mci *host)
{
	const struct dw_mci_drv_data *drv_data = host->drv_data;
	int width, i, ret = 0;
	u32 fifo_size;

	if (!host->pdata) {
		host->pdata = dw_mci_parse_dt(host);
		if (IS_ERR(host->pdata))
			return dev_err_probe(host->dev, PTR_ERR(host->pdata),
					     "platform data not available\n");
	}

	host->biu_clk = devm_clk_get(host->dev, "biu");
	if (IS_ERR(host->biu_clk)) {
		dev_dbg(host->dev, "biu clock not available\n");
	} else {
		ret = clk_prepare_enable(host->biu_clk);
		if (ret) {
			dev_err(host->dev, "failed to enable biu clock\n");
			return ret;
		}
	}

	host->ciu_clk = devm_clk_get(host->dev, "ciu");
	if (IS_ERR(host->ciu_clk)) {
		dev_dbg(host->dev, "ciu clock not available\n");
		host->bus_hz = host->pdata->bus_hz;
	} else {
		ret = clk_prepare_enable(host->ciu_clk);
		if (ret) {
			dev_err(host->dev, "failed to enable ciu clock\n");
			goto err_clk_biu;
		}

		if (host->pdata->bus_hz) {
			ret = clk_set_rate(host->ciu_clk, host->pdata->bus_hz);
			if (ret)
				dev_warn(host->dev,
					 "Unable to set bus rate to %uHz\n",
					 host->pdata->bus_hz);
		}
		host->bus_hz = clk_get_rate(host->ciu_clk);
	}

	if (!host->bus_hz) {
		dev_err(host->dev,
			"Platform data must supply bus speed\n");
		ret = -ENODEV;
		goto err_clk_ciu;
	}

	if (host->pdata->rstc) {
		reset_control_assert(host->pdata->rstc);
		usleep_range(10, 50);
		reset_control_deassert(host->pdata->rstc);
	}

	if (drv_data && drv_data->init) {
		ret = drv_data->init(host);
		if (ret) {
			dev_err(host->dev,
				"implementation specific init failed\n");
			goto err_clk_ciu;
		}
	}

	timer_setup(&host->cmd11_timer, dw_mci_cmd11_timer, 0);
	timer_setup(&host->cto_timer, dw_mci_cto_timer, 0);
	timer_setup(&host->dto_timer, dw_mci_dto_timer, 0);

	spin_lock_init(&host->lock);
	spin_lock_init(&host->irq_lock);
	INIT_LIST_HEAD(&host->queue);

	dw_mci_init_fault(host);

	 
	i = SDMMC_GET_HDATA_WIDTH(mci_readl(host, HCON));
	if (!i) {
		host->push_data = dw_mci_push_data16;
		host->pull_data = dw_mci_pull_data16;
		width = 16;
		host->data_shift = 1;
	} else if (i == 2) {
		host->push_data = dw_mci_push_data64;
		host->pull_data = dw_mci_pull_data64;
		width = 64;
		host->data_shift = 3;
	} else {
		 
		WARN((i != 1),
		     "HCON reports a reserved host data width!\n"
		     "Defaulting to 32-bit access.\n");
		host->push_data = dw_mci_push_data32;
		host->pull_data = dw_mci_pull_data32;
		width = 32;
		host->data_shift = 2;
	}

	 
	if (!dw_mci_ctrl_reset(host, SDMMC_CTRL_ALL_RESET_FLAGS)) {
		ret = -ENODEV;
		goto err_clk_ciu;
	}

	host->dma_ops = host->pdata->dma_ops;
	dw_mci_init_dma(host);

	 
	mci_writel(host, RINTSTS, 0xFFFFFFFF);
	mci_writel(host, INTMASK, 0);  

	 
	mci_writel(host, TMOUT, 0xFFFFFFFF);

	 
	if (!host->pdata->fifo_depth) {
		 
		fifo_size = mci_readl(host, FIFOTH);
		fifo_size = 1 + ((fifo_size >> 16) & 0xfff);
	} else {
		fifo_size = host->pdata->fifo_depth;
	}
	host->fifo_depth = fifo_size;
	host->fifoth_val =
		SDMMC_SET_FIFOTH(0x2, fifo_size / 2 - 1, fifo_size / 2);
	mci_writel(host, FIFOTH, host->fifoth_val);

	 
	mci_writel(host, CLKENA, 0);
	mci_writel(host, CLKSRC, 0);

	 
	host->verid = SDMMC_GET_VERID(mci_readl(host, VERID));
	dev_info(host->dev, "Version ID is %04x\n", host->verid);

	if (host->data_addr_override)
		host->fifo_reg = host->regs + host->data_addr_override;
	else if (host->verid < DW_MMC_240A)
		host->fifo_reg = host->regs + DATA_OFFSET;
	else
		host->fifo_reg = host->regs + DATA_240A_OFFSET;

	tasklet_setup(&host->tasklet, dw_mci_tasklet_func);
	ret = devm_request_irq(host->dev, host->irq, dw_mci_interrupt,
			       host->irq_flags, "dw-mci", host);
	if (ret)
		goto err_dmaunmap;

	 
	mci_writel(host, INTMASK, SDMMC_INT_CMD_DONE | SDMMC_INT_DATA_OVER |
		   SDMMC_INT_TXDR | SDMMC_INT_RXDR |
		   DW_MCI_ERROR_FLAGS);
	 
	mci_writel(host, CTRL, SDMMC_CTRL_INT_ENABLE);

	dev_info(host->dev,
		 "DW MMC controller at irq %d,%d bit host data width,%u deep fifo\n",
		 host->irq, width, fifo_size);

	 
	ret = dw_mci_init_slot(host);
	if (ret) {
		dev_dbg(host->dev, "slot %d init failed\n", i);
		goto err_dmaunmap;
	}

	 
	dw_mci_enable_cd(host);

	return 0;

err_dmaunmap:
	if (host->use_dma && host->dma_ops->exit)
		host->dma_ops->exit(host);

	reset_control_assert(host->pdata->rstc);

err_clk_ciu:
	clk_disable_unprepare(host->ciu_clk);

err_clk_biu:
	clk_disable_unprepare(host->biu_clk);

	return ret;
}
EXPORT_SYMBOL(dw_mci_probe);

void dw_mci_remove(struct dw_mci *host)
{
	dev_dbg(host->dev, "remove slot\n");
	if (host->slot)
		dw_mci_cleanup_slot(host->slot);

	mci_writel(host, RINTSTS, 0xFFFFFFFF);
	mci_writel(host, INTMASK, 0);  

	 
	mci_writel(host, CLKENA, 0);
	mci_writel(host, CLKSRC, 0);

	if (host->use_dma && host->dma_ops->exit)
		host->dma_ops->exit(host);

	reset_control_assert(host->pdata->rstc);

	clk_disable_unprepare(host->ciu_clk);
	clk_disable_unprepare(host->biu_clk);
}
EXPORT_SYMBOL(dw_mci_remove);



#ifdef CONFIG_PM
int dw_mci_runtime_suspend(struct device *dev)
{
	struct dw_mci *host = dev_get_drvdata(dev);

	if (host->use_dma && host->dma_ops->exit)
		host->dma_ops->exit(host);

	clk_disable_unprepare(host->ciu_clk);

	if (host->slot &&
	    (mmc_can_gpio_cd(host->slot->mmc) ||
	     !mmc_card_is_removable(host->slot->mmc)))
		clk_disable_unprepare(host->biu_clk);

	return 0;
}
EXPORT_SYMBOL(dw_mci_runtime_suspend);

int dw_mci_runtime_resume(struct device *dev)
{
	int ret = 0;
	struct dw_mci *host = dev_get_drvdata(dev);

	if (host->slot &&
	    (mmc_can_gpio_cd(host->slot->mmc) ||
	     !mmc_card_is_removable(host->slot->mmc))) {
		ret = clk_prepare_enable(host->biu_clk);
		if (ret)
			return ret;
	}

	ret = clk_prepare_enable(host->ciu_clk);
	if (ret)
		goto err;

	if (!dw_mci_ctrl_reset(host, SDMMC_CTRL_ALL_RESET_FLAGS)) {
		clk_disable_unprepare(host->ciu_clk);
		ret = -ENODEV;
		goto err;
	}

	if (host->use_dma && host->dma_ops->init)
		host->dma_ops->init(host);

	 
	mci_writel(host, FIFOTH, host->fifoth_val);
	host->prev_blksz = 0;

	 
	mci_writel(host, TMOUT, 0xFFFFFFFF);

	mci_writel(host, RINTSTS, 0xFFFFFFFF);
	mci_writel(host, INTMASK, SDMMC_INT_CMD_DONE | SDMMC_INT_DATA_OVER |
		   SDMMC_INT_TXDR | SDMMC_INT_RXDR |
		   DW_MCI_ERROR_FLAGS);
	mci_writel(host, CTRL, SDMMC_CTRL_INT_ENABLE);


	if (host->slot && host->slot->mmc->pm_flags & MMC_PM_KEEP_POWER)
		dw_mci_set_ios(host->slot->mmc, &host->slot->mmc->ios);

	 
	dw_mci_setup_bus(host->slot, true);

	 
	if (sdio_irq_claimed(host->slot->mmc))
		__dw_mci_enable_sdio_irq(host->slot, 1);

	 
	dw_mci_enable_cd(host);

	return 0;

err:
	if (host->slot &&
	    (mmc_can_gpio_cd(host->slot->mmc) ||
	     !mmc_card_is_removable(host->slot->mmc)))
		clk_disable_unprepare(host->biu_clk);

	return ret;
}
EXPORT_SYMBOL(dw_mci_runtime_resume);
#endif  

static int __init dw_mci_init(void)
{
	pr_info("Synopsys Designware Multimedia Card Interface Driver\n");
	return 0;
}

static void __exit dw_mci_exit(void)
{
}

module_init(dw_mci_init);
module_exit(dw_mci_exit);

MODULE_DESCRIPTION("DW Multimedia Card Interface driver");
MODULE_AUTHOR("NXP Semiconductor VietNam");
MODULE_AUTHOR("Imagination Technologies Ltd");
MODULE_LICENSE("GPL v2");
