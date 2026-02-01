
 

#include <linux/amba/bus.h>
#include <linux/interrupt.h>
#include <linux/mailbox_controller.h>
#include <linux/mailbox/arm_mhuv2_message.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/spinlock.h>

 

 
#define MHUV2_CH_WN_MAX			124
 
#define MHUV2_CMB_INT_ST_REG_CNT	4
#define MHUV2_STAT_BYTES		(sizeof(u32))
#define MHUV2_STAT_BITS			(MHUV2_STAT_BYTES * __CHAR_BIT__)

#define LSB_MASK(n)			((1 << (n * __CHAR_BIT__)) - 1)
#define MHUV2_PROTOCOL_PROP		"arm,mhuv2-protocols"

 
struct mhu_cfg_t {
	u32 num_ch : 7;
	u32 pad : 25;
} __packed;

 
struct int_st_t {
	u32 nr2r : 1;
	u32 r2nr : 1;
	u32 pad : 30;
} __packed;

 
struct int_clr_t {
	u32 nr2r : 1;
	u32 r2nr : 1;
	u32 pad : 30;
} __packed;

 
struct int_en_t {
	u32 r2nr : 1;
	u32 nr2r : 1;
	u32 chcomb : 1;
	u32 pad : 29;
} __packed;

 
struct iidr_t {
	u32 implementer : 12;
	u32 revision : 4;
	u32 variant : 4;
	u32 product_id : 12;
} __packed;

 
struct aidr_t {
	u32 arch_minor_rev : 4;
	u32 arch_major_rev : 4;
	u32 pad : 24;
} __packed;

 
struct mhu2_send_ch_wn_reg {
	u32 stat;
	u8 pad1[0x0C - 0x04];
	u32 stat_set;
	u32 int_st;
	u32 int_clr;
	u32 int_en;
	u8 pad2[0x20 - 0x1C];
} __packed;

 
struct mhu2_send_frame_reg {
	struct mhu2_send_ch_wn_reg ch_wn[MHUV2_CH_WN_MAX];
	struct mhu_cfg_t mhu_cfg;
	u32 resp_cfg;
	u32 access_request;
	u32 access_ready;
	struct int_st_t int_st;
	struct int_clr_t int_clr;
	struct int_en_t int_en;
	u32 reserved0;
	u32 chcomb_int_st[MHUV2_CMB_INT_ST_REG_CNT];
	u8 pad[0xFC8 - 0xFB0];
	struct iidr_t iidr;
	struct aidr_t aidr;
} __packed;

 
struct mhu2_recv_ch_wn_reg {
	u32 stat;
	u32 stat_masked;
	u32 stat_clear;
	u8 reserved0[0x10 - 0x0C];
	u32 mask;
	u32 mask_set;
	u32 mask_clear;
	u8 pad[0x20 - 0x1C];
} __packed;

 
struct mhu2_recv_frame_reg {
	struct mhu2_recv_ch_wn_reg ch_wn[MHUV2_CH_WN_MAX];
	struct mhu_cfg_t mhu_cfg;
	u8 reserved0[0xF90 - 0xF84];
	struct int_st_t int_st;
	struct int_clr_t int_clr;
	struct int_en_t int_en;
	u32 pad;
	u32 chcomb_int_st[MHUV2_CMB_INT_ST_REG_CNT];
	u8 reserved2[0xFC8 - 0xFB0];
	struct iidr_t iidr;
	struct aidr_t aidr;
} __packed;


 

enum mhuv2_transport_protocol {
	DOORBELL = 0,
	DATA_TRANSFER = 1
};

enum mhuv2_frame {
	RECEIVER_FRAME,
	SENDER_FRAME
};

 
struct mhuv2 {
	struct mbox_controller mbox;
	union {
		struct mhu2_send_frame_reg __iomem *send;
		struct mhu2_recv_frame_reg __iomem *recv;
	};
	enum mhuv2_frame frame;
	unsigned int irq;
	unsigned int windows;
	unsigned int minor;
	unsigned int length;
	u32 *protocols;

	spinlock_t doorbell_pending_lock;
};

#define mhu_from_mbox(_mbox) container_of(_mbox, struct mhuv2, mbox)

 
struct mhuv2_protocol_ops {
	int (*rx_startup)(struct mhuv2 *mhu, struct mbox_chan *chan);
	void (*rx_shutdown)(struct mhuv2 *mhu, struct mbox_chan *chan);
	void *(*read_data)(struct mhuv2 *mhu, struct mbox_chan *chan);

	void (*tx_startup)(struct mhuv2 *mhu, struct mbox_chan *chan);
	void (*tx_shutdown)(struct mhuv2 *mhu, struct mbox_chan *chan);
	int (*last_tx_done)(struct mhuv2 *mhu, struct mbox_chan *chan);
	int (*send_data)(struct mhuv2 *mhu, struct mbox_chan *chan, void *arg);
};

 
struct mhuv2_mbox_chan_priv {
	const struct mhuv2_protocol_ops *ops;
	u32 ch_wn_idx;
	union {
		u32 windows;
		struct {
			u32 doorbell;
			u32 pending;
		};
	};
};

 
#define readl_relaxed_bitfield(_regptr, _type, _field)			\
	({								\
		u32 _regval;						\
		_regval = readl_relaxed((_regptr));			\
		(*(_type *)(&_regval))._field;				\
	})

 
#define writel_relaxed_bitfield(_value, _regptr, _type, _field)		\
	({								\
		u32 _regval;						\
		_regval = readl_relaxed(_regptr);			\
		(*(_type *)(&_regval))._field = _value;			\
		writel_relaxed(_regval, _regptr);			\
	})


 

static int mhuv2_doorbell_rx_startup(struct mhuv2 *mhu, struct mbox_chan *chan)
{
	struct mhuv2_mbox_chan_priv *priv = chan->con_priv;

	writel_relaxed(BIT(priv->doorbell),
		       &mhu->recv->ch_wn[priv->ch_wn_idx].mask_clear);
	return 0;
}

static void mhuv2_doorbell_rx_shutdown(struct mhuv2 *mhu,
				       struct mbox_chan *chan)
{
	struct mhuv2_mbox_chan_priv *priv = chan->con_priv;

	writel_relaxed(BIT(priv->doorbell),
		       &mhu->recv->ch_wn[priv->ch_wn_idx].mask_set);
}

static void *mhuv2_doorbell_read_data(struct mhuv2 *mhu, struct mbox_chan *chan)
{
	struct mhuv2_mbox_chan_priv *priv = chan->con_priv;

	writel_relaxed(BIT(priv->doorbell),
		       &mhu->recv->ch_wn[priv->ch_wn_idx].stat_clear);
	return NULL;
}

static int mhuv2_doorbell_last_tx_done(struct mhuv2 *mhu,
				       struct mbox_chan *chan)
{
	struct mhuv2_mbox_chan_priv *priv = chan->con_priv;

	return !(readl_relaxed(&mhu->send->ch_wn[priv->ch_wn_idx].stat) &
		 BIT(priv->doorbell));
}

static int mhuv2_doorbell_send_data(struct mhuv2 *mhu, struct mbox_chan *chan,
				    void *arg)
{
	struct mhuv2_mbox_chan_priv *priv = chan->con_priv;
	unsigned long flags;

	spin_lock_irqsave(&mhu->doorbell_pending_lock, flags);

	priv->pending = 1;
	writel_relaxed(BIT(priv->doorbell),
		       &mhu->send->ch_wn[priv->ch_wn_idx].stat_set);

	spin_unlock_irqrestore(&mhu->doorbell_pending_lock, flags);

	return 0;
}

static const struct mhuv2_protocol_ops mhuv2_doorbell_ops = {
	.rx_startup = mhuv2_doorbell_rx_startup,
	.rx_shutdown = mhuv2_doorbell_rx_shutdown,
	.read_data = mhuv2_doorbell_read_data,
	.last_tx_done = mhuv2_doorbell_last_tx_done,
	.send_data = mhuv2_doorbell_send_data,
};
#define IS_PROTOCOL_DOORBELL(_priv) (_priv->ops == &mhuv2_doorbell_ops)

 

static int mhuv2_data_transfer_rx_startup(struct mhuv2 *mhu,
					  struct mbox_chan *chan)
{
	struct mhuv2_mbox_chan_priv *priv = chan->con_priv;
	int i = priv->ch_wn_idx + priv->windows - 1;

	 
	writel_relaxed(0xFFFFFFFF, &mhu->recv->ch_wn[i].mask_clear);
	return 0;
}

static void mhuv2_data_transfer_rx_shutdown(struct mhuv2 *mhu,
					    struct mbox_chan *chan)
{
	struct mhuv2_mbox_chan_priv *priv = chan->con_priv;
	int i = priv->ch_wn_idx + priv->windows - 1;

	writel_relaxed(0xFFFFFFFF, &mhu->recv->ch_wn[i].mask_set);
}

static void *mhuv2_data_transfer_read_data(struct mhuv2 *mhu,
					   struct mbox_chan *chan)
{
	struct mhuv2_mbox_chan_priv *priv = chan->con_priv;
	const int windows = priv->windows;
	struct arm_mhuv2_mbox_msg *msg;
	u32 *data;
	int i, idx;

	msg = kzalloc(sizeof(*msg) + windows * MHUV2_STAT_BYTES, GFP_KERNEL);
	if (!msg)
		return ERR_PTR(-ENOMEM);

	data = msg->data = msg + 1;
	msg->len = windows * MHUV2_STAT_BYTES;

	 
	for (i = 0; i < windows; i++) {
		idx = priv->ch_wn_idx + i;
		data[windows - 1 - i] = readl_relaxed(&mhu->recv->ch_wn[idx].stat);
		writel_relaxed(0xFFFFFFFF, &mhu->recv->ch_wn[idx].stat_clear);
	}

	return msg;
}

static void mhuv2_data_transfer_tx_startup(struct mhuv2 *mhu,
					   struct mbox_chan *chan)
{
	struct mhuv2_mbox_chan_priv *priv = chan->con_priv;
	int i = priv->ch_wn_idx + priv->windows - 1;

	 
	if (mhu->minor) {
		writel_relaxed(0x1, &mhu->send->ch_wn[i].int_clr);
		writel_relaxed(0x1, &mhu->send->ch_wn[i].int_en);
	}
}

static void mhuv2_data_transfer_tx_shutdown(struct mhuv2 *mhu,
					    struct mbox_chan *chan)
{
	struct mhuv2_mbox_chan_priv *priv = chan->con_priv;
	int i = priv->ch_wn_idx + priv->windows - 1;

	if (mhu->minor)
		writel_relaxed(0x0, &mhu->send->ch_wn[i].int_en);
}

static int mhuv2_data_transfer_last_tx_done(struct mhuv2 *mhu,
					    struct mbox_chan *chan)
{
	struct mhuv2_mbox_chan_priv *priv = chan->con_priv;
	int i = priv->ch_wn_idx + priv->windows - 1;

	 
	return !readl_relaxed(&mhu->send->ch_wn[i].stat);
}

 
static int mhuv2_data_transfer_send_data(struct mhuv2 *mhu,
					 struct mbox_chan *chan, void *arg)
{
	const struct arm_mhuv2_mbox_msg *msg = arg;
	int bytes_left = msg->len, bytes_to_send, bytes_in_round, i;
	struct mhuv2_mbox_chan_priv *priv = chan->con_priv;
	int windows = priv->windows;
	u32 *data = msg->data, word;

	while (bytes_left) {
		if (!data[0]) {
			dev_err(mhu->mbox.dev, "Data aligned at first window can't be zero to guarantee interrupt generation at receiver");
			return -EINVAL;
		}

		while(!mhuv2_data_transfer_last_tx_done(mhu, chan))
			continue;

		bytes_in_round = min(bytes_left, (int)(windows * MHUV2_STAT_BYTES));

		for (i = windows - 1; i >= 0; i--) {
			 
			if (unlikely(bytes_in_round <= i * MHUV2_STAT_BYTES))
				continue;

			word = data[i];
			bytes_to_send = bytes_in_round & (MHUV2_STAT_BYTES - 1);
			if (unlikely(bytes_to_send))
				word &= LSB_MASK(bytes_to_send);
			else
				bytes_to_send = MHUV2_STAT_BYTES;

			writel_relaxed(word, &mhu->send->ch_wn[priv->ch_wn_idx + windows - 1 - i].stat_set);
			bytes_left -= bytes_to_send;
			bytes_in_round -= bytes_to_send;
		}

		data += windows;
	}

	return 0;
}

static const struct mhuv2_protocol_ops mhuv2_data_transfer_ops = {
	.rx_startup = mhuv2_data_transfer_rx_startup,
	.rx_shutdown = mhuv2_data_transfer_rx_shutdown,
	.read_data = mhuv2_data_transfer_read_data,
	.tx_startup = mhuv2_data_transfer_tx_startup,
	.tx_shutdown = mhuv2_data_transfer_tx_shutdown,
	.last_tx_done = mhuv2_data_transfer_last_tx_done,
	.send_data = mhuv2_data_transfer_send_data,
};

 

static struct mbox_chan *get_irq_chan_comb(struct mhuv2 *mhu, u32 __iomem *reg)
{
	struct mbox_chan *chans = mhu->mbox.chans;
	int channel = 0, i, offset = 0, windows, protocol, ch_wn;
	u32 stat;

	for (i = 0; i < MHUV2_CMB_INT_ST_REG_CNT; i++) {
		stat = readl_relaxed(reg + i);
		if (!stat)
			continue;

		ch_wn = i * MHUV2_STAT_BITS + __builtin_ctz(stat);

		for (i = 0; i < mhu->length; i += 2) {
			protocol = mhu->protocols[i];
			windows = mhu->protocols[i + 1];

			if (ch_wn >= offset + windows) {
				if (protocol == DOORBELL)
					channel += MHUV2_STAT_BITS * windows;
				else
					channel++;

				offset += windows;
				continue;
			}

			 
			if (protocol == DOORBELL)
				channel += MHUV2_STAT_BITS * (ch_wn - offset);

			return &chans[channel];
		}
	}

	return ERR_PTR(-EIO);
}

static irqreturn_t mhuv2_sender_interrupt(int irq, void *data)
{
	struct mhuv2 *mhu = data;
	struct device *dev = mhu->mbox.dev;
	struct mhuv2_mbox_chan_priv *priv;
	struct mbox_chan *chan;
	unsigned long flags;
	int i, found = 0;
	u32 stat;

	chan = get_irq_chan_comb(mhu, mhu->send->chcomb_int_st);
	if (IS_ERR(chan)) {
		dev_warn(dev, "Failed to find channel for the Tx interrupt\n");
		return IRQ_NONE;
	}
	priv = chan->con_priv;

	if (!IS_PROTOCOL_DOORBELL(priv)) {
		writel_relaxed(1, &mhu->send->ch_wn[priv->ch_wn_idx + priv->windows - 1].int_clr);

		if (chan->cl) {
			mbox_chan_txdone(chan, 0);
			return IRQ_HANDLED;
		}

		dev_warn(dev, "Tx interrupt Received on channel (%u) not currently attached to a mailbox client\n",
			 priv->ch_wn_idx);
		return IRQ_NONE;
	}

	 
	writel_relaxed(1, &mhu->send->ch_wn[priv->ch_wn_idx].int_clr);

	 
	spin_lock_irqsave(&mhu->doorbell_pending_lock, flags);

	 
	stat = readl_relaxed(&mhu->send->ch_wn[priv->ch_wn_idx].stat);

	for (i = 0; i < MHUV2_STAT_BITS; i++) {
		priv = chan[i].con_priv;

		 
		if (priv->pending ^ ((stat >> i) & 0x1)) {
			BUG_ON(!priv->pending);

			if (!chan->cl) {
				dev_warn(dev, "Tx interrupt received on doorbell (%u : %u) channel not currently attached to a mailbox client\n",
					 priv->ch_wn_idx, i);
				continue;
			}

			mbox_chan_txdone(&chan[i], 0);
			priv->pending = 0;
			found++;
		}
	}

	spin_unlock_irqrestore(&mhu->doorbell_pending_lock, flags);

	if (!found) {
		 
		dev_dbg(dev, "Couldn't find the doorbell (%u) for the Tx interrupt interrupt\n",
			priv->ch_wn_idx);
		return IRQ_NONE;
	}

	return IRQ_HANDLED;
}

static struct mbox_chan *get_irq_chan_comb_rx(struct mhuv2 *mhu)
{
	struct mhuv2_mbox_chan_priv *priv;
	struct mbox_chan *chan;
	u32 stat;

	chan = get_irq_chan_comb(mhu, mhu->recv->chcomb_int_st);
	if (IS_ERR(chan))
		return chan;

	priv = chan->con_priv;
	if (!IS_PROTOCOL_DOORBELL(priv))
		return chan;

	 
	stat = readl_relaxed(&mhu->recv->ch_wn[priv->ch_wn_idx].stat_masked);
	BUG_ON(!stat);

	return chan + __builtin_ctz(stat);
}

static struct mbox_chan *get_irq_chan_stat_rx(struct mhuv2 *mhu)
{
	struct mbox_chan *chans = mhu->mbox.chans;
	struct mhuv2_mbox_chan_priv *priv;
	u32 stat;
	int i = 0;

	while (i < mhu->mbox.num_chans) {
		priv = chans[i].con_priv;
		stat = readl_relaxed(&mhu->recv->ch_wn[priv->ch_wn_idx].stat_masked);

		if (stat) {
			if (IS_PROTOCOL_DOORBELL(priv))
				i += __builtin_ctz(stat);
			return &chans[i];
		}

		i += IS_PROTOCOL_DOORBELL(priv) ? MHUV2_STAT_BITS : 1;
	}

	return ERR_PTR(-EIO);
}

static struct mbox_chan *get_irq_chan_rx(struct mhuv2 *mhu)
{
	if (!mhu->minor)
		return get_irq_chan_stat_rx(mhu);

	return get_irq_chan_comb_rx(mhu);
}

static irqreturn_t mhuv2_receiver_interrupt(int irq, void *arg)
{
	struct mhuv2 *mhu = arg;
	struct mbox_chan *chan = get_irq_chan_rx(mhu);
	struct device *dev = mhu->mbox.dev;
	struct mhuv2_mbox_chan_priv *priv;
	int ret = IRQ_NONE;
	void *data;

	if (IS_ERR(chan)) {
		dev_warn(dev, "Failed to find channel for the rx interrupt\n");
		return IRQ_NONE;
	}
	priv = chan->con_priv;

	 
	data = priv->ops->read_data(mhu, chan);

	if (!chan->cl) {
		dev_warn(dev, "Received data on channel (%u) not currently attached to a mailbox client\n",
			 priv->ch_wn_idx);
	} else if (IS_ERR(data)) {
		dev_err(dev, "Failed to read data: %lu\n", PTR_ERR(data));
	} else {
		mbox_chan_received_data(chan, data);
		ret = IRQ_HANDLED;
	}

	if (!IS_ERR(data))
		kfree(data);

	return ret;
}

 
static bool mhuv2_sender_last_tx_done(struct mbox_chan *chan)
{
	struct mhuv2 *mhu = mhu_from_mbox(chan->mbox);
	struct mhuv2_mbox_chan_priv *priv = chan->con_priv;

	return priv->ops->last_tx_done(mhu, chan);
}

static int mhuv2_sender_send_data(struct mbox_chan *chan, void *data)
{
	struct mhuv2 *mhu = mhu_from_mbox(chan->mbox);
	struct mhuv2_mbox_chan_priv *priv = chan->con_priv;

	if (!priv->ops->last_tx_done(mhu, chan))
		return -EBUSY;

	return priv->ops->send_data(mhu, chan, data);
}

static int mhuv2_sender_startup(struct mbox_chan *chan)
{
	struct mhuv2 *mhu = mhu_from_mbox(chan->mbox);
	struct mhuv2_mbox_chan_priv *priv = chan->con_priv;

	if (priv->ops->tx_startup)
		priv->ops->tx_startup(mhu, chan);
	return 0;
}

static void mhuv2_sender_shutdown(struct mbox_chan *chan)
{
	struct mhuv2 *mhu = mhu_from_mbox(chan->mbox);
	struct mhuv2_mbox_chan_priv *priv = chan->con_priv;

	if (priv->ops->tx_shutdown)
		priv->ops->tx_shutdown(mhu, chan);
}

static const struct mbox_chan_ops mhuv2_sender_ops = {
	.send_data = mhuv2_sender_send_data,
	.startup = mhuv2_sender_startup,
	.shutdown = mhuv2_sender_shutdown,
	.last_tx_done = mhuv2_sender_last_tx_done,
};

static int mhuv2_receiver_startup(struct mbox_chan *chan)
{
	struct mhuv2 *mhu = mhu_from_mbox(chan->mbox);
	struct mhuv2_mbox_chan_priv *priv = chan->con_priv;

	return priv->ops->rx_startup(mhu, chan);
}

static void mhuv2_receiver_shutdown(struct mbox_chan *chan)
{
	struct mhuv2 *mhu = mhu_from_mbox(chan->mbox);
	struct mhuv2_mbox_chan_priv *priv = chan->con_priv;

	priv->ops->rx_shutdown(mhu, chan);
}

static int mhuv2_receiver_send_data(struct mbox_chan *chan, void *data)
{
	dev_err(chan->mbox->dev,
		"Trying to transmit on a receiver MHU frame\n");
	return -EIO;
}

static bool mhuv2_receiver_last_tx_done(struct mbox_chan *chan)
{
	dev_err(chan->mbox->dev, "Trying to Tx poll on a receiver MHU frame\n");
	return true;
}

static const struct mbox_chan_ops mhuv2_receiver_ops = {
	.send_data = mhuv2_receiver_send_data,
	.startup = mhuv2_receiver_startup,
	.shutdown = mhuv2_receiver_shutdown,
	.last_tx_done = mhuv2_receiver_last_tx_done,
};

static struct mbox_chan *mhuv2_mbox_of_xlate(struct mbox_controller *mbox,
					     const struct of_phandle_args *pa)
{
	struct mhuv2 *mhu = mhu_from_mbox(mbox);
	struct mbox_chan *chans = mbox->chans;
	int channel = 0, i, offset, doorbell, protocol, windows;

	if (pa->args_count != 2)
		return ERR_PTR(-EINVAL);

	offset = pa->args[0];
	doorbell = pa->args[1];
	if (doorbell >= MHUV2_STAT_BITS)
		goto out;

	for (i = 0; i < mhu->length; i += 2) {
		protocol = mhu->protocols[i];
		windows = mhu->protocols[i + 1];

		if (protocol == DOORBELL) {
			if (offset < windows)
				return &chans[channel + MHUV2_STAT_BITS * offset + doorbell];

			channel += MHUV2_STAT_BITS * windows;
			offset -= windows;
		} else {
			if (offset == 0) {
				if (doorbell)
					goto out;

				return &chans[channel];
			}

			channel++;
			offset--;
		}
	}

out:
	dev_err(mbox->dev, "Couldn't xlate to a valid channel (%d: %d)\n",
		pa->args[0], doorbell);
	return ERR_PTR(-ENODEV);
}

static int mhuv2_verify_protocol(struct mhuv2 *mhu)
{
	struct device *dev = mhu->mbox.dev;
	int protocol, windows, channels = 0, total_windows = 0, i;

	for (i = 0; i < mhu->length; i += 2) {
		protocol = mhu->protocols[i];
		windows = mhu->protocols[i + 1];

		if (!windows) {
			dev_err(dev, "Window size can't be zero (%d)\n", i);
			return -EINVAL;
		}
		total_windows += windows;

		if (protocol == DOORBELL) {
			channels += MHUV2_STAT_BITS * windows;
		} else if (protocol == DATA_TRANSFER) {
			channels++;
		} else {
			dev_err(dev, "Invalid protocol (%d) present in %s property at index %d\n",
				protocol, MHUV2_PROTOCOL_PROP, i);
			return -EINVAL;
		}
	}

	if (total_windows > mhu->windows) {
		dev_err(dev, "Channel windows can't be more than what's implemented by the hardware ( %d: %d)\n",
			total_windows, mhu->windows);
		return -EINVAL;
	}

	mhu->mbox.num_chans = channels;
	return 0;
}

static int mhuv2_allocate_channels(struct mhuv2 *mhu)
{
	struct mbox_controller *mbox = &mhu->mbox;
	struct mhuv2_mbox_chan_priv *priv;
	struct device *dev = mbox->dev;
	struct mbox_chan *chans;
	int protocol, windows = 0, next_window = 0, i, j, k;

	chans = devm_kcalloc(dev, mbox->num_chans, sizeof(*chans), GFP_KERNEL);
	if (!chans)
		return -ENOMEM;

	mbox->chans = chans;

	for (i = 0; i < mhu->length; i += 2) {
		next_window += windows;

		protocol = mhu->protocols[i];
		windows = mhu->protocols[i + 1];

		if (protocol == DATA_TRANSFER) {
			priv = devm_kmalloc(dev, sizeof(*priv), GFP_KERNEL);
			if (!priv)
				return -ENOMEM;

			priv->ch_wn_idx = next_window;
			priv->ops = &mhuv2_data_transfer_ops;
			priv->windows = windows;
			chans++->con_priv = priv;
			continue;
		}

		for (j = 0; j < windows; j++) {
			for (k = 0; k < MHUV2_STAT_BITS; k++) {
				priv = devm_kmalloc(dev, sizeof(*priv), GFP_KERNEL);
				if (!priv)
					return -ENOMEM;

				priv->ch_wn_idx = next_window + j;
				priv->ops = &mhuv2_doorbell_ops;
				priv->doorbell = k;
				chans++->con_priv = priv;
			}

			 
			if (mhu->frame == SENDER_FRAME && mhu->minor)
				writel_relaxed(0x1, &mhu->send->ch_wn[priv->ch_wn_idx].int_en);
		}
	}

	 
	BUG_ON(chans - mbox->chans != mbox->num_chans);

	return 0;
}

static int mhuv2_parse_channels(struct mhuv2 *mhu)
{
	struct device *dev = mhu->mbox.dev;
	const struct device_node *np = dev->of_node;
	int ret, count;
	u32 *protocols;

	count = of_property_count_u32_elems(np, MHUV2_PROTOCOL_PROP);
	if (count <= 0 || count % 2) {
		dev_err(dev, "Invalid %s property (%d)\n", MHUV2_PROTOCOL_PROP,
			count);
		return -EINVAL;
	}

	protocols = devm_kmalloc_array(dev, count, sizeof(*protocols), GFP_KERNEL);
	if (!protocols)
		return -ENOMEM;

	ret = of_property_read_u32_array(np, MHUV2_PROTOCOL_PROP, protocols, count);
	if (ret) {
		dev_err(dev, "Failed to read %s property: %d\n",
			MHUV2_PROTOCOL_PROP, ret);
		return ret;
	}

	mhu->protocols = protocols;
	mhu->length = count;

	ret = mhuv2_verify_protocol(mhu);
	if (ret)
		return ret;

	return mhuv2_allocate_channels(mhu);
}

static int mhuv2_tx_init(struct amba_device *adev, struct mhuv2 *mhu,
			 void __iomem *reg)
{
	struct device *dev = mhu->mbox.dev;
	int ret, i;

	mhu->frame = SENDER_FRAME;
	mhu->mbox.ops = &mhuv2_sender_ops;
	mhu->send = reg;

	mhu->windows = readl_relaxed_bitfield(&mhu->send->mhu_cfg, struct mhu_cfg_t, num_ch);
	mhu->minor = readl_relaxed_bitfield(&mhu->send->aidr, struct aidr_t, arch_minor_rev);

	spin_lock_init(&mhu->doorbell_pending_lock);

	 
	if (mhu->minor && adev->irq[0]) {
		ret = devm_request_threaded_irq(dev, adev->irq[0], NULL,
						mhuv2_sender_interrupt,
						IRQF_ONESHOT, "mhuv2-tx", mhu);
		if (ret) {
			dev_err(dev, "Failed to request tx IRQ, fallback to polling mode: %d\n",
				ret);
		} else {
			mhu->mbox.txdone_irq = true;
			mhu->mbox.txdone_poll = false;
			mhu->irq = adev->irq[0];

			writel_relaxed_bitfield(1, &mhu->send->int_en, struct int_en_t, chcomb);

			 
			for (i = 0; i < mhu->windows; i++)
				writel_relaxed(0x0, &mhu->send->ch_wn[i].int_en);

			goto out;
		}
	}

	mhu->mbox.txdone_irq = false;
	mhu->mbox.txdone_poll = true;
	mhu->mbox.txpoll_period = 1;

out:
	 
	writel_relaxed(0x1, &mhu->send->access_request);
	while (!readl_relaxed(&mhu->send->access_ready))
		continue;

	return 0;
}

static int mhuv2_rx_init(struct amba_device *adev, struct mhuv2 *mhu,
			 void __iomem *reg)
{
	struct device *dev = mhu->mbox.dev;
	int ret, i;

	mhu->frame = RECEIVER_FRAME;
	mhu->mbox.ops = &mhuv2_receiver_ops;
	mhu->recv = reg;

	mhu->windows = readl_relaxed_bitfield(&mhu->recv->mhu_cfg, struct mhu_cfg_t, num_ch);
	mhu->minor = readl_relaxed_bitfield(&mhu->recv->aidr, struct aidr_t, arch_minor_rev);

	mhu->irq = adev->irq[0];
	if (!mhu->irq) {
		dev_err(dev, "Missing receiver IRQ\n");
		return -EINVAL;
	}

	ret = devm_request_threaded_irq(dev, mhu->irq, NULL,
					mhuv2_receiver_interrupt, IRQF_ONESHOT,
					"mhuv2-rx", mhu);
	if (ret) {
		dev_err(dev, "Failed to request rx IRQ\n");
		return ret;
	}

	 
	for (i = 0; i < mhu->windows; i++)
		writel_relaxed(0xFFFFFFFF, &mhu->recv->ch_wn[i].mask_set);

	if (mhu->minor)
		writel_relaxed_bitfield(1, &mhu->recv->int_en, struct int_en_t, chcomb);

	return 0;
}

static int mhuv2_probe(struct amba_device *adev, const struct amba_id *id)
{
	struct device *dev = &adev->dev;
	const struct device_node *np = dev->of_node;
	struct mhuv2 *mhu;
	void __iomem *reg;
	int ret = -EINVAL;

	reg = devm_of_iomap(dev, dev->of_node, 0, NULL);
	if (IS_ERR(reg))
		return PTR_ERR(reg);

	mhu = devm_kzalloc(dev, sizeof(*mhu), GFP_KERNEL);
	if (!mhu)
		return -ENOMEM;

	mhu->mbox.dev = dev;
	mhu->mbox.of_xlate = mhuv2_mbox_of_xlate;

	if (of_device_is_compatible(np, "arm,mhuv2-tx"))
		ret = mhuv2_tx_init(adev, mhu, reg);
	else if (of_device_is_compatible(np, "arm,mhuv2-rx"))
		ret = mhuv2_rx_init(adev, mhu, reg);
	else
		dev_err(dev, "Invalid compatible property\n");

	if (ret)
		return ret;

	 
	BUG_ON(!mhu->windows);

	ret = mhuv2_parse_channels(mhu);
	if (ret)
		return ret;

	amba_set_drvdata(adev, mhu);

	ret = devm_mbox_controller_register(dev, &mhu->mbox);
	if (ret)
		dev_err(dev, "failed to register ARM MHUv2 driver %d\n", ret);

	return ret;
}

static void mhuv2_remove(struct amba_device *adev)
{
	struct mhuv2 *mhu = amba_get_drvdata(adev);

	if (mhu->frame == SENDER_FRAME)
		writel_relaxed(0x0, &mhu->send->access_request);
}

static struct amba_id mhuv2_ids[] = {
	{
		 
		.id = 0xbb0d1,
		.mask = 0xfffff,
	},
	{
		 
		.id = 0xbb076,
		.mask = 0xfffff,
	},
	{ 0, 0 },
};
MODULE_DEVICE_TABLE(amba, mhuv2_ids);

static struct amba_driver mhuv2_driver = {
	.drv = {
		.name	= "arm-mhuv2",
	},
	.id_table	= mhuv2_ids,
	.probe		= mhuv2_probe,
	.remove		= mhuv2_remove,
};
module_amba_driver(mhuv2_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("ARM MHUv2 Driver");
MODULE_AUTHOR("Viresh Kumar <viresh.kumar@linaro.org>");
MODULE_AUTHOR("Tushar Khandelwal <tushar.khandelwal@arm.com>");
