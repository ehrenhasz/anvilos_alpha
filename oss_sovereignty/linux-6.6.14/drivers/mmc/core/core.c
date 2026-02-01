
 
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/completion.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/pagemap.h>
#include <linux/err.h>
#include <linux/leds.h>
#include <linux/scatterlist.h>
#include <linux/log2.h>
#include <linux/pm_runtime.h>
#include <linux/pm_wakeup.h>
#include <linux/suspend.h>
#include <linux/fault-inject.h>
#include <linux/random.h>
#include <linux/slab.h>
#include <linux/of.h>

#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/sd.h>
#include <linux/mmc/slot-gpio.h>

#define CREATE_TRACE_POINTS
#include <trace/events/mmc.h>

#include "core.h"
#include "card.h"
#include "crypto.h"
#include "bus.h"
#include "host.h"
#include "sdio_bus.h"
#include "pwrseq.h"

#include "mmc_ops.h"
#include "sd_ops.h"
#include "sdio_ops.h"

 
#define MMC_ERASE_TIMEOUT_MS	(60 * 1000)  
#define SD_DISCARD_TIMEOUT_MS	(250)

static const unsigned freqs[] = { 400000, 300000, 200000, 100000 };

 
bool use_spi_crc = 1;
module_param(use_spi_crc, bool, 0);

static int mmc_schedule_delayed_work(struct delayed_work *work,
				     unsigned long delay)
{
	 
	return queue_delayed_work(system_freezable_wq, work, delay);
}

#ifdef CONFIG_FAIL_MMC_REQUEST

 
static void mmc_should_fail_request(struct mmc_host *host,
				    struct mmc_request *mrq)
{
	struct mmc_command *cmd = mrq->cmd;
	struct mmc_data *data = mrq->data;
	static const int data_errors[] = {
		-ETIMEDOUT,
		-EILSEQ,
		-EIO,
	};

	if (!data)
		return;

	if ((cmd && cmd->error) || data->error ||
	    !should_fail(&host->fail_mmc_request, data->blksz * data->blocks))
		return;

	data->error = data_errors[get_random_u32_below(ARRAY_SIZE(data_errors))];
	data->bytes_xfered = get_random_u32_below(data->bytes_xfered >> 9) << 9;
}

#else  

static inline void mmc_should_fail_request(struct mmc_host *host,
					   struct mmc_request *mrq)
{
}

#endif  

static inline void mmc_complete_cmd(struct mmc_request *mrq)
{
	if (mrq->cap_cmd_during_tfr && !completion_done(&mrq->cmd_completion))
		complete_all(&mrq->cmd_completion);
}

void mmc_command_done(struct mmc_host *host, struct mmc_request *mrq)
{
	if (!mrq->cap_cmd_during_tfr)
		return;

	mmc_complete_cmd(mrq);

	pr_debug("%s: cmd done, tfr ongoing (CMD%u)\n",
		 mmc_hostname(host), mrq->cmd->opcode);
}
EXPORT_SYMBOL(mmc_command_done);

 
void mmc_request_done(struct mmc_host *host, struct mmc_request *mrq)
{
	struct mmc_command *cmd = mrq->cmd;
	int err = cmd->error;

	 
	if (!mmc_op_tuning(cmd->opcode) &&
	    !host->retune_crc_disable &&
	    (err == -EILSEQ || (mrq->sbc && mrq->sbc->error == -EILSEQ) ||
	    (mrq->data && mrq->data->error == -EILSEQ) ||
	    (mrq->stop && mrq->stop->error == -EILSEQ)))
		mmc_retune_needed(host);

	if (err && cmd->retries && mmc_host_is_spi(host)) {
		if (cmd->resp[0] & R1_SPI_ILLEGAL_COMMAND)
			cmd->retries = 0;
	}

	if (host->ongoing_mrq == mrq)
		host->ongoing_mrq = NULL;

	mmc_complete_cmd(mrq);

	trace_mmc_request_done(host, mrq);

	 
	if (!err || !cmd->retries || mmc_card_removed(host->card)) {
		mmc_should_fail_request(host, mrq);

		if (!host->ongoing_mrq)
			led_trigger_event(host->led, LED_OFF);

		if (mrq->sbc) {
			pr_debug("%s: req done <CMD%u>: %d: %08x %08x %08x %08x\n",
				mmc_hostname(host), mrq->sbc->opcode,
				mrq->sbc->error,
				mrq->sbc->resp[0], mrq->sbc->resp[1],
				mrq->sbc->resp[2], mrq->sbc->resp[3]);
		}

		pr_debug("%s: req done (CMD%u): %d: %08x %08x %08x %08x\n",
			mmc_hostname(host), cmd->opcode, err,
			cmd->resp[0], cmd->resp[1],
			cmd->resp[2], cmd->resp[3]);

		if (mrq->data) {
			pr_debug("%s:     %d bytes transferred: %d\n",
				mmc_hostname(host),
				mrq->data->bytes_xfered, mrq->data->error);
		}

		if (mrq->stop) {
			pr_debug("%s:     (CMD%u): %d: %08x %08x %08x %08x\n",
				mmc_hostname(host), mrq->stop->opcode,
				mrq->stop->error,
				mrq->stop->resp[0], mrq->stop->resp[1],
				mrq->stop->resp[2], mrq->stop->resp[3]);
		}
	}
	 
	if (mrq->done)
		mrq->done(mrq);
}

EXPORT_SYMBOL(mmc_request_done);

static void __mmc_start_request(struct mmc_host *host, struct mmc_request *mrq)
{
	int err;

	 
	err = mmc_retune(host);
	if (err) {
		mrq->cmd->error = err;
		mmc_request_done(host, mrq);
		return;
	}

	 
	if (sdio_is_io_busy(mrq->cmd->opcode, mrq->cmd->arg) &&
	    host->ops->card_busy) {
		int tries = 500;  

		while (host->ops->card_busy(host) && --tries)
			mmc_delay(1);

		if (tries == 0) {
			mrq->cmd->error = -EBUSY;
			mmc_request_done(host, mrq);
			return;
		}
	}

	if (mrq->cap_cmd_during_tfr) {
		host->ongoing_mrq = mrq;
		 
		reinit_completion(&mrq->cmd_completion);
	}

	trace_mmc_request_start(host, mrq);

	if (host->cqe_on)
		host->cqe_ops->cqe_off(host);

	host->ops->request(host, mrq);
}

static void mmc_mrq_pr_debug(struct mmc_host *host, struct mmc_request *mrq,
			     bool cqe)
{
	if (mrq->sbc) {
		pr_debug("<%s: starting CMD%u arg %08x flags %08x>\n",
			 mmc_hostname(host), mrq->sbc->opcode,
			 mrq->sbc->arg, mrq->sbc->flags);
	}

	if (mrq->cmd) {
		pr_debug("%s: starting %sCMD%u arg %08x flags %08x\n",
			 mmc_hostname(host), cqe ? "CQE direct " : "",
			 mrq->cmd->opcode, mrq->cmd->arg, mrq->cmd->flags);
	} else if (cqe) {
		pr_debug("%s: starting CQE transfer for tag %d blkaddr %u\n",
			 mmc_hostname(host), mrq->tag, mrq->data->blk_addr);
	}

	if (mrq->data) {
		pr_debug("%s:     blksz %d blocks %d flags %08x "
			"tsac %d ms nsac %d\n",
			mmc_hostname(host), mrq->data->blksz,
			mrq->data->blocks, mrq->data->flags,
			mrq->data->timeout_ns / 1000000,
			mrq->data->timeout_clks);
	}

	if (mrq->stop) {
		pr_debug("%s:     CMD%u arg %08x flags %08x\n",
			 mmc_hostname(host), mrq->stop->opcode,
			 mrq->stop->arg, mrq->stop->flags);
	}
}

static int mmc_mrq_prep(struct mmc_host *host, struct mmc_request *mrq)
{
	unsigned int i, sz = 0;
	struct scatterlist *sg;

	if (mrq->cmd) {
		mrq->cmd->error = 0;
		mrq->cmd->mrq = mrq;
		mrq->cmd->data = mrq->data;
	}
	if (mrq->sbc) {
		mrq->sbc->error = 0;
		mrq->sbc->mrq = mrq;
	}
	if (mrq->data) {
		if (mrq->data->blksz > host->max_blk_size ||
		    mrq->data->blocks > host->max_blk_count ||
		    mrq->data->blocks * mrq->data->blksz > host->max_req_size)
			return -EINVAL;

		for_each_sg(mrq->data->sg, sg, mrq->data->sg_len, i)
			sz += sg->length;
		if (sz != mrq->data->blocks * mrq->data->blksz)
			return -EINVAL;

		mrq->data->error = 0;
		mrq->data->mrq = mrq;
		if (mrq->stop) {
			mrq->data->stop = mrq->stop;
			mrq->stop->error = 0;
			mrq->stop->mrq = mrq;
		}
	}

	return 0;
}

int mmc_start_request(struct mmc_host *host, struct mmc_request *mrq)
{
	int err;

	init_completion(&mrq->cmd_completion);

	mmc_retune_hold(host);

	if (mmc_card_removed(host->card))
		return -ENOMEDIUM;

	mmc_mrq_pr_debug(host, mrq, false);

	WARN_ON(!host->claimed);

	err = mmc_mrq_prep(host, mrq);
	if (err)
		return err;

	led_trigger_event(host->led, LED_FULL);
	__mmc_start_request(host, mrq);

	return 0;
}
EXPORT_SYMBOL(mmc_start_request);

static void mmc_wait_done(struct mmc_request *mrq)
{
	complete(&mrq->completion);
}

static inline void mmc_wait_ongoing_tfr_cmd(struct mmc_host *host)
{
	struct mmc_request *ongoing_mrq = READ_ONCE(host->ongoing_mrq);

	 
	if (ongoing_mrq && !completion_done(&ongoing_mrq->cmd_completion))
		wait_for_completion(&ongoing_mrq->cmd_completion);
}

static int __mmc_start_req(struct mmc_host *host, struct mmc_request *mrq)
{
	int err;

	mmc_wait_ongoing_tfr_cmd(host);

	init_completion(&mrq->completion);
	mrq->done = mmc_wait_done;

	err = mmc_start_request(host, mrq);
	if (err) {
		mrq->cmd->error = err;
		mmc_complete_cmd(mrq);
		complete(&mrq->completion);
	}

	return err;
}

void mmc_wait_for_req_done(struct mmc_host *host, struct mmc_request *mrq)
{
	struct mmc_command *cmd;

	while (1) {
		wait_for_completion(&mrq->completion);

		cmd = mrq->cmd;

		if (!cmd->error || !cmd->retries ||
		    mmc_card_removed(host->card))
			break;

		mmc_retune_recheck(host);

		pr_debug("%s: req failed (CMD%u): %d, retrying...\n",
			 mmc_hostname(host), cmd->opcode, cmd->error);
		cmd->retries--;
		cmd->error = 0;
		__mmc_start_request(host, mrq);
	}

	mmc_retune_release(host);
}
EXPORT_SYMBOL(mmc_wait_for_req_done);

 
int mmc_cqe_start_req(struct mmc_host *host, struct mmc_request *mrq)
{
	int err;

	 
	err = mmc_retune(host);
	if (err)
		goto out_err;

	mrq->host = host;

	mmc_mrq_pr_debug(host, mrq, true);

	err = mmc_mrq_prep(host, mrq);
	if (err)
		goto out_err;

	err = host->cqe_ops->cqe_request(host, mrq);
	if (err)
		goto out_err;

	trace_mmc_request_start(host, mrq);

	return 0;

out_err:
	if (mrq->cmd) {
		pr_debug("%s: failed to start CQE direct CMD%u, error %d\n",
			 mmc_hostname(host), mrq->cmd->opcode, err);
	} else {
		pr_debug("%s: failed to start CQE transfer for tag %d, error %d\n",
			 mmc_hostname(host), mrq->tag, err);
	}
	return err;
}
EXPORT_SYMBOL(mmc_cqe_start_req);

 
void mmc_cqe_request_done(struct mmc_host *host, struct mmc_request *mrq)
{
	mmc_should_fail_request(host, mrq);

	 
	if ((mrq->cmd && mrq->cmd->error == -EILSEQ) ||
	    (mrq->data && mrq->data->error == -EILSEQ))
		mmc_retune_needed(host);

	trace_mmc_request_done(host, mrq);

	if (mrq->cmd) {
		pr_debug("%s: CQE req done (direct CMD%u): %d\n",
			 mmc_hostname(host), mrq->cmd->opcode, mrq->cmd->error);
	} else {
		pr_debug("%s: CQE transfer done tag %d\n",
			 mmc_hostname(host), mrq->tag);
	}

	if (mrq->data) {
		pr_debug("%s:     %d bytes transferred: %d\n",
			 mmc_hostname(host),
			 mrq->data->bytes_xfered, mrq->data->error);
	}

	mrq->done(mrq);
}
EXPORT_SYMBOL(mmc_cqe_request_done);

 
void mmc_cqe_post_req(struct mmc_host *host, struct mmc_request *mrq)
{
	if (host->cqe_ops->cqe_post_req)
		host->cqe_ops->cqe_post_req(host, mrq);
}
EXPORT_SYMBOL(mmc_cqe_post_req);

 
#define MMC_CQE_RECOVERY_TIMEOUT	1000

 
int mmc_cqe_recovery(struct mmc_host *host)
{
	struct mmc_command cmd;
	int err;

	mmc_retune_hold_now(host);

	 
	pr_warn("%s: running CQE recovery\n", mmc_hostname(host));

	host->cqe_ops->cqe_recovery_start(host);

	memset(&cmd, 0, sizeof(cmd));
	cmd.opcode       = MMC_STOP_TRANSMISSION;
	cmd.flags        = MMC_RSP_R1B | MMC_CMD_AC;
	cmd.flags       &= ~MMC_RSP_CRC;  
	cmd.busy_timeout = MMC_CQE_RECOVERY_TIMEOUT;
	mmc_wait_for_cmd(host, &cmd, MMC_CMD_RETRIES);

	mmc_poll_for_busy(host->card, MMC_CQE_RECOVERY_TIMEOUT, true, MMC_BUSY_IO);

	memset(&cmd, 0, sizeof(cmd));
	cmd.opcode       = MMC_CMDQ_TASK_MGMT;
	cmd.arg          = 1;  
	cmd.flags        = MMC_RSP_R1B | MMC_CMD_AC;
	cmd.flags       &= ~MMC_RSP_CRC;  
	cmd.busy_timeout = MMC_CQE_RECOVERY_TIMEOUT;
	err = mmc_wait_for_cmd(host, &cmd, MMC_CMD_RETRIES);

	host->cqe_ops->cqe_recovery_finish(host);

	if (err)
		err = mmc_wait_for_cmd(host, &cmd, MMC_CMD_RETRIES);

	mmc_retune_release(host);

	return err;
}
EXPORT_SYMBOL(mmc_cqe_recovery);

 
bool mmc_is_req_done(struct mmc_host *host, struct mmc_request *mrq)
{
	return completion_done(&mrq->completion);
}
EXPORT_SYMBOL(mmc_is_req_done);

 
void mmc_wait_for_req(struct mmc_host *host, struct mmc_request *mrq)
{
	__mmc_start_req(host, mrq);

	if (!mrq->cap_cmd_during_tfr)
		mmc_wait_for_req_done(host, mrq);
}
EXPORT_SYMBOL(mmc_wait_for_req);

 
int mmc_wait_for_cmd(struct mmc_host *host, struct mmc_command *cmd, int retries)
{
	struct mmc_request mrq = {};

	WARN_ON(!host->claimed);

	memset(cmd->resp, 0, sizeof(cmd->resp));
	cmd->retries = retries;

	mrq.cmd = cmd;
	cmd->data = NULL;

	mmc_wait_for_req(host, &mrq);

	return cmd->error;
}

EXPORT_SYMBOL(mmc_wait_for_cmd);

 
void mmc_set_data_timeout(struct mmc_data *data, const struct mmc_card *card)
{
	unsigned int mult;

	 
	if (mmc_card_sdio(card)) {
		data->timeout_ns = 1000000000;
		data->timeout_clks = 0;
		return;
	}

	 
	mult = mmc_card_sd(card) ? 100 : 10;

	 
	if (data->flags & MMC_DATA_WRITE)
		mult <<= card->csd.r2w_factor;

	data->timeout_ns = card->csd.taac_ns * mult;
	data->timeout_clks = card->csd.taac_clks * mult;

	 
	if (mmc_card_sd(card)) {
		unsigned int timeout_us, limit_us;

		timeout_us = data->timeout_ns / 1000;
		if (card->host->ios.clock)
			timeout_us += data->timeout_clks * 1000 /
				(card->host->ios.clock / 1000);

		if (data->flags & MMC_DATA_WRITE)
			 
			limit_us = 3000000;
		else
			limit_us = 100000;

		 
		if (timeout_us > limit_us) {
			data->timeout_ns = limit_us * 1000;
			data->timeout_clks = 0;
		}

		 
		if (timeout_us == 0)
			data->timeout_ns = limit_us * 1000;
	}

	 
	if (mmc_card_long_read_time(card) && data->flags & MMC_DATA_READ) {
		data->timeout_ns = 600000000;
		data->timeout_clks = 0;
	}

	 
	if (mmc_host_is_spi(card->host)) {
		if (data->flags & MMC_DATA_WRITE) {
			if (data->timeout_ns < 1000000000)
				data->timeout_ns = 1000000000;	 
		} else {
			if (data->timeout_ns < 100000000)
				data->timeout_ns =  100000000;	 
		}
	}
}
EXPORT_SYMBOL(mmc_set_data_timeout);

 
static inline bool mmc_ctx_matches(struct mmc_host *host, struct mmc_ctx *ctx,
				   struct task_struct *task)
{
	return host->claimer == ctx ||
	       (!ctx && task && host->claimer->task == task);
}

static inline void mmc_ctx_set_claimer(struct mmc_host *host,
				       struct mmc_ctx *ctx,
				       struct task_struct *task)
{
	if (!host->claimer) {
		if (ctx)
			host->claimer = ctx;
		else
			host->claimer = &host->default_ctx;
	}
	if (task)
		host->claimer->task = task;
}

 
int __mmc_claim_host(struct mmc_host *host, struct mmc_ctx *ctx,
		     atomic_t *abort)
{
	struct task_struct *task = ctx ? NULL : current;
	DECLARE_WAITQUEUE(wait, current);
	unsigned long flags;
	int stop;
	bool pm = false;

	might_sleep();

	add_wait_queue(&host->wq, &wait);
	spin_lock_irqsave(&host->lock, flags);
	while (1) {
		set_current_state(TASK_UNINTERRUPTIBLE);
		stop = abort ? atomic_read(abort) : 0;
		if (stop || !host->claimed || mmc_ctx_matches(host, ctx, task))
			break;
		spin_unlock_irqrestore(&host->lock, flags);
		schedule();
		spin_lock_irqsave(&host->lock, flags);
	}
	set_current_state(TASK_RUNNING);
	if (!stop) {
		host->claimed = 1;
		mmc_ctx_set_claimer(host, ctx, task);
		host->claim_cnt += 1;
		if (host->claim_cnt == 1)
			pm = true;
	} else
		wake_up(&host->wq);
	spin_unlock_irqrestore(&host->lock, flags);
	remove_wait_queue(&host->wq, &wait);

	if (pm)
		pm_runtime_get_sync(mmc_dev(host));

	return stop;
}
EXPORT_SYMBOL(__mmc_claim_host);

 
void mmc_release_host(struct mmc_host *host)
{
	unsigned long flags;

	WARN_ON(!host->claimed);

	spin_lock_irqsave(&host->lock, flags);
	if (--host->claim_cnt) {
		 
		spin_unlock_irqrestore(&host->lock, flags);
	} else {
		host->claimed = 0;
		host->claimer->task = NULL;
		host->claimer = NULL;
		spin_unlock_irqrestore(&host->lock, flags);
		wake_up(&host->wq);
		pm_runtime_mark_last_busy(mmc_dev(host));
		if (host->caps & MMC_CAP_SYNC_RUNTIME_PM)
			pm_runtime_put_sync_suspend(mmc_dev(host));
		else
			pm_runtime_put_autosuspend(mmc_dev(host));
	}
}
EXPORT_SYMBOL(mmc_release_host);

 
void mmc_get_card(struct mmc_card *card, struct mmc_ctx *ctx)
{
	pm_runtime_get_sync(&card->dev);
	__mmc_claim_host(card->host, ctx, NULL);
}
EXPORT_SYMBOL(mmc_get_card);

 
void mmc_put_card(struct mmc_card *card, struct mmc_ctx *ctx)
{
	struct mmc_host *host = card->host;

	WARN_ON(ctx && host->claimer != ctx);

	mmc_release_host(host);
	pm_runtime_mark_last_busy(&card->dev);
	pm_runtime_put_autosuspend(&card->dev);
}
EXPORT_SYMBOL(mmc_put_card);

 
static inline void mmc_set_ios(struct mmc_host *host)
{
	struct mmc_ios *ios = &host->ios;

	pr_debug("%s: clock %uHz busmode %u powermode %u cs %u Vdd %u "
		"width %u timing %u\n",
		 mmc_hostname(host), ios->clock, ios->bus_mode,
		 ios->power_mode, ios->chip_select, ios->vdd,
		 1 << ios->bus_width, ios->timing);

	host->ops->set_ios(host, ios);
}

 
void mmc_set_chip_select(struct mmc_host *host, int mode)
{
	host->ios.chip_select = mode;
	mmc_set_ios(host);
}

 
void mmc_set_clock(struct mmc_host *host, unsigned int hz)
{
	WARN_ON(hz && hz < host->f_min);

	if (hz > host->f_max)
		hz = host->f_max;

	host->ios.clock = hz;
	mmc_set_ios(host);
}

int mmc_execute_tuning(struct mmc_card *card)
{
	struct mmc_host *host = card->host;
	u32 opcode;
	int err;

	if (!host->ops->execute_tuning)
		return 0;

	if (host->cqe_on)
		host->cqe_ops->cqe_off(host);

	if (mmc_card_mmc(card))
		opcode = MMC_SEND_TUNING_BLOCK_HS200;
	else
		opcode = MMC_SEND_TUNING_BLOCK;

	err = host->ops->execute_tuning(host, opcode);
	if (!err) {
		mmc_retune_clear(host);
		mmc_retune_enable(host);
		return 0;
	}

	 
	if (!host->detect_change) {
		pr_err("%s: tuning execution failed: %d\n",
			mmc_hostname(host), err);
		mmc_debugfs_err_stats_inc(host, MMC_ERR_TUNING);
	}

	return err;
}

 
void mmc_set_bus_mode(struct mmc_host *host, unsigned int mode)
{
	host->ios.bus_mode = mode;
	mmc_set_ios(host);
}

 
void mmc_set_bus_width(struct mmc_host *host, unsigned int width)
{
	host->ios.bus_width = width;
	mmc_set_ios(host);
}

 
void mmc_set_initial_state(struct mmc_host *host)
{
	if (host->cqe_on)
		host->cqe_ops->cqe_off(host);

	mmc_retune_disable(host);

	if (mmc_host_is_spi(host))
		host->ios.chip_select = MMC_CS_HIGH;
	else
		host->ios.chip_select = MMC_CS_DONTCARE;
	host->ios.bus_mode = MMC_BUSMODE_PUSHPULL;
	host->ios.bus_width = MMC_BUS_WIDTH_1;
	host->ios.timing = MMC_TIMING_LEGACY;
	host->ios.drv_type = 0;
	host->ios.enhanced_strobe = false;

	 
	if ((host->caps2 & MMC_CAP2_HS400_ES) &&
	     host->ops->hs400_enhanced_strobe)
		host->ops->hs400_enhanced_strobe(host, &host->ios);

	mmc_set_ios(host);

	mmc_crypto_set_initial_state(host);
}

 
static int mmc_vdd_to_ocrbitnum(int vdd, bool low_bits)
{
	const int max_bit = ilog2(MMC_VDD_35_36);
	int bit;

	if (vdd < 1650 || vdd > 3600)
		return -EINVAL;

	if (vdd >= 1650 && vdd <= 1950)
		return ilog2(MMC_VDD_165_195);

	if (low_bits)
		vdd -= 1;

	 
	bit = (vdd - 2000) / 100 + 8;
	if (bit > max_bit)
		return max_bit;
	return bit;
}

 
u32 mmc_vddrange_to_ocrmask(int vdd_min, int vdd_max)
{
	u32 mask = 0;

	if (vdd_max < vdd_min)
		return 0;

	 
	vdd_max = mmc_vdd_to_ocrbitnum(vdd_max, false);
	if (vdd_max < 0)
		return 0;

	 
	vdd_min = mmc_vdd_to_ocrbitnum(vdd_min, true);
	if (vdd_min < 0)
		return 0;

	 
	while (vdd_max >= vdd_min)
		mask |= 1 << vdd_max--;

	return mask;
}

static int mmc_of_get_func_num(struct device_node *node)
{
	u32 reg;
	int ret;

	ret = of_property_read_u32(node, "reg", &reg);
	if (ret < 0)
		return ret;

	return reg;
}

struct device_node *mmc_of_find_child_device(struct mmc_host *host,
		unsigned func_num)
{
	struct device_node *node;

	if (!host->parent || !host->parent->of_node)
		return NULL;

	for_each_child_of_node(host->parent->of_node, node) {
		if (mmc_of_get_func_num(node) == func_num)
			return node;
	}

	return NULL;
}

 
u32 mmc_select_voltage(struct mmc_host *host, u32 ocr)
{
	int bit;

	 
	if (ocr & 0x7F) {
		dev_warn(mmc_dev(host),
		"card claims to support voltages below defined range\n");
		ocr &= ~0x7F;
	}

	ocr &= host->ocr_avail;
	if (!ocr) {
		dev_warn(mmc_dev(host), "no support for card's volts\n");
		return 0;
	}

	if (host->caps2 & MMC_CAP2_FULL_PWR_CYCLE) {
		bit = ffs(ocr) - 1;
		ocr &= 3 << bit;
		mmc_power_cycle(host, ocr);
	} else {
		bit = fls(ocr) - 1;
		 
		ocr &= 3 << (bit - 1);
		if (bit != host->ios.vdd)
			dev_warn(mmc_dev(host), "exceeding card's volts\n");
	}

	return ocr;
}

int mmc_set_signal_voltage(struct mmc_host *host, int signal_voltage)
{
	int err = 0;
	int old_signal_voltage = host->ios.signal_voltage;

	host->ios.signal_voltage = signal_voltage;
	if (host->ops->start_signal_voltage_switch)
		err = host->ops->start_signal_voltage_switch(host, &host->ios);

	if (err)
		host->ios.signal_voltage = old_signal_voltage;

	return err;

}

void mmc_set_initial_signal_voltage(struct mmc_host *host)
{
	 
	if (!mmc_set_signal_voltage(host, MMC_SIGNAL_VOLTAGE_330))
		dev_dbg(mmc_dev(host), "Initial signal voltage of 3.3v\n");
	else if (!mmc_set_signal_voltage(host, MMC_SIGNAL_VOLTAGE_180))
		dev_dbg(mmc_dev(host), "Initial signal voltage of 1.8v\n");
	else if (!mmc_set_signal_voltage(host, MMC_SIGNAL_VOLTAGE_120))
		dev_dbg(mmc_dev(host), "Initial signal voltage of 1.2v\n");
}

int mmc_host_set_uhs_voltage(struct mmc_host *host)
{
	u32 clock;

	 
	clock = host->ios.clock;
	host->ios.clock = 0;
	mmc_set_ios(host);

	if (mmc_set_signal_voltage(host, MMC_SIGNAL_VOLTAGE_180))
		return -EAGAIN;

	 
	mmc_delay(10);
	host->ios.clock = clock;
	mmc_set_ios(host);

	return 0;
}

int mmc_set_uhs_voltage(struct mmc_host *host, u32 ocr)
{
	struct mmc_command cmd = {};
	int err = 0;

	 
	if (!host->ops->start_signal_voltage_switch)
		return -EPERM;
	if (!host->ops->card_busy)
		pr_warn("%s: cannot verify signal voltage switch\n",
			mmc_hostname(host));

	cmd.opcode = SD_SWITCH_VOLTAGE;
	cmd.arg = 0;
	cmd.flags = MMC_RSP_R1 | MMC_CMD_AC;

	err = mmc_wait_for_cmd(host, &cmd, 0);
	if (err)
		goto power_cycle;

	if (!mmc_host_is_spi(host) && (cmd.resp[0] & R1_ERROR))
		return -EIO;

	 
	mmc_delay(1);
	if (host->ops->card_busy && !host->ops->card_busy(host)) {
		err = -EAGAIN;
		goto power_cycle;
	}

	if (mmc_host_set_uhs_voltage(host)) {
		 
		err = -EAGAIN;
		goto power_cycle;
	}

	 
	mmc_delay(1);

	 
	if (host->ops->card_busy && host->ops->card_busy(host))
		err = -EAGAIN;

power_cycle:
	if (err) {
		pr_debug("%s: Signal voltage switch failed, "
			"power cycling card\n", mmc_hostname(host));
		mmc_power_cycle(host, ocr);
	}

	return err;
}

 
void mmc_set_timing(struct mmc_host *host, unsigned int timing)
{
	host->ios.timing = timing;
	mmc_set_ios(host);
}

 
void mmc_set_driver_type(struct mmc_host *host, unsigned int drv_type)
{
	host->ios.drv_type = drv_type;
	mmc_set_ios(host);
}

int mmc_select_drive_strength(struct mmc_card *card, unsigned int max_dtr,
			      int card_drv_type, int *drv_type)
{
	struct mmc_host *host = card->host;
	int host_drv_type = SD_DRIVER_TYPE_B;

	*drv_type = 0;

	if (!host->ops->select_drive_strength)
		return 0;

	 
	if (host->caps & MMC_CAP_DRIVER_TYPE_A)
		host_drv_type |= SD_DRIVER_TYPE_A;

	if (host->caps & MMC_CAP_DRIVER_TYPE_C)
		host_drv_type |= SD_DRIVER_TYPE_C;

	if (host->caps & MMC_CAP_DRIVER_TYPE_D)
		host_drv_type |= SD_DRIVER_TYPE_D;

	 
	return host->ops->select_drive_strength(card, max_dtr,
						host_drv_type,
						card_drv_type,
						drv_type);
}

 
void mmc_power_up(struct mmc_host *host, u32 ocr)
{
	if (host->ios.power_mode == MMC_POWER_ON)
		return;

	mmc_pwrseq_pre_power_on(host);

	host->ios.vdd = fls(ocr) - 1;
	host->ios.power_mode = MMC_POWER_UP;
	 
	mmc_set_initial_state(host);

	mmc_set_initial_signal_voltage(host);

	 
	mmc_delay(host->ios.power_delay_ms);

	mmc_pwrseq_post_power_on(host);

	host->ios.clock = host->f_init;

	host->ios.power_mode = MMC_POWER_ON;
	mmc_set_ios(host);

	 
	mmc_delay(host->ios.power_delay_ms);
}

void mmc_power_off(struct mmc_host *host)
{
	if (host->ios.power_mode == MMC_POWER_OFF)
		return;

	mmc_pwrseq_power_off(host);

	host->ios.clock = 0;
	host->ios.vdd = 0;

	host->ios.power_mode = MMC_POWER_OFF;
	 
	mmc_set_initial_state(host);

	 
	mmc_delay(1);
}

void mmc_power_cycle(struct mmc_host *host, u32 ocr)
{
	mmc_power_off(host);
	 
	mmc_delay(1);
	mmc_power_up(host, ocr);
}

 
void mmc_attach_bus(struct mmc_host *host, const struct mmc_bus_ops *ops)
{
	host->bus_ops = ops;
}

 
void mmc_detach_bus(struct mmc_host *host)
{
	host->bus_ops = NULL;
}

void _mmc_detect_change(struct mmc_host *host, unsigned long delay, bool cd_irq)
{
	 
	if (cd_irq && !(host->caps & MMC_CAP_NEEDS_POLL))
		__pm_wakeup_event(host->ws, 5000);

	host->detect_change = 1;
	mmc_schedule_delayed_work(&host->detect, delay);
}

 
void mmc_detect_change(struct mmc_host *host, unsigned long delay)
{
	_mmc_detect_change(host, delay, true);
}
EXPORT_SYMBOL(mmc_detect_change);

void mmc_init_erase(struct mmc_card *card)
{
	unsigned int sz;

	if (is_power_of_2(card->erase_size))
		card->erase_shift = ffs(card->erase_size) - 1;
	else
		card->erase_shift = 0;

	 
	if (mmc_card_sd(card) && card->ssr.au) {
		card->pref_erase = card->ssr.au;
		card->erase_shift = ffs(card->ssr.au) - 1;
	} else if (card->erase_size) {
		sz = (card->csd.capacity << (card->csd.read_blkbits - 9)) >> 11;
		if (sz < 128)
			card->pref_erase = 512 * 1024 / 512;
		else if (sz < 512)
			card->pref_erase = 1024 * 1024 / 512;
		else if (sz < 1024)
			card->pref_erase = 2 * 1024 * 1024 / 512;
		else
			card->pref_erase = 4 * 1024 * 1024 / 512;
		if (card->pref_erase < card->erase_size)
			card->pref_erase = card->erase_size;
		else {
			sz = card->pref_erase % card->erase_size;
			if (sz)
				card->pref_erase += card->erase_size - sz;
		}
	} else
		card->pref_erase = 0;
}

static bool is_trim_arg(unsigned int arg)
{
	return (arg & MMC_TRIM_OR_DISCARD_ARGS) && arg != MMC_DISCARD_ARG;
}

static unsigned int mmc_mmc_erase_timeout(struct mmc_card *card,
				          unsigned int arg, unsigned int qty)
{
	unsigned int erase_timeout;

	if (arg == MMC_DISCARD_ARG ||
	    (arg == MMC_TRIM_ARG && card->ext_csd.rev >= 6)) {
		erase_timeout = card->ext_csd.trim_timeout;
	} else if (card->ext_csd.erase_group_def & 1) {
		 
		if (arg == MMC_TRIM_ARG)
			erase_timeout = card->ext_csd.trim_timeout;
		else
			erase_timeout = card->ext_csd.hc_erase_timeout;
	} else {
		 
		unsigned int mult = (10 << card->csd.r2w_factor);
		unsigned int timeout_clks = card->csd.taac_clks * mult;
		unsigned int timeout_us;

		 
		if (card->csd.taac_ns < 1000000)
			timeout_us = (card->csd.taac_ns * mult) / 1000;
		else
			timeout_us = (card->csd.taac_ns / 1000) * mult;

		 
		timeout_clks <<= 1;
		timeout_us += (timeout_clks * 1000) /
			      (card->host->ios.clock / 1000);

		erase_timeout = timeout_us / 1000;

		 
		if (!erase_timeout)
			erase_timeout = 1;
	}

	 
	if (arg & MMC_SECURE_ARGS) {
		if (arg == MMC_SECURE_ERASE_ARG)
			erase_timeout *= card->ext_csd.sec_erase_mult;
		else
			erase_timeout *= card->ext_csd.sec_trim_mult;
	}

	erase_timeout *= qty;

	 
	if (mmc_host_is_spi(card->host) && erase_timeout < 1000)
		erase_timeout = 1000;

	return erase_timeout;
}

static unsigned int mmc_sd_erase_timeout(struct mmc_card *card,
					 unsigned int arg,
					 unsigned int qty)
{
	unsigned int erase_timeout;

	 
	if (arg == SD_DISCARD_ARG)
		return SD_DISCARD_TIMEOUT_MS;

	if (card->ssr.erase_timeout) {
		 
		erase_timeout = card->ssr.erase_timeout * qty +
				card->ssr.erase_offset;
	} else {
		 
		erase_timeout = 250 * qty;
	}

	 
	if (erase_timeout < 1000)
		erase_timeout = 1000;

	return erase_timeout;
}

static unsigned int mmc_erase_timeout(struct mmc_card *card,
				      unsigned int arg,
				      unsigned int qty)
{
	if (mmc_card_sd(card))
		return mmc_sd_erase_timeout(card, arg, qty);
	else
		return mmc_mmc_erase_timeout(card, arg, qty);
}

static int mmc_do_erase(struct mmc_card *card, unsigned int from,
			unsigned int to, unsigned int arg)
{
	struct mmc_command cmd = {};
	unsigned int qty = 0, busy_timeout = 0;
	bool use_r1b_resp;
	int err;

	mmc_retune_hold(card->host);

	 
	if (card->erase_shift)
		qty += ((to >> card->erase_shift) -
			(from >> card->erase_shift)) + 1;
	else if (mmc_card_sd(card))
		qty += to - from + 1;
	else
		qty += ((to / card->erase_size) -
			(from / card->erase_size)) + 1;

	if (!mmc_card_blockaddr(card)) {
		from <<= 9;
		to <<= 9;
	}

	if (mmc_card_sd(card))
		cmd.opcode = SD_ERASE_WR_BLK_START;
	else
		cmd.opcode = MMC_ERASE_GROUP_START;
	cmd.arg = from;
	cmd.flags = MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_AC;
	err = mmc_wait_for_cmd(card->host, &cmd, 0);
	if (err) {
		pr_err("mmc_erase: group start error %d, "
		       "status %#x\n", err, cmd.resp[0]);
		err = -EIO;
		goto out;
	}

	memset(&cmd, 0, sizeof(struct mmc_command));
	if (mmc_card_sd(card))
		cmd.opcode = SD_ERASE_WR_BLK_END;
	else
		cmd.opcode = MMC_ERASE_GROUP_END;
	cmd.arg = to;
	cmd.flags = MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_AC;
	err = mmc_wait_for_cmd(card->host, &cmd, 0);
	if (err) {
		pr_err("mmc_erase: group end error %d, status %#x\n",
		       err, cmd.resp[0]);
		err = -EIO;
		goto out;
	}

	memset(&cmd, 0, sizeof(struct mmc_command));
	cmd.opcode = MMC_ERASE;
	cmd.arg = arg;
	busy_timeout = mmc_erase_timeout(card, arg, qty);
	use_r1b_resp = mmc_prepare_busy_cmd(card->host, &cmd, busy_timeout);

	err = mmc_wait_for_cmd(card->host, &cmd, 0);
	if (err) {
		pr_err("mmc_erase: erase error %d, status %#x\n",
		       err, cmd.resp[0]);
		err = -EIO;
		goto out;
	}

	if (mmc_host_is_spi(card->host))
		goto out;

	 
	if ((card->host->caps & MMC_CAP_WAIT_WHILE_BUSY) && use_r1b_resp)
		goto out;

	 
	err = mmc_poll_for_busy(card, busy_timeout, false, MMC_BUSY_ERASE);

out:
	mmc_retune_release(card->host);
	return err;
}

static unsigned int mmc_align_erase_size(struct mmc_card *card,
					 unsigned int *from,
					 unsigned int *to,
					 unsigned int nr)
{
	unsigned int from_new = *from, nr_new = nr, rem;

	 
	if (is_power_of_2(card->erase_size)) {
		unsigned int temp = from_new;

		from_new = round_up(temp, card->erase_size);
		rem = from_new - temp;

		if (nr_new > rem)
			nr_new -= rem;
		else
			return 0;

		nr_new = round_down(nr_new, card->erase_size);
	} else {
		rem = from_new % card->erase_size;
		if (rem) {
			rem = card->erase_size - rem;
			from_new += rem;
			if (nr_new > rem)
				nr_new -= rem;
			else
				return 0;
		}

		rem = nr_new % card->erase_size;
		if (rem)
			nr_new -= rem;
	}

	if (nr_new == 0)
		return 0;

	*to = from_new + nr_new;
	*from = from_new;

	return nr_new;
}

 
int mmc_erase(struct mmc_card *card, unsigned int from, unsigned int nr,
	      unsigned int arg)
{
	unsigned int rem, to = from + nr;
	int err;

	if (!(card->csd.cmdclass & CCC_ERASE))
		return -EOPNOTSUPP;

	if (!card->erase_size)
		return -EOPNOTSUPP;

	if (mmc_card_sd(card) && arg != SD_ERASE_ARG && arg != SD_DISCARD_ARG)
		return -EOPNOTSUPP;

	if (mmc_card_mmc(card) && (arg & MMC_SECURE_ARGS) &&
	    !(card->ext_csd.sec_feature_support & EXT_CSD_SEC_ER_EN))
		return -EOPNOTSUPP;

	if (mmc_card_mmc(card) && is_trim_arg(arg) &&
	    !(card->ext_csd.sec_feature_support & EXT_CSD_SEC_GB_CL_EN))
		return -EOPNOTSUPP;

	if (arg == MMC_SECURE_ERASE_ARG) {
		if (from % card->erase_size || nr % card->erase_size)
			return -EINVAL;
	}

	if (arg == MMC_ERASE_ARG)
		nr = mmc_align_erase_size(card, &from, &to, nr);

	if (nr == 0)
		return 0;

	if (to <= from)
		return -EINVAL;

	 
	to -= 1;

	 
	rem = card->erase_size - (from % card->erase_size);
	if ((arg & MMC_TRIM_OR_DISCARD_ARGS) && card->eg_boundary && nr > rem) {
		err = mmc_do_erase(card, from, from + rem - 1, arg);
		from += rem;
		if ((err) || (to <= from))
			return err;
	}

	return mmc_do_erase(card, from, to, arg);
}
EXPORT_SYMBOL(mmc_erase);

int mmc_can_erase(struct mmc_card *card)
{
	if (card->csd.cmdclass & CCC_ERASE && card->erase_size)
		return 1;
	return 0;
}
EXPORT_SYMBOL(mmc_can_erase);

int mmc_can_trim(struct mmc_card *card)
{
	if ((card->ext_csd.sec_feature_support & EXT_CSD_SEC_GB_CL_EN) &&
	    (!(card->quirks & MMC_QUIRK_TRIM_BROKEN)))
		return 1;
	return 0;
}
EXPORT_SYMBOL(mmc_can_trim);

int mmc_can_discard(struct mmc_card *card)
{
	 
	if (card->ext_csd.feature_support & MMC_DISCARD_FEATURE)
		return 1;
	return 0;
}
EXPORT_SYMBOL(mmc_can_discard);

int mmc_can_sanitize(struct mmc_card *card)
{
	if (!mmc_can_trim(card) && !mmc_can_erase(card))
		return 0;
	if (card->ext_csd.sec_feature_support & EXT_CSD_SEC_SANITIZE)
		return 1;
	return 0;
}

int mmc_can_secure_erase_trim(struct mmc_card *card)
{
	if ((card->ext_csd.sec_feature_support & EXT_CSD_SEC_ER_EN) &&
	    !(card->quirks & MMC_QUIRK_SEC_ERASE_TRIM_BROKEN))
		return 1;
	return 0;
}
EXPORT_SYMBOL(mmc_can_secure_erase_trim);

int mmc_erase_group_aligned(struct mmc_card *card, unsigned int from,
			    unsigned int nr)
{
	if (!card->erase_size)
		return 0;
	if (from % card->erase_size || nr % card->erase_size)
		return 0;
	return 1;
}
EXPORT_SYMBOL(mmc_erase_group_aligned);

static unsigned int mmc_do_calc_max_discard(struct mmc_card *card,
					    unsigned int arg)
{
	struct mmc_host *host = card->host;
	unsigned int max_discard, x, y, qty = 0, max_qty, min_qty, timeout;
	unsigned int last_timeout = 0;
	unsigned int max_busy_timeout = host->max_busy_timeout ?
			host->max_busy_timeout : MMC_ERASE_TIMEOUT_MS;

	if (card->erase_shift) {
		max_qty = UINT_MAX >> card->erase_shift;
		min_qty = card->pref_erase >> card->erase_shift;
	} else if (mmc_card_sd(card)) {
		max_qty = UINT_MAX;
		min_qty = card->pref_erase;
	} else {
		max_qty = UINT_MAX / card->erase_size;
		min_qty = card->pref_erase / card->erase_size;
	}

	 
	do {
		y = 0;
		for (x = 1; x && x <= max_qty && max_qty - x >= qty; x <<= 1) {
			timeout = mmc_erase_timeout(card, arg, qty + x);

			if (qty + x > min_qty && timeout > max_busy_timeout)
				break;

			if (timeout < last_timeout)
				break;
			last_timeout = timeout;
			y = x;
		}
		qty += y;
	} while (y);

	if (!qty)
		return 0;

	 
	if (qty == 1)
		card->eg_boundary = 1;
	else
		qty--;

	 
	if (card->erase_shift)
		max_discard = qty << card->erase_shift;
	else if (mmc_card_sd(card))
		max_discard = qty + 1;
	else
		max_discard = qty * card->erase_size;

	return max_discard;
}

unsigned int mmc_calc_max_discard(struct mmc_card *card)
{
	struct mmc_host *host = card->host;
	unsigned int max_discard, max_trim;

	 
	if (mmc_card_mmc(card) && !(card->ext_csd.erase_group_def & 1))
		return card->pref_erase;

	max_discard = mmc_do_calc_max_discard(card, MMC_ERASE_ARG);
	if (mmc_can_trim(card)) {
		max_trim = mmc_do_calc_max_discard(card, MMC_TRIM_ARG);
		if (max_trim < max_discard || max_discard == 0)
			max_discard = max_trim;
	} else if (max_discard < card->erase_size) {
		max_discard = 0;
	}
	pr_debug("%s: calculated max. discard sectors %u for timeout %u ms\n",
		mmc_hostname(host), max_discard, host->max_busy_timeout ?
		host->max_busy_timeout : MMC_ERASE_TIMEOUT_MS);
	return max_discard;
}
EXPORT_SYMBOL(mmc_calc_max_discard);

bool mmc_card_is_blockaddr(struct mmc_card *card)
{
	return card ? mmc_card_blockaddr(card) : false;
}
EXPORT_SYMBOL(mmc_card_is_blockaddr);

int mmc_set_blocklen(struct mmc_card *card, unsigned int blocklen)
{
	struct mmc_command cmd = {};

	if (mmc_card_blockaddr(card) || mmc_card_ddr52(card) ||
	    mmc_card_hs400(card) || mmc_card_hs400es(card))
		return 0;

	cmd.opcode = MMC_SET_BLOCKLEN;
	cmd.arg = blocklen;
	cmd.flags = MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_AC;
	return mmc_wait_for_cmd(card->host, &cmd, 5);
}
EXPORT_SYMBOL(mmc_set_blocklen);

static void mmc_hw_reset_for_init(struct mmc_host *host)
{
	mmc_pwrseq_reset(host);

	if (!(host->caps & MMC_CAP_HW_RESET) || !host->ops->card_hw_reset)
		return;
	host->ops->card_hw_reset(host);
}

 
int mmc_hw_reset(struct mmc_card *card)
{
	struct mmc_host *host = card->host;
	int ret;

	ret = host->bus_ops->hw_reset(host);
	if (ret < 0)
		pr_warn("%s: tried to HW reset card, got error %d\n",
			mmc_hostname(host), ret);

	return ret;
}
EXPORT_SYMBOL(mmc_hw_reset);

int mmc_sw_reset(struct mmc_card *card)
{
	struct mmc_host *host = card->host;
	int ret;

	if (!host->bus_ops->sw_reset)
		return -EOPNOTSUPP;

	ret = host->bus_ops->sw_reset(host);
	if (ret)
		pr_warn("%s: tried to SW reset card, got error %d\n",
			mmc_hostname(host), ret);

	return ret;
}
EXPORT_SYMBOL(mmc_sw_reset);

static int mmc_rescan_try_freq(struct mmc_host *host, unsigned freq)
{
	host->f_init = freq;

	pr_debug("%s: %s: trying to init card at %u Hz\n",
		mmc_hostname(host), __func__, host->f_init);

	mmc_power_up(host, host->ocr_avail);

	 
	mmc_hw_reset_for_init(host);

	 
	if (!(host->caps2 & MMC_CAP2_NO_SDIO))
		sdio_reset(host);

	mmc_go_idle(host);

	if (!(host->caps2 & MMC_CAP2_NO_SD)) {
		if (mmc_send_if_cond_pcie(host, host->ocr_avail))
			goto out;
		if (mmc_card_sd_express(host))
			return 0;
	}

	 
	if (!(host->caps2 & MMC_CAP2_NO_SDIO))
		if (!mmc_attach_sdio(host))
			return 0;

	if (!(host->caps2 & MMC_CAP2_NO_SD))
		if (!mmc_attach_sd(host))
			return 0;

	if (!(host->caps2 & MMC_CAP2_NO_MMC))
		if (!mmc_attach_mmc(host))
			return 0;

out:
	mmc_power_off(host);
	return -EIO;
}

int _mmc_detect_card_removed(struct mmc_host *host)
{
	int ret;

	if (!host->card || mmc_card_removed(host->card))
		return 1;

	ret = host->bus_ops->alive(host);

	 
	if (!ret && host->ops->get_cd && !host->ops->get_cd(host)) {
		mmc_detect_change(host, msecs_to_jiffies(200));
		pr_debug("%s: card removed too slowly\n", mmc_hostname(host));
	}

	if (ret) {
		mmc_card_set_removed(host->card);
		pr_debug("%s: card remove detected\n", mmc_hostname(host));
	}

	return ret;
}

int mmc_detect_card_removed(struct mmc_host *host)
{
	struct mmc_card *card = host->card;
	int ret;

	WARN_ON(!host->claimed);

	if (!card)
		return 1;

	if (!mmc_card_is_removable(host))
		return 0;

	ret = mmc_card_removed(card);
	 
	if (!host->detect_change && !(host->caps & MMC_CAP_NEEDS_POLL))
		return ret;

	host->detect_change = 0;
	if (!ret) {
		ret = _mmc_detect_card_removed(host);
		if (ret && (host->caps & MMC_CAP_NEEDS_POLL)) {
			 
			cancel_delayed_work(&host->detect);
			_mmc_detect_change(host, 0, false);
		}
	}

	return ret;
}
EXPORT_SYMBOL(mmc_detect_card_removed);

int mmc_card_alternative_gpt_sector(struct mmc_card *card, sector_t *gpt_sector)
{
	unsigned int boot_sectors_num;

	if ((!(card->host->caps2 & MMC_CAP2_ALT_GPT_TEGRA)))
		return -EOPNOTSUPP;

	 
	if (card->ext_csd.rev < 3 ||
	    !mmc_card_mmc(card) ||
	    !mmc_card_is_blockaddr(card) ||
	     mmc_card_is_removable(card->host))
		return -ENOENT;

	 
	boot_sectors_num = card->ext_csd.raw_boot_mult * SZ_128K /
			   SZ_512 * MMC_NUM_BOOT_PARTITION;

	 
	*gpt_sector = card->ext_csd.sectors - boot_sectors_num - 1;

	return 0;
}
EXPORT_SYMBOL(mmc_card_alternative_gpt_sector);

void mmc_rescan(struct work_struct *work)
{
	struct mmc_host *host =
		container_of(work, struct mmc_host, detect.work);
	int i;

	if (host->rescan_disable)
		return;

	 
	if (!mmc_card_is_removable(host) && host->rescan_entered)
		return;
	host->rescan_entered = 1;

	if (host->trigger_card_event && host->ops->card_event) {
		mmc_claim_host(host);
		host->ops->card_event(host);
		mmc_release_host(host);
		host->trigger_card_event = false;
	}

	 
	if (host->bus_ops)
		host->bus_ops->detect(host);

	host->detect_change = 0;

	 
	if (host->bus_ops != NULL)
		goto out;

	mmc_claim_host(host);
	if (mmc_card_is_removable(host) && host->ops->get_cd &&
			host->ops->get_cd(host) == 0) {
		mmc_power_off(host);
		mmc_release_host(host);
		goto out;
	}

	 
	if (mmc_card_sd_express(host)) {
		mmc_release_host(host);
		goto out;
	}

	for (i = 0; i < ARRAY_SIZE(freqs); i++) {
		unsigned int freq = freqs[i];
		if (freq > host->f_max) {
			if (i + 1 < ARRAY_SIZE(freqs))
				continue;
			freq = host->f_max;
		}
		if (!mmc_rescan_try_freq(host, max(freq, host->f_min)))
			break;
		if (freqs[i] <= host->f_min)
			break;
	}

	 
	if (!mmc_card_is_removable(host) && !host->bus_ops)
		pr_info("%s: Failed to initialize a non-removable card",
			mmc_hostname(host));

	 
	host->err_stats[MMC_ERR_CMD_TIMEOUT] = 0;
	mmc_release_host(host);

 out:
	if (host->caps & MMC_CAP_NEEDS_POLL)
		mmc_schedule_delayed_work(&host->detect, HZ);
}

void mmc_start_host(struct mmc_host *host)
{
	host->f_init = max(min(freqs[0], host->f_max), host->f_min);
	host->rescan_disable = 0;

	if (!(host->caps2 & MMC_CAP2_NO_PRESCAN_POWERUP)) {
		mmc_claim_host(host);
		mmc_power_up(host, host->ocr_avail);
		mmc_release_host(host);
	}

	mmc_gpiod_request_cd_irq(host);
	_mmc_detect_change(host, 0, false);
}

void __mmc_stop_host(struct mmc_host *host)
{
	if (host->slot.cd_irq >= 0) {
		mmc_gpio_set_cd_wake(host, false);
		disable_irq(host->slot.cd_irq);
	}

	host->rescan_disable = 1;
	cancel_delayed_work_sync(&host->detect);
}

void mmc_stop_host(struct mmc_host *host)
{
	__mmc_stop_host(host);

	 
	host->pm_flags = 0;

	if (host->bus_ops) {
		 
		host->bus_ops->remove(host);
		mmc_claim_host(host);
		mmc_detach_bus(host);
		mmc_power_off(host);
		mmc_release_host(host);
		return;
	}

	mmc_claim_host(host);
	mmc_power_off(host);
	mmc_release_host(host);
}

static int __init mmc_init(void)
{
	int ret;

	ret = mmc_register_bus();
	if (ret)
		return ret;

	ret = mmc_register_host_class();
	if (ret)
		goto unregister_bus;

	ret = sdio_register_bus();
	if (ret)
		goto unregister_host_class;

	return 0;

unregister_host_class:
	mmc_unregister_host_class();
unregister_bus:
	mmc_unregister_bus();
	return ret;
}

static void __exit mmc_exit(void)
{
	sdio_unregister_bus();
	mmc_unregister_host_class();
	mmc_unregister_bus();
}

subsys_initcall(mmc_init);
module_exit(mmc_exit);

MODULE_LICENSE("GPL");
