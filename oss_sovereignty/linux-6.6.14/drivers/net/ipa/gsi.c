

 

#include <linux/types.h>
#include <linux/bits.h>
#include <linux/bitfield.h>
#include <linux/mutex.h>
#include <linux/completion.h>
#include <linux/io.h>
#include <linux/bug.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/netdevice.h>

#include "gsi.h"
#include "reg.h"
#include "gsi_reg.h"
#include "gsi_private.h"
#include "gsi_trans.h"
#include "ipa_gsi.h"
#include "ipa_data.h"
#include "ipa_version.h"

 

 
#define GSI_EVT_RING_INT_MODT		(32 * 1)  

#define GSI_CMD_TIMEOUT			50	 

#define GSI_CHANNEL_STOP_RETRIES	10
#define GSI_CHANNEL_MODEM_HALT_RETRIES	10
#define GSI_CHANNEL_MODEM_FLOW_RETRIES	5	 

#define GSI_MHI_EVENT_ID_START		10	 
#define GSI_MHI_EVENT_ID_END		16	 

#define GSI_ISR_MAX_ITER		50	 

 
struct gsi_event {
	__le64 xfer_ptr;
	__le16 len;
	u8 reserved1;
	u8 code;
	__le16 reserved2;
	u8 type;
	u8 chid;
};

 
struct gsi_channel_scratch_gpi {
	u64 reserved1;
	u16 reserved2;
	u16 max_outstanding_tre;
	u16 reserved3;
	u16 outstanding_threshold;
};

 
union gsi_channel_scratch {
	struct gsi_channel_scratch_gpi gpi;
	struct {
		u32 word1;
		u32 word2;
		u32 word3;
		u32 word4;
	} data;
};

 
static void gsi_validate_build(void)
{
	 
	BUILD_BUG_ON(!GSI_RING_ELEMENT_SIZE);

	 
	BUILD_BUG_ON(sizeof(struct gsi_event) != GSI_RING_ELEMENT_SIZE);

	 
	BUILD_BUG_ON(!is_power_of_2(GSI_RING_ELEMENT_SIZE));
}

 
static u32 gsi_channel_id(struct gsi_channel *channel)
{
	return channel - &channel->gsi->channel[0];
}

 
static bool gsi_channel_initialized(struct gsi_channel *channel)
{
	return !!channel->gsi;
}

 
static u32 ch_c_cntxt_0_type_encode(enum ipa_version version,
				    const struct reg *reg,
				    enum gsi_channel_type type)
{
	u32 val;

	val = reg_encode(reg, CHTYPE_PROTOCOL, type);
	if (version < IPA_VERSION_4_5 || version >= IPA_VERSION_5_0)
		return val;

	type >>= hweight32(reg_fmask(reg, CHTYPE_PROTOCOL));

	return val | reg_encode(reg, CHTYPE_PROTOCOL_MSB, type);
}

 
static void gsi_irq_type_update(struct gsi *gsi, u32 val)
{
	const struct reg *reg = gsi_reg(gsi, CNTXT_TYPE_IRQ_MSK);

	gsi->type_enabled_bitmap = val;
	iowrite32(val, gsi->virt + reg_offset(reg));
}

static void gsi_irq_type_enable(struct gsi *gsi, enum gsi_irq_type_id type_id)
{
	gsi_irq_type_update(gsi, gsi->type_enabled_bitmap | type_id);
}

static void gsi_irq_type_disable(struct gsi *gsi, enum gsi_irq_type_id type_id)
{
	gsi_irq_type_update(gsi, gsi->type_enabled_bitmap & ~type_id);
}

 
static void gsi_irq_ev_ctrl_enable(struct gsi *gsi, u32 evt_ring_id)
{
	u32 val = BIT(evt_ring_id);
	const struct reg *reg;

	 
	reg = gsi_reg(gsi, CNTXT_SRC_EV_CH_IRQ_CLR);
	iowrite32(~0, gsi->virt + reg_offset(reg));

	reg = gsi_reg(gsi, CNTXT_SRC_EV_CH_IRQ_MSK);
	iowrite32(val, gsi->virt + reg_offset(reg));
	gsi_irq_type_enable(gsi, GSI_EV_CTRL);
}

 
static void gsi_irq_ev_ctrl_disable(struct gsi *gsi)
{
	const struct reg *reg;

	gsi_irq_type_disable(gsi, GSI_EV_CTRL);

	reg = gsi_reg(gsi, CNTXT_SRC_EV_CH_IRQ_MSK);
	iowrite32(0, gsi->virt + reg_offset(reg));
}

 
static void gsi_irq_ch_ctrl_enable(struct gsi *gsi, u32 channel_id)
{
	u32 val = BIT(channel_id);
	const struct reg *reg;

	 
	reg = gsi_reg(gsi, CNTXT_SRC_CH_IRQ_CLR);
	iowrite32(~0, gsi->virt + reg_offset(reg));

	reg = gsi_reg(gsi, CNTXT_SRC_CH_IRQ_MSK);
	iowrite32(val, gsi->virt + reg_offset(reg));

	gsi_irq_type_enable(gsi, GSI_CH_CTRL);
}

 
static void gsi_irq_ch_ctrl_disable(struct gsi *gsi)
{
	const struct reg *reg;

	gsi_irq_type_disable(gsi, GSI_CH_CTRL);

	reg = gsi_reg(gsi, CNTXT_SRC_CH_IRQ_MSK);
	iowrite32(0, gsi->virt + reg_offset(reg));
}

static void gsi_irq_ieob_enable_one(struct gsi *gsi, u32 evt_ring_id)
{
	bool enable_ieob = !gsi->ieob_enabled_bitmap;
	const struct reg *reg;
	u32 val;

	gsi->ieob_enabled_bitmap |= BIT(evt_ring_id);

	reg = gsi_reg(gsi, CNTXT_SRC_IEOB_IRQ_MSK);
	val = gsi->ieob_enabled_bitmap;
	iowrite32(val, gsi->virt + reg_offset(reg));

	 
	if (enable_ieob)
		gsi_irq_type_enable(gsi, GSI_IEOB);
}

static void gsi_irq_ieob_disable(struct gsi *gsi, u32 event_mask)
{
	const struct reg *reg;
	u32 val;

	gsi->ieob_enabled_bitmap &= ~event_mask;

	 
	if (!gsi->ieob_enabled_bitmap)
		gsi_irq_type_disable(gsi, GSI_IEOB);

	reg = gsi_reg(gsi, CNTXT_SRC_IEOB_IRQ_MSK);
	val = gsi->ieob_enabled_bitmap;
	iowrite32(val, gsi->virt + reg_offset(reg));
}

static void gsi_irq_ieob_disable_one(struct gsi *gsi, u32 evt_ring_id)
{
	gsi_irq_ieob_disable(gsi, BIT(evt_ring_id));
}

 
static void gsi_irq_enable(struct gsi *gsi)
{
	const struct reg *reg;
	u32 val;

	 
	reg = gsi_reg(gsi, CNTXT_GLOB_IRQ_EN);
	iowrite32(ERROR_INT, gsi->virt + reg_offset(reg));

	gsi_irq_type_update(gsi, gsi->type_enabled_bitmap | GSI_GLOB_EE);

	 
	reg = gsi_reg(gsi, CNTXT_GSI_IRQ_EN);
	val = BUS_ERROR;
	val |= CMD_FIFO_OVRFLOW;
	val |= MCS_STACK_OVRFLOW;
	iowrite32(val, gsi->virt + reg_offset(reg));

	gsi_irq_type_update(gsi, gsi->type_enabled_bitmap | GSI_GENERAL);
}

 
static void gsi_irq_disable(struct gsi *gsi)
{
	const struct reg *reg;

	gsi_irq_type_update(gsi, 0);

	 
	reg = gsi_reg(gsi, CNTXT_GSI_IRQ_EN);
	iowrite32(0, gsi->virt + reg_offset(reg));

	reg = gsi_reg(gsi, CNTXT_GLOB_IRQ_EN);
	iowrite32(0, gsi->virt + reg_offset(reg));
}

 
void *gsi_ring_virt(struct gsi_ring *ring, u32 index)
{
	 
	return ring->virt + (index % ring->count) * GSI_RING_ELEMENT_SIZE;
}

 
static u32 gsi_ring_addr(struct gsi_ring *ring, u32 index)
{
	return lower_32_bits(ring->addr) + index * GSI_RING_ELEMENT_SIZE;
}

 
static u32 gsi_ring_index(struct gsi_ring *ring, u32 offset)
{
	return (offset - gsi_ring_addr(ring, 0)) / GSI_RING_ELEMENT_SIZE;
}

 
static bool gsi_command(struct gsi *gsi, u32 reg, u32 val)
{
	unsigned long timeout = msecs_to_jiffies(GSI_CMD_TIMEOUT);
	struct completion *completion = &gsi->completion;

	reinit_completion(completion);

	iowrite32(val, gsi->virt + reg);

	return !!wait_for_completion_timeout(completion, timeout);
}

 
static enum gsi_evt_ring_state
gsi_evt_ring_state(struct gsi *gsi, u32 evt_ring_id)
{
	const struct reg *reg = gsi_reg(gsi, EV_CH_E_CNTXT_0);
	u32 val;

	val = ioread32(gsi->virt + reg_n_offset(reg, evt_ring_id));

	return reg_decode(reg, EV_CHSTATE, val);
}

 
static void gsi_evt_ring_command(struct gsi *gsi, u32 evt_ring_id,
				 enum gsi_evt_cmd_opcode opcode)
{
	struct device *dev = gsi->dev;
	const struct reg *reg;
	bool timeout;
	u32 val;

	 
	gsi_irq_ev_ctrl_enable(gsi, evt_ring_id);

	reg = gsi_reg(gsi, EV_CH_CMD);
	val = reg_encode(reg, EV_CHID, evt_ring_id);
	val |= reg_encode(reg, EV_OPCODE, opcode);

	timeout = !gsi_command(gsi, reg_offset(reg), val);

	gsi_irq_ev_ctrl_disable(gsi);

	if (!timeout)
		return;

	dev_err(dev, "GSI command %u for event ring %u timed out, state %u\n",
		opcode, evt_ring_id, gsi_evt_ring_state(gsi, evt_ring_id));
}

 
static int gsi_evt_ring_alloc_command(struct gsi *gsi, u32 evt_ring_id)
{
	enum gsi_evt_ring_state state;

	 
	state = gsi_evt_ring_state(gsi, evt_ring_id);
	if (state != GSI_EVT_RING_STATE_NOT_ALLOCATED) {
		dev_err(gsi->dev, "event ring %u bad state %u before alloc\n",
			evt_ring_id, state);
		return -EINVAL;
	}

	gsi_evt_ring_command(gsi, evt_ring_id, GSI_EVT_ALLOCATE);

	 
	state = gsi_evt_ring_state(gsi, evt_ring_id);
	if (state == GSI_EVT_RING_STATE_ALLOCATED)
		return 0;

	dev_err(gsi->dev, "event ring %u bad state %u after alloc\n",
		evt_ring_id, state);

	return -EIO;
}

 
static void gsi_evt_ring_reset_command(struct gsi *gsi, u32 evt_ring_id)
{
	enum gsi_evt_ring_state state;

	state = gsi_evt_ring_state(gsi, evt_ring_id);
	if (state != GSI_EVT_RING_STATE_ALLOCATED &&
	    state != GSI_EVT_RING_STATE_ERROR) {
		dev_err(gsi->dev, "event ring %u bad state %u before reset\n",
			evt_ring_id, state);
		return;
	}

	gsi_evt_ring_command(gsi, evt_ring_id, GSI_EVT_RESET);

	 
	state = gsi_evt_ring_state(gsi, evt_ring_id);
	if (state == GSI_EVT_RING_STATE_ALLOCATED)
		return;

	dev_err(gsi->dev, "event ring %u bad state %u after reset\n",
		evt_ring_id, state);
}

 
static void gsi_evt_ring_de_alloc_command(struct gsi *gsi, u32 evt_ring_id)
{
	enum gsi_evt_ring_state state;

	state = gsi_evt_ring_state(gsi, evt_ring_id);
	if (state != GSI_EVT_RING_STATE_ALLOCATED) {
		dev_err(gsi->dev, "event ring %u state %u before dealloc\n",
			evt_ring_id, state);
		return;
	}

	gsi_evt_ring_command(gsi, evt_ring_id, GSI_EVT_DE_ALLOC);

	 
	state = gsi_evt_ring_state(gsi, evt_ring_id);
	if (state == GSI_EVT_RING_STATE_NOT_ALLOCATED)
		return;

	dev_err(gsi->dev, "event ring %u bad state %u after dealloc\n",
		evt_ring_id, state);
}

 
static enum gsi_channel_state gsi_channel_state(struct gsi_channel *channel)
{
	const struct reg *reg = gsi_reg(channel->gsi, CH_C_CNTXT_0);
	u32 channel_id = gsi_channel_id(channel);
	struct gsi *gsi = channel->gsi;
	void __iomem *virt = gsi->virt;
	u32 val;

	reg = gsi_reg(gsi, CH_C_CNTXT_0);
	val = ioread32(virt + reg_n_offset(reg, channel_id));

	return reg_decode(reg, CHSTATE, val);
}

 
static void
gsi_channel_command(struct gsi_channel *channel, enum gsi_ch_cmd_opcode opcode)
{
	u32 channel_id = gsi_channel_id(channel);
	struct gsi *gsi = channel->gsi;
	struct device *dev = gsi->dev;
	const struct reg *reg;
	bool timeout;
	u32 val;

	 
	gsi_irq_ch_ctrl_enable(gsi, channel_id);

	reg = gsi_reg(gsi, CH_CMD);
	val = reg_encode(reg, CH_CHID, channel_id);
	val |= reg_encode(reg, CH_OPCODE, opcode);

	timeout = !gsi_command(gsi, reg_offset(reg), val);

	gsi_irq_ch_ctrl_disable(gsi);

	if (!timeout)
		return;

	dev_err(dev, "GSI command %u for channel %u timed out, state %u\n",
		opcode, channel_id, gsi_channel_state(channel));
}

 
static int gsi_channel_alloc_command(struct gsi *gsi, u32 channel_id)
{
	struct gsi_channel *channel = &gsi->channel[channel_id];
	struct device *dev = gsi->dev;
	enum gsi_channel_state state;

	 
	state = gsi_channel_state(channel);
	if (state != GSI_CHANNEL_STATE_NOT_ALLOCATED) {
		dev_err(dev, "channel %u bad state %u before alloc\n",
			channel_id, state);
		return -EINVAL;
	}

	gsi_channel_command(channel, GSI_CH_ALLOCATE);

	 
	state = gsi_channel_state(channel);
	if (state == GSI_CHANNEL_STATE_ALLOCATED)
		return 0;

	dev_err(dev, "channel %u bad state %u after alloc\n",
		channel_id, state);

	return -EIO;
}

 
static int gsi_channel_start_command(struct gsi_channel *channel)
{
	struct device *dev = channel->gsi->dev;
	enum gsi_channel_state state;

	state = gsi_channel_state(channel);
	if (state != GSI_CHANNEL_STATE_ALLOCATED &&
	    state != GSI_CHANNEL_STATE_STOPPED) {
		dev_err(dev, "channel %u bad state %u before start\n",
			gsi_channel_id(channel), state);
		return -EINVAL;
	}

	gsi_channel_command(channel, GSI_CH_START);

	 
	state = gsi_channel_state(channel);
	if (state == GSI_CHANNEL_STATE_STARTED)
		return 0;

	dev_err(dev, "channel %u bad state %u after start\n",
		gsi_channel_id(channel), state);

	return -EIO;
}

 
static int gsi_channel_stop_command(struct gsi_channel *channel)
{
	struct device *dev = channel->gsi->dev;
	enum gsi_channel_state state;

	state = gsi_channel_state(channel);

	 
	if (state == GSI_CHANNEL_STATE_STOPPED)
		return 0;

	if (state != GSI_CHANNEL_STATE_STARTED &&
	    state != GSI_CHANNEL_STATE_STOP_IN_PROC) {
		dev_err(dev, "channel %u bad state %u before stop\n",
			gsi_channel_id(channel), state);
		return -EINVAL;
	}

	gsi_channel_command(channel, GSI_CH_STOP);

	 
	state = gsi_channel_state(channel);
	if (state == GSI_CHANNEL_STATE_STOPPED)
		return 0;

	 
	if (state == GSI_CHANNEL_STATE_STOP_IN_PROC)
		return -EAGAIN;

	dev_err(dev, "channel %u bad state %u after stop\n",
		gsi_channel_id(channel), state);

	return -EIO;
}

 
static void gsi_channel_reset_command(struct gsi_channel *channel)
{
	struct device *dev = channel->gsi->dev;
	enum gsi_channel_state state;

	 
	usleep_range(USEC_PER_MSEC, 2 * USEC_PER_MSEC);

	state = gsi_channel_state(channel);
	if (state != GSI_CHANNEL_STATE_STOPPED &&
	    state != GSI_CHANNEL_STATE_ERROR) {
		 
		if (state != GSI_CHANNEL_STATE_ALLOCATED)
			dev_err(dev, "channel %u bad state %u before reset\n",
				gsi_channel_id(channel), state);
		return;
	}

	gsi_channel_command(channel, GSI_CH_RESET);

	 
	state = gsi_channel_state(channel);
	if (state != GSI_CHANNEL_STATE_ALLOCATED)
		dev_err(dev, "channel %u bad state %u after reset\n",
			gsi_channel_id(channel), state);
}

 
static void gsi_channel_de_alloc_command(struct gsi *gsi, u32 channel_id)
{
	struct gsi_channel *channel = &gsi->channel[channel_id];
	struct device *dev = gsi->dev;
	enum gsi_channel_state state;

	state = gsi_channel_state(channel);
	if (state != GSI_CHANNEL_STATE_ALLOCATED) {
		dev_err(dev, "channel %u bad state %u before dealloc\n",
			channel_id, state);
		return;
	}

	gsi_channel_command(channel, GSI_CH_DE_ALLOC);

	 
	state = gsi_channel_state(channel);

	if (state != GSI_CHANNEL_STATE_NOT_ALLOCATED)
		dev_err(dev, "channel %u bad state %u after dealloc\n",
			channel_id, state);
}

 
static void gsi_evt_ring_doorbell(struct gsi *gsi, u32 evt_ring_id, u32 index)
{
	const struct reg *reg = gsi_reg(gsi, EV_CH_E_DOORBELL_0);
	struct gsi_ring *ring = &gsi->evt_ring[evt_ring_id].ring;
	u32 val;

	ring->index = index;	 

	 
	val = gsi_ring_addr(ring, (index - 1) % ring->count);
	iowrite32(val, gsi->virt + reg_n_offset(reg, evt_ring_id));
}

 
static void gsi_evt_ring_program(struct gsi *gsi, u32 evt_ring_id)
{
	struct gsi_evt_ring *evt_ring = &gsi->evt_ring[evt_ring_id];
	struct gsi_ring *ring = &evt_ring->ring;
	const struct reg *reg;
	u32 val;

	reg = gsi_reg(gsi, EV_CH_E_CNTXT_0);
	 
	val = reg_encode(reg, EV_CHTYPE, GSI_CHANNEL_TYPE_GPI);
	 
	val |= reg_bit(reg, EV_INTYPE);
	val |= reg_encode(reg, EV_ELEMENT_SIZE, GSI_RING_ELEMENT_SIZE);
	iowrite32(val, gsi->virt + reg_n_offset(reg, evt_ring_id));

	reg = gsi_reg(gsi, EV_CH_E_CNTXT_1);
	val = reg_encode(reg, R_LENGTH, ring->count * GSI_RING_ELEMENT_SIZE);
	iowrite32(val, gsi->virt + reg_n_offset(reg, evt_ring_id));

	 
	reg = gsi_reg(gsi, EV_CH_E_CNTXT_2);
	val = lower_32_bits(ring->addr);
	iowrite32(val, gsi->virt + reg_n_offset(reg, evt_ring_id));

	reg = gsi_reg(gsi, EV_CH_E_CNTXT_3);
	val = upper_32_bits(ring->addr);
	iowrite32(val, gsi->virt + reg_n_offset(reg, evt_ring_id));

	 
	reg = gsi_reg(gsi, EV_CH_E_CNTXT_8);
	val = reg_encode(reg, EV_MODT, GSI_EVT_RING_INT_MODT);
	val |= reg_encode(reg, EV_MODC, 1);	 
	 
	iowrite32(val, gsi->virt + reg_n_offset(reg, evt_ring_id));

	 
	reg = gsi_reg(gsi, EV_CH_E_CNTXT_9);
	iowrite32(0, gsi->virt + reg_n_offset(reg, evt_ring_id));

	reg = gsi_reg(gsi, EV_CH_E_CNTXT_10);
	iowrite32(0, gsi->virt + reg_n_offset(reg, evt_ring_id));

	reg = gsi_reg(gsi, EV_CH_E_CNTXT_11);
	iowrite32(0, gsi->virt + reg_n_offset(reg, evt_ring_id));

	 
	reg = gsi_reg(gsi, EV_CH_E_CNTXT_12);
	iowrite32(0, gsi->virt + reg_n_offset(reg, evt_ring_id));

	reg = gsi_reg(gsi, EV_CH_E_CNTXT_13);
	iowrite32(0, gsi->virt + reg_n_offset(reg, evt_ring_id));

	 
	gsi_evt_ring_doorbell(gsi, evt_ring_id, ring->index);
}

 
static struct gsi_trans *gsi_channel_trans_last(struct gsi_channel *channel)
{
	struct gsi_trans_info *trans_info = &channel->trans_info;
	u32 pending_id = trans_info->pending_id;
	struct gsi_trans *trans;
	u16 trans_id;

	if (channel->toward_ipa && pending_id != trans_info->free_id) {
		 
		trans_id = trans_info->free_id - 1;
	} else if (trans_info->polled_id != pending_id) {
		 
		trans_id = pending_id - 1;
	} else {
		return NULL;
	}

	 
	trans = &trans_info->trans[trans_id % channel->tre_count];
	refcount_inc(&trans->refcount);

	return trans;
}

 
static void gsi_channel_trans_quiesce(struct gsi_channel *channel)
{
	struct gsi_trans *trans;

	 
	trans = gsi_channel_trans_last(channel);
	if (trans) {
		wait_for_completion(&trans->completion);
		gsi_trans_free(trans);
	}
}

 
static void gsi_channel_program(struct gsi_channel *channel, bool doorbell)
{
	size_t size = channel->tre_ring.count * GSI_RING_ELEMENT_SIZE;
	u32 channel_id = gsi_channel_id(channel);
	union gsi_channel_scratch scr = { };
	struct gsi_channel_scratch_gpi *gpi;
	struct gsi *gsi = channel->gsi;
	const struct reg *reg;
	u32 wrr_weight = 0;
	u32 offset;
	u32 val;

	reg = gsi_reg(gsi, CH_C_CNTXT_0);

	 
	val = ch_c_cntxt_0_type_encode(gsi->version, reg, GSI_CHANNEL_TYPE_GPI);
	if (channel->toward_ipa)
		val |= reg_bit(reg, CHTYPE_DIR);
	if (gsi->version < IPA_VERSION_5_0)
		val |= reg_encode(reg, ERINDEX, channel->evt_ring_id);
	val |= reg_encode(reg, ELEMENT_SIZE, GSI_RING_ELEMENT_SIZE);
	iowrite32(val, gsi->virt + reg_n_offset(reg, channel_id));

	reg = gsi_reg(gsi, CH_C_CNTXT_1);
	val = reg_encode(reg, CH_R_LENGTH, size);
	if (gsi->version >= IPA_VERSION_5_0)
		val |= reg_encode(reg, CH_ERINDEX, channel->evt_ring_id);
	iowrite32(val, gsi->virt + reg_n_offset(reg, channel_id));

	 
	reg = gsi_reg(gsi, CH_C_CNTXT_2);
	val = lower_32_bits(channel->tre_ring.addr);
	iowrite32(val, gsi->virt + reg_n_offset(reg, channel_id));

	reg = gsi_reg(gsi, CH_C_CNTXT_3);
	val = upper_32_bits(channel->tre_ring.addr);
	iowrite32(val, gsi->virt + reg_n_offset(reg, channel_id));

	reg = gsi_reg(gsi, CH_C_QOS);

	 
	if (channel->command)
		wrr_weight = reg_field_max(reg, WRR_WEIGHT);
	val = reg_encode(reg, WRR_WEIGHT, wrr_weight);

	 

	 
	if (gsi->version < IPA_VERSION_4_0 && doorbell)
		val |= reg_bit(reg, USE_DB_ENG);

	 
	if (gsi->version >= IPA_VERSION_4_0 && !channel->command) {
		 
		if (gsi->version < IPA_VERSION_4_5)
			val |= reg_bit(reg, USE_ESCAPE_BUF_ONLY);
		else
			val |= reg_encode(reg, PREFETCH_MODE, ESCAPE_BUF_ONLY);
	}
	 
	if (gsi->version >= IPA_VERSION_4_9)
		val |= reg_bit(reg, DB_IN_BYTES);

	iowrite32(val, gsi->virt + reg_n_offset(reg, channel_id));

	 
	gpi = &scr.gpi;
	gpi->max_outstanding_tre = channel->trans_tre_max *
					GSI_RING_ELEMENT_SIZE;
	gpi->outstanding_threshold = 2 * GSI_RING_ELEMENT_SIZE;

	reg = gsi_reg(gsi, CH_C_SCRATCH_0);
	val = scr.data.word1;
	iowrite32(val, gsi->virt + reg_n_offset(reg, channel_id));

	reg = gsi_reg(gsi, CH_C_SCRATCH_1);
	val = scr.data.word2;
	iowrite32(val, gsi->virt + reg_n_offset(reg, channel_id));

	reg = gsi_reg(gsi, CH_C_SCRATCH_2);
	val = scr.data.word3;
	iowrite32(val, gsi->virt + reg_n_offset(reg, channel_id));

	 
	reg = gsi_reg(gsi, CH_C_SCRATCH_3);
	offset = reg_n_offset(reg, channel_id);
	val = ioread32(gsi->virt + offset);
	val = (scr.data.word4 & GENMASK(31, 16)) | (val & GENMASK(15, 0));
	iowrite32(val, gsi->virt + offset);

	 
}

static int __gsi_channel_start(struct gsi_channel *channel, bool resume)
{
	struct gsi *gsi = channel->gsi;
	int ret;

	 
	if (resume && gsi->version < IPA_VERSION_4_0)
		return 0;

	mutex_lock(&gsi->mutex);

	ret = gsi_channel_start_command(channel);

	mutex_unlock(&gsi->mutex);

	return ret;
}

 
int gsi_channel_start(struct gsi *gsi, u32 channel_id)
{
	struct gsi_channel *channel = &gsi->channel[channel_id];
	int ret;

	 
	napi_enable(&channel->napi);
	gsi_irq_ieob_enable_one(gsi, channel->evt_ring_id);

	ret = __gsi_channel_start(channel, false);
	if (ret) {
		gsi_irq_ieob_disable_one(gsi, channel->evt_ring_id);
		napi_disable(&channel->napi);
	}

	return ret;
}

static int gsi_channel_stop_retry(struct gsi_channel *channel)
{
	u32 retries = GSI_CHANNEL_STOP_RETRIES;
	int ret;

	do {
		ret = gsi_channel_stop_command(channel);
		if (ret != -EAGAIN)
			break;
		usleep_range(3 * USEC_PER_MSEC, 5 * USEC_PER_MSEC);
	} while (retries--);

	return ret;
}

static int __gsi_channel_stop(struct gsi_channel *channel, bool suspend)
{
	struct gsi *gsi = channel->gsi;
	int ret;

	 
	gsi_channel_trans_quiesce(channel);

	 
	if (suspend && gsi->version < IPA_VERSION_4_0)
		return 0;

	mutex_lock(&gsi->mutex);

	ret = gsi_channel_stop_retry(channel);

	mutex_unlock(&gsi->mutex);

	return ret;
}

 
int gsi_channel_stop(struct gsi *gsi, u32 channel_id)
{
	struct gsi_channel *channel = &gsi->channel[channel_id];
	int ret;

	ret = __gsi_channel_stop(channel, false);
	if (ret)
		return ret;

	 
	gsi_irq_ieob_disable_one(gsi, channel->evt_ring_id);
	napi_disable(&channel->napi);

	return 0;
}

 
void gsi_channel_reset(struct gsi *gsi, u32 channel_id, bool doorbell)
{
	struct gsi_channel *channel = &gsi->channel[channel_id];

	mutex_lock(&gsi->mutex);

	gsi_channel_reset_command(channel);
	 
	if (gsi->version < IPA_VERSION_4_0 && !channel->toward_ipa)
		gsi_channel_reset_command(channel);

	 
	channel->tre_ring.index = 0;
	gsi_channel_program(channel, doorbell);
	gsi_channel_trans_cancel_pending(channel);

	mutex_unlock(&gsi->mutex);
}

 
int gsi_channel_suspend(struct gsi *gsi, u32 channel_id)
{
	struct gsi_channel *channel = &gsi->channel[channel_id];
	int ret;

	ret = __gsi_channel_stop(channel, true);
	if (ret)
		return ret;

	 
	napi_synchronize(&channel->napi);

	return 0;
}

 
int gsi_channel_resume(struct gsi *gsi, u32 channel_id)
{
	struct gsi_channel *channel = &gsi->channel[channel_id];

	return __gsi_channel_start(channel, true);
}

 
void gsi_suspend(struct gsi *gsi)
{
	disable_irq(gsi->irq);
}

 
void gsi_resume(struct gsi *gsi)
{
	enable_irq(gsi->irq);
}

void gsi_trans_tx_committed(struct gsi_trans *trans)
{
	struct gsi_channel *channel = &trans->gsi->channel[trans->channel_id];

	channel->trans_count++;
	channel->byte_count += trans->len;

	trans->trans_count = channel->trans_count;
	trans->byte_count = channel->byte_count;
}

void gsi_trans_tx_queued(struct gsi_trans *trans)
{
	u32 channel_id = trans->channel_id;
	struct gsi *gsi = trans->gsi;
	struct gsi_channel *channel;
	u32 trans_count;
	u32 byte_count;

	channel = &gsi->channel[channel_id];

	byte_count = channel->byte_count - channel->queued_byte_count;
	trans_count = channel->trans_count - channel->queued_trans_count;
	channel->queued_byte_count = channel->byte_count;
	channel->queued_trans_count = channel->trans_count;

	ipa_gsi_channel_tx_queued(gsi, channel_id, trans_count, byte_count);
}

 
static void gsi_trans_tx_completed(struct gsi_trans *trans)
{
	u32 channel_id = trans->channel_id;
	struct gsi *gsi = trans->gsi;
	struct gsi_channel *channel;
	u32 trans_count;
	u32 byte_count;

	channel = &gsi->channel[channel_id];
	trans_count = trans->trans_count - channel->compl_trans_count;
	byte_count = trans->byte_count - channel->compl_byte_count;

	channel->compl_trans_count += trans_count;
	channel->compl_byte_count += byte_count;

	ipa_gsi_channel_tx_completed(gsi, channel_id, trans_count, byte_count);
}

 
static void gsi_isr_chan_ctrl(struct gsi *gsi)
{
	const struct reg *reg;
	u32 channel_mask;

	reg = gsi_reg(gsi, CNTXT_SRC_CH_IRQ);
	channel_mask = ioread32(gsi->virt + reg_offset(reg));

	reg = gsi_reg(gsi, CNTXT_SRC_CH_IRQ_CLR);
	iowrite32(channel_mask, gsi->virt + reg_offset(reg));

	while (channel_mask) {
		u32 channel_id = __ffs(channel_mask);

		channel_mask ^= BIT(channel_id);

		complete(&gsi->completion);
	}
}

 
static void gsi_isr_evt_ctrl(struct gsi *gsi)
{
	const struct reg *reg;
	u32 event_mask;

	reg = gsi_reg(gsi, CNTXT_SRC_EV_CH_IRQ);
	event_mask = ioread32(gsi->virt + reg_offset(reg));

	reg = gsi_reg(gsi, CNTXT_SRC_EV_CH_IRQ_CLR);
	iowrite32(event_mask, gsi->virt + reg_offset(reg));

	while (event_mask) {
		u32 evt_ring_id = __ffs(event_mask);

		event_mask ^= BIT(evt_ring_id);

		complete(&gsi->completion);
	}
}

 
static void
gsi_isr_glob_chan_err(struct gsi *gsi, u32 err_ee, u32 channel_id, u32 code)
{
	if (code == GSI_OUT_OF_RESOURCES) {
		dev_err(gsi->dev, "channel %u out of resources\n", channel_id);
		complete(&gsi->completion);
		return;
	}

	 
	dev_err(gsi->dev, "channel %u global error ee 0x%08x code 0x%08x\n",
		channel_id, err_ee, code);
}

 
static void
gsi_isr_glob_evt_err(struct gsi *gsi, u32 err_ee, u32 evt_ring_id, u32 code)
{
	if (code == GSI_OUT_OF_RESOURCES) {
		struct gsi_evt_ring *evt_ring = &gsi->evt_ring[evt_ring_id];
		u32 channel_id = gsi_channel_id(evt_ring->channel);

		complete(&gsi->completion);
		dev_err(gsi->dev, "evt_ring for channel %u out of resources\n",
			channel_id);
		return;
	}

	 
	dev_err(gsi->dev, "event ring %u global error ee %u code 0x%08x\n",
		evt_ring_id, err_ee, code);
}

 
static void gsi_isr_glob_err(struct gsi *gsi)
{
	const struct reg *log_reg;
	const struct reg *clr_reg;
	enum gsi_err_type type;
	enum gsi_err_code code;
	u32 offset;
	u32 which;
	u32 val;
	u32 ee;

	 
	log_reg = gsi_reg(gsi, ERROR_LOG);
	offset = reg_offset(log_reg);
	val = ioread32(gsi->virt + offset);
	iowrite32(0, gsi->virt + offset);

	clr_reg = gsi_reg(gsi, ERROR_LOG_CLR);
	iowrite32(~0, gsi->virt + reg_offset(clr_reg));

	 
	ee = reg_decode(log_reg, ERR_EE, val);
	type = reg_decode(log_reg, ERR_TYPE, val);
	which = reg_decode(log_reg, ERR_VIRT_IDX, val);
	code = reg_decode(log_reg, ERR_CODE, val);

	if (type == GSI_ERR_TYPE_CHAN)
		gsi_isr_glob_chan_err(gsi, ee, which, code);
	else if (type == GSI_ERR_TYPE_EVT)
		gsi_isr_glob_evt_err(gsi, ee, which, code);
	else	 
		dev_err(gsi->dev, "unexpected global error 0x%08x\n", type);
}

 
static void gsi_isr_gp_int1(struct gsi *gsi)
{
	const struct reg *reg;
	u32 result;
	u32 val;

	 
	reg = gsi_reg(gsi, CNTXT_SCRATCH_0);
	val = ioread32(gsi->virt + reg_offset(reg));
	result = reg_decode(reg, GENERIC_EE_RESULT, val);

	switch (result) {
	case GENERIC_EE_SUCCESS:
	case GENERIC_EE_INCORRECT_CHANNEL_STATE:
		gsi->result = 0;
		break;

	case GENERIC_EE_RETRY:
		gsi->result = -EAGAIN;
		break;

	default:
		dev_err(gsi->dev, "global INT1 generic result %u\n", result);
		gsi->result = -EIO;
		break;
	}

	complete(&gsi->completion);
}

 
static void gsi_isr_glob_ee(struct gsi *gsi)
{
	const struct reg *reg;
	u32 val;

	reg = gsi_reg(gsi, CNTXT_GLOB_IRQ_STTS);
	val = ioread32(gsi->virt + reg_offset(reg));

	if (val & ERROR_INT)
		gsi_isr_glob_err(gsi);

	reg = gsi_reg(gsi, CNTXT_GLOB_IRQ_CLR);
	iowrite32(val, gsi->virt + reg_offset(reg));

	val &= ~ERROR_INT;

	if (val & GP_INT1) {
		val ^= GP_INT1;
		gsi_isr_gp_int1(gsi);
	}

	if (val)
		dev_err(gsi->dev, "unexpected global interrupt 0x%08x\n", val);
}

 
static void gsi_isr_ieob(struct gsi *gsi)
{
	const struct reg *reg;
	u32 event_mask;

	reg = gsi_reg(gsi, CNTXT_SRC_IEOB_IRQ);
	event_mask = ioread32(gsi->virt + reg_offset(reg));

	gsi_irq_ieob_disable(gsi, event_mask);

	reg = gsi_reg(gsi, CNTXT_SRC_IEOB_IRQ_CLR);
	iowrite32(event_mask, gsi->virt + reg_offset(reg));

	while (event_mask) {
		u32 evt_ring_id = __ffs(event_mask);

		event_mask ^= BIT(evt_ring_id);

		napi_schedule(&gsi->evt_ring[evt_ring_id].channel->napi);
	}
}

 
static void gsi_isr_general(struct gsi *gsi)
{
	struct device *dev = gsi->dev;
	const struct reg *reg;
	u32 val;

	reg = gsi_reg(gsi, CNTXT_GSI_IRQ_STTS);
	val = ioread32(gsi->virt + reg_offset(reg));

	reg = gsi_reg(gsi, CNTXT_GSI_IRQ_CLR);
	iowrite32(val, gsi->virt + reg_offset(reg));

	dev_err(dev, "unexpected general interrupt 0x%08x\n", val);
}

 
static irqreturn_t gsi_isr(int irq, void *dev_id)
{
	struct gsi *gsi = dev_id;
	const struct reg *reg;
	u32 intr_mask;
	u32 cnt = 0;
	u32 offset;

	reg = gsi_reg(gsi, CNTXT_TYPE_IRQ);
	offset = reg_offset(reg);

	 
	while ((intr_mask = ioread32(gsi->virt + offset))) {
		 
		do {
			u32 gsi_intr = BIT(__ffs(intr_mask));

			intr_mask ^= gsi_intr;

			 
			switch (gsi_intr) {
			case GSI_CH_CTRL:
				gsi_isr_chan_ctrl(gsi);
				break;
			case GSI_EV_CTRL:
				gsi_isr_evt_ctrl(gsi);
				break;
			case GSI_GLOB_EE:
				gsi_isr_glob_ee(gsi);
				break;
			case GSI_IEOB:
				gsi_isr_ieob(gsi);
				break;
			case GSI_GENERAL:
				gsi_isr_general(gsi);
				break;
			default:
				dev_err(gsi->dev,
					"unrecognized interrupt type 0x%08x\n",
					gsi_intr);
				break;
			}
		} while (intr_mask);

		if (++cnt > GSI_ISR_MAX_ITER) {
			dev_err(gsi->dev, "interrupt flood\n");
			break;
		}
	}

	return IRQ_HANDLED;
}

 
static int gsi_irq_init(struct gsi *gsi, struct platform_device *pdev)
{
	int ret;

	ret = platform_get_irq_byname(pdev, "gsi");
	if (ret <= 0)
		return ret ? : -EINVAL;

	gsi->irq = ret;

	return 0;
}

 
static struct gsi_trans *
gsi_event_trans(struct gsi *gsi, struct gsi_event *event)
{
	u32 channel_id = event->chid;
	struct gsi_channel *channel;
	struct gsi_trans *trans;
	u32 tre_offset;
	u32 tre_index;

	channel = &gsi->channel[channel_id];
	if (WARN(!channel->gsi, "event has bad channel %u\n", channel_id))
		return NULL;

	 
	tre_offset = lower_32_bits(le64_to_cpu(event->xfer_ptr));
	tre_index = gsi_ring_index(&channel->tre_ring, tre_offset);

	trans = gsi_channel_trans_mapped(channel, tre_index);

	if (WARN(!trans, "channel %u event with no transaction\n", channel_id))
		return NULL;

	return trans;
}

 
static void gsi_evt_ring_update(struct gsi *gsi, u32 evt_ring_id, u32 index)
{
	struct gsi_evt_ring *evt_ring = &gsi->evt_ring[evt_ring_id];
	struct gsi_ring *ring = &evt_ring->ring;
	struct gsi_event *event_done;
	struct gsi_event *event;
	u32 event_avail;
	u32 old_index;

	 
	old_index = ring->index;
	event = gsi_ring_virt(ring, old_index);

	 
	event_avail = ring->count - old_index % ring->count;
	event_done = gsi_ring_virt(ring, index);
	do {
		struct gsi_trans *trans;

		trans = gsi_event_trans(gsi, event);
		if (!trans)
			return;

		if (trans->direction == DMA_FROM_DEVICE)
			trans->len = __le16_to_cpu(event->len);
		else
			gsi_trans_tx_completed(trans);

		gsi_trans_move_complete(trans);

		 
		if (--event_avail)
			event++;
		else
			event = gsi_ring_virt(ring, 0);
	} while (event != event_done);

	 
	gsi_evt_ring_doorbell(gsi, evt_ring_id, index);
}

 
static int gsi_ring_alloc(struct gsi *gsi, struct gsi_ring *ring, u32 count)
{
	u32 size = count * GSI_RING_ELEMENT_SIZE;
	struct device *dev = gsi->dev;
	dma_addr_t addr;

	 
	ring->virt = dma_alloc_coherent(dev, size, &addr, GFP_KERNEL);
	if (!ring->virt)
		return -ENOMEM;

	ring->addr = addr;
	ring->count = count;
	ring->index = 0;

	return 0;
}

 
static void gsi_ring_free(struct gsi *gsi, struct gsi_ring *ring)
{
	size_t size = ring->count * GSI_RING_ELEMENT_SIZE;

	dma_free_coherent(gsi->dev, size, ring->virt, ring->addr);
}

 
static int gsi_evt_ring_id_alloc(struct gsi *gsi)
{
	u32 evt_ring_id;

	if (gsi->event_bitmap == ~0U) {
		dev_err(gsi->dev, "event rings exhausted\n");
		return -ENOSPC;
	}

	evt_ring_id = ffz(gsi->event_bitmap);
	gsi->event_bitmap |= BIT(evt_ring_id);

	return (int)evt_ring_id;
}

 
static void gsi_evt_ring_id_free(struct gsi *gsi, u32 evt_ring_id)
{
	gsi->event_bitmap &= ~BIT(evt_ring_id);
}

 
void gsi_channel_doorbell(struct gsi_channel *channel)
{
	struct gsi_ring *tre_ring = &channel->tre_ring;
	u32 channel_id = gsi_channel_id(channel);
	struct gsi *gsi = channel->gsi;
	const struct reg *reg;
	u32 val;

	reg = gsi_reg(gsi, CH_C_DOORBELL_0);
	 
	val = gsi_ring_addr(tre_ring, tre_ring->index % tre_ring->count);
	iowrite32(val, gsi->virt + reg_n_offset(reg, channel_id));
}

 
void gsi_channel_update(struct gsi_channel *channel)
{
	u32 evt_ring_id = channel->evt_ring_id;
	struct gsi *gsi = channel->gsi;
	struct gsi_evt_ring *evt_ring;
	struct gsi_trans *trans;
	struct gsi_ring *ring;
	const struct reg *reg;
	u32 offset;
	u32 index;

	evt_ring = &gsi->evt_ring[evt_ring_id];
	ring = &evt_ring->ring;

	 
	reg = gsi_reg(gsi, EV_CH_E_CNTXT_4);
	offset = reg_n_offset(reg, evt_ring_id);
	index = gsi_ring_index(ring, ioread32(gsi->virt + offset));
	if (index == ring->index % ring->count)
		return;

	 
	trans = gsi_event_trans(gsi, gsi_ring_virt(ring, index - 1));
	if (!trans)
		return;

	 
	gsi_evt_ring_update(gsi, evt_ring_id, index);
}

 
static struct gsi_trans *gsi_channel_poll_one(struct gsi_channel *channel)
{
	struct gsi_trans *trans;

	 
	trans = gsi_channel_trans_complete(channel);
	if (trans)
		gsi_trans_move_polled(trans);

	return trans;
}

 
static int gsi_channel_poll(struct napi_struct *napi, int budget)
{
	struct gsi_channel *channel;
	int count;

	channel = container_of(napi, struct gsi_channel, napi);
	for (count = 0; count < budget; count++) {
		struct gsi_trans *trans;

		trans = gsi_channel_poll_one(channel);
		if (!trans)
			break;
		gsi_trans_complete(trans);
	}

	if (count < budget && napi_complete(napi))
		gsi_irq_ieob_enable_one(channel->gsi, channel->evt_ring_id);

	return count;
}

 
static u32 gsi_event_bitmap_init(u32 evt_ring_max)
{
	u32 event_bitmap = GENMASK(BITS_PER_LONG - 1, evt_ring_max);

	event_bitmap |= GENMASK(GSI_MHI_EVENT_ID_END, GSI_MHI_EVENT_ID_START);

	return event_bitmap;
}

 
static int gsi_channel_setup_one(struct gsi *gsi, u32 channel_id)
{
	struct gsi_channel *channel = &gsi->channel[channel_id];
	u32 evt_ring_id = channel->evt_ring_id;
	int ret;

	if (!gsi_channel_initialized(channel))
		return 0;

	ret = gsi_evt_ring_alloc_command(gsi, evt_ring_id);
	if (ret)
		return ret;

	gsi_evt_ring_program(gsi, evt_ring_id);

	ret = gsi_channel_alloc_command(gsi, channel_id);
	if (ret)
		goto err_evt_ring_de_alloc;

	gsi_channel_program(channel, true);

	if (channel->toward_ipa)
		netif_napi_add_tx(&gsi->dummy_dev, &channel->napi,
				  gsi_channel_poll);
	else
		netif_napi_add(&gsi->dummy_dev, &channel->napi,
			       gsi_channel_poll);

	return 0;

err_evt_ring_de_alloc:
	 
	gsi_evt_ring_de_alloc_command(gsi, evt_ring_id);

	return ret;
}

 
static void gsi_channel_teardown_one(struct gsi *gsi, u32 channel_id)
{
	struct gsi_channel *channel = &gsi->channel[channel_id];
	u32 evt_ring_id = channel->evt_ring_id;

	if (!gsi_channel_initialized(channel))
		return;

	netif_napi_del(&channel->napi);

	gsi_channel_de_alloc_command(gsi, channel_id);
	gsi_evt_ring_reset_command(gsi, evt_ring_id);
	gsi_evt_ring_de_alloc_command(gsi, evt_ring_id);
}

 
static int gsi_generic_command(struct gsi *gsi, u32 channel_id,
			       enum gsi_generic_cmd_opcode opcode,
			       u8 params)
{
	const struct reg *reg;
	bool timeout;
	u32 offset;
	u32 val;

	 
	reg = gsi_reg(gsi, CNTXT_GLOB_IRQ_EN);
	val = ERROR_INT | GP_INT1;
	iowrite32(val, gsi->virt + reg_offset(reg));

	 
	reg = gsi_reg(gsi, CNTXT_SCRATCH_0);
	offset = reg_offset(reg);
	val = ioread32(gsi->virt + offset);

	val &= ~reg_fmask(reg, GENERIC_EE_RESULT);
	iowrite32(val, gsi->virt + offset);

	 
	reg = gsi_reg(gsi, GENERIC_CMD);
	val = reg_encode(reg, GENERIC_OPCODE, opcode);
	val |= reg_encode(reg, GENERIC_CHID, channel_id);
	val |= reg_encode(reg, GENERIC_EE, GSI_EE_MODEM);
	if (gsi->version >= IPA_VERSION_4_11)
		val |= reg_encode(reg, GENERIC_PARAMS, params);

	timeout = !gsi_command(gsi, reg_offset(reg), val);

	 
	reg = gsi_reg(gsi, CNTXT_GLOB_IRQ_EN);
	iowrite32(ERROR_INT, gsi->virt + reg_offset(reg));

	if (!timeout)
		return gsi->result;

	dev_err(gsi->dev, "GSI generic command %u to channel %u timed out\n",
		opcode, channel_id);

	return -ETIMEDOUT;
}

static int gsi_modem_channel_alloc(struct gsi *gsi, u32 channel_id)
{
	return gsi_generic_command(gsi, channel_id,
				   GSI_GENERIC_ALLOCATE_CHANNEL, 0);
}

static void gsi_modem_channel_halt(struct gsi *gsi, u32 channel_id)
{
	u32 retries = GSI_CHANNEL_MODEM_HALT_RETRIES;
	int ret;

	do
		ret = gsi_generic_command(gsi, channel_id,
					  GSI_GENERIC_HALT_CHANNEL, 0);
	while (ret == -EAGAIN && retries--);

	if (ret)
		dev_err(gsi->dev, "error %d halting modem channel %u\n",
			ret, channel_id);
}

 
void
gsi_modem_channel_flow_control(struct gsi *gsi, u32 channel_id, bool enable)
{
	u32 retries = 0;
	u32 command;
	int ret;

	command = enable ? GSI_GENERIC_ENABLE_FLOW_CONTROL
			 : GSI_GENERIC_DISABLE_FLOW_CONTROL;
	 
	if (!enable && gsi->version >= IPA_VERSION_4_11)
		retries = GSI_CHANNEL_MODEM_FLOW_RETRIES;

	do
		ret = gsi_generic_command(gsi, channel_id, command, 0);
	while (ret == -EAGAIN && retries--);

	if (ret)
		dev_err(gsi->dev,
			"error %d %sabling mode channel %u flow control\n",
			ret, enable ? "en" : "dis", channel_id);
}

 
static int gsi_channel_setup(struct gsi *gsi)
{
	u32 channel_id = 0;
	u32 mask;
	int ret;

	gsi_irq_enable(gsi);

	mutex_lock(&gsi->mutex);

	do {
		ret = gsi_channel_setup_one(gsi, channel_id);
		if (ret)
			goto err_unwind;
	} while (++channel_id < gsi->channel_count);

	 
	while (channel_id < GSI_CHANNEL_COUNT_MAX) {
		struct gsi_channel *channel = &gsi->channel[channel_id++];

		if (!gsi_channel_initialized(channel))
			continue;

		ret = -EINVAL;
		dev_err(gsi->dev, "channel %u not supported by hardware\n",
			channel_id - 1);
		channel_id = gsi->channel_count;
		goto err_unwind;
	}

	 
	mask = gsi->modem_channel_bitmap;
	while (mask) {
		u32 modem_channel_id = __ffs(mask);

		ret = gsi_modem_channel_alloc(gsi, modem_channel_id);
		if (ret)
			goto err_unwind_modem;

		 
		mask ^= BIT(modem_channel_id);
	}

	mutex_unlock(&gsi->mutex);

	return 0;

err_unwind_modem:
	 
	mask ^= gsi->modem_channel_bitmap;
	while (mask) {
		channel_id = __fls(mask);

		mask ^= BIT(channel_id);

		gsi_modem_channel_halt(gsi, channel_id);
	}

err_unwind:
	while (channel_id--)
		gsi_channel_teardown_one(gsi, channel_id);

	mutex_unlock(&gsi->mutex);

	gsi_irq_disable(gsi);

	return ret;
}

 
static void gsi_channel_teardown(struct gsi *gsi)
{
	u32 mask = gsi->modem_channel_bitmap;
	u32 channel_id;

	mutex_lock(&gsi->mutex);

	while (mask) {
		channel_id = __fls(mask);

		mask ^= BIT(channel_id);

		gsi_modem_channel_halt(gsi, channel_id);
	}

	channel_id = gsi->channel_count - 1;
	do
		gsi_channel_teardown_one(gsi, channel_id);
	while (channel_id--);

	mutex_unlock(&gsi->mutex);

	gsi_irq_disable(gsi);
}

 
static int gsi_irq_setup(struct gsi *gsi)
{
	const struct reg *reg;
	int ret;

	 
	reg = gsi_reg(gsi, CNTXT_INTSET);
	iowrite32(reg_bit(reg, INTYPE), gsi->virt + reg_offset(reg));

	 
	gsi_irq_type_update(gsi, 0);

	 
	reg = gsi_reg(gsi, CNTXT_SRC_CH_IRQ_MSK);
	iowrite32(0, gsi->virt + reg_offset(reg));

	reg = gsi_reg(gsi, CNTXT_SRC_EV_CH_IRQ_MSK);
	iowrite32(0, gsi->virt + reg_offset(reg));

	reg = gsi_reg(gsi, CNTXT_GLOB_IRQ_EN);
	iowrite32(0, gsi->virt + reg_offset(reg));

	reg = gsi_reg(gsi, CNTXT_SRC_IEOB_IRQ_MSK);
	iowrite32(0, gsi->virt + reg_offset(reg));

	 
	if (gsi->version > IPA_VERSION_3_1) {
		reg = gsi_reg(gsi, INTER_EE_SRC_CH_IRQ_MSK);
		iowrite32(0, gsi->virt + reg_offset(reg));

		reg = gsi_reg(gsi, INTER_EE_SRC_EV_CH_IRQ_MSK);
		iowrite32(0, gsi->virt + reg_offset(reg));
	}

	reg = gsi_reg(gsi, CNTXT_GSI_IRQ_EN);
	iowrite32(0, gsi->virt + reg_offset(reg));

	ret = request_irq(gsi->irq, gsi_isr, 0, "gsi", gsi);
	if (ret)
		dev_err(gsi->dev, "error %d requesting \"gsi\" IRQ\n", ret);

	return ret;
}

static void gsi_irq_teardown(struct gsi *gsi)
{
	free_irq(gsi->irq, gsi);
}

 
static int gsi_ring_setup(struct gsi *gsi)
{
	struct device *dev = gsi->dev;
	const struct reg *reg;
	u32 count;
	u32 val;

	if (gsi->version < IPA_VERSION_3_5_1) {
		 
		gsi->channel_count = GSI_CHANNEL_COUNT_MAX;
		gsi->evt_ring_count = GSI_EVT_RING_COUNT_MAX;

		return 0;
	}

	reg = gsi_reg(gsi, HW_PARAM_2);
	val = ioread32(gsi->virt + reg_offset(reg));

	count = reg_decode(reg, NUM_CH_PER_EE, val);
	if (!count) {
		dev_err(dev, "GSI reports zero channels supported\n");
		return -EINVAL;
	}
	if (count > GSI_CHANNEL_COUNT_MAX) {
		dev_warn(dev, "limiting to %u channels; hardware supports %u\n",
			 GSI_CHANNEL_COUNT_MAX, count);
		count = GSI_CHANNEL_COUNT_MAX;
	}
	gsi->channel_count = count;

	if (gsi->version < IPA_VERSION_5_0) {
		count = reg_decode(reg, NUM_EV_PER_EE, val);
	} else {
		reg = gsi_reg(gsi, HW_PARAM_4);
		count = reg_decode(reg, EV_PER_EE, val);
	}
	if (!count) {
		dev_err(dev, "GSI reports zero event rings supported\n");
		return -EINVAL;
	}
	if (count > GSI_EVT_RING_COUNT_MAX) {
		dev_warn(dev,
			 "limiting to %u event rings; hardware supports %u\n",
			 GSI_EVT_RING_COUNT_MAX, count);
		count = GSI_EVT_RING_COUNT_MAX;
	}
	gsi->evt_ring_count = count;

	return 0;
}

 
int gsi_setup(struct gsi *gsi)
{
	const struct reg *reg;
	u32 val;
	int ret;

	 
	reg = gsi_reg(gsi, GSI_STATUS);
	val = ioread32(gsi->virt + reg_offset(reg));
	if (!(val & reg_bit(reg, ENABLED))) {
		dev_err(gsi->dev, "GSI has not been enabled\n");
		return -EIO;
	}

	ret = gsi_irq_setup(gsi);
	if (ret)
		return ret;

	ret = gsi_ring_setup(gsi);	 
	if (ret)
		goto err_irq_teardown;

	 
	reg = gsi_reg(gsi, ERROR_LOG);
	iowrite32(0, gsi->virt + reg_offset(reg));

	ret = gsi_channel_setup(gsi);
	if (ret)
		goto err_irq_teardown;

	return 0;

err_irq_teardown:
	gsi_irq_teardown(gsi);

	return ret;
}

 
void gsi_teardown(struct gsi *gsi)
{
	gsi_channel_teardown(gsi);
	gsi_irq_teardown(gsi);
}

 
static int gsi_channel_evt_ring_init(struct gsi_channel *channel)
{
	struct gsi *gsi = channel->gsi;
	struct gsi_evt_ring *evt_ring;
	int ret;

	ret = gsi_evt_ring_id_alloc(gsi);
	if (ret < 0)
		return ret;
	channel->evt_ring_id = ret;

	evt_ring = &gsi->evt_ring[channel->evt_ring_id];
	evt_ring->channel = channel;

	ret = gsi_ring_alloc(gsi, &evt_ring->ring, channel->event_count);
	if (!ret)
		return 0;	 

	dev_err(gsi->dev, "error %d allocating channel %u event ring\n",
		ret, gsi_channel_id(channel));

	gsi_evt_ring_id_free(gsi, channel->evt_ring_id);

	return ret;
}

 
static void gsi_channel_evt_ring_exit(struct gsi_channel *channel)
{
	u32 evt_ring_id = channel->evt_ring_id;
	struct gsi *gsi = channel->gsi;
	struct gsi_evt_ring *evt_ring;

	evt_ring = &gsi->evt_ring[evt_ring_id];
	gsi_ring_free(gsi, &evt_ring->ring);
	gsi_evt_ring_id_free(gsi, evt_ring_id);
}

static bool gsi_channel_data_valid(struct gsi *gsi, bool command,
				   const struct ipa_gsi_endpoint_data *data)
{
	const struct gsi_channel_data *channel_data;
	u32 channel_id = data->channel_id;
	struct device *dev = gsi->dev;

	 
	if (channel_id >= GSI_CHANNEL_COUNT_MAX) {
		dev_err(dev, "bad channel id %u; must be less than %u\n",
			channel_id, GSI_CHANNEL_COUNT_MAX);
		return false;
	}

	if (data->ee_id != GSI_EE_AP && data->ee_id != GSI_EE_MODEM) {
		dev_err(dev, "bad EE id %u; not AP or modem\n", data->ee_id);
		return false;
	}

	if (command && !data->toward_ipa) {
		dev_err(dev, "command channel %u is not TX\n", channel_id);
		return false;
	}

	channel_data = &data->channel;

	if (!channel_data->tlv_count ||
	    channel_data->tlv_count > GSI_TLV_MAX) {
		dev_err(dev, "channel %u bad tlv_count %u; must be 1..%u\n",
			channel_id, channel_data->tlv_count, GSI_TLV_MAX);
		return false;
	}

	if (command && IPA_COMMAND_TRANS_TRE_MAX > channel_data->tlv_count) {
		dev_err(dev, "command TRE max too big for channel %u (%u > %u)\n",
			channel_id, IPA_COMMAND_TRANS_TRE_MAX,
			channel_data->tlv_count);
		return false;
	}

	 
	if (channel_data->tre_count < 2 * channel_data->tlv_count - 1) {
		dev_err(dev, "channel %u TLV count %u exceeds TRE count %u\n",
			channel_id, channel_data->tlv_count,
			channel_data->tre_count);
		return false;
	}

	if (!is_power_of_2(channel_data->tre_count)) {
		dev_err(dev, "channel %u bad tre_count %u; not power of 2\n",
			channel_id, channel_data->tre_count);
		return false;
	}

	if (!is_power_of_2(channel_data->event_count)) {
		dev_err(dev, "channel %u bad event_count %u; not power of 2\n",
			channel_id, channel_data->event_count);
		return false;
	}

	return true;
}

 
static int gsi_channel_init_one(struct gsi *gsi,
				const struct ipa_gsi_endpoint_data *data,
				bool command)
{
	struct gsi_channel *channel;
	u32 tre_count;
	int ret;

	if (!gsi_channel_data_valid(gsi, command, data))
		return -EINVAL;

	 
	if (data->channel.tre_count > data->channel.event_count) {
		tre_count = data->channel.event_count;
		dev_warn(gsi->dev, "channel %u limited to %u TREs\n",
			 data->channel_id, tre_count);
	} else {
		tre_count = data->channel.tre_count;
	}

	channel = &gsi->channel[data->channel_id];
	memset(channel, 0, sizeof(*channel));

	channel->gsi = gsi;
	channel->toward_ipa = data->toward_ipa;
	channel->command = command;
	channel->trans_tre_max = data->channel.tlv_count;
	channel->tre_count = tre_count;
	channel->event_count = data->channel.event_count;

	ret = gsi_channel_evt_ring_init(channel);
	if (ret)
		goto err_clear_gsi;

	ret = gsi_ring_alloc(gsi, &channel->tre_ring, data->channel.tre_count);
	if (ret) {
		dev_err(gsi->dev, "error %d allocating channel %u ring\n",
			ret, data->channel_id);
		goto err_channel_evt_ring_exit;
	}

	ret = gsi_channel_trans_init(gsi, data->channel_id);
	if (ret)
		goto err_ring_free;

	if (command) {
		u32 tre_max = gsi_channel_tre_max(gsi, data->channel_id);

		ret = ipa_cmd_pool_init(channel, tre_max);
	}
	if (!ret)
		return 0;	 

	gsi_channel_trans_exit(channel);
err_ring_free:
	gsi_ring_free(gsi, &channel->tre_ring);
err_channel_evt_ring_exit:
	gsi_channel_evt_ring_exit(channel);
err_clear_gsi:
	channel->gsi = NULL;	 

	return ret;
}

 
static void gsi_channel_exit_one(struct gsi_channel *channel)
{
	if (!gsi_channel_initialized(channel))
		return;

	if (channel->command)
		ipa_cmd_pool_exit(channel);
	gsi_channel_trans_exit(channel);
	gsi_ring_free(channel->gsi, &channel->tre_ring);
	gsi_channel_evt_ring_exit(channel);
}

 
static int gsi_channel_init(struct gsi *gsi, u32 count,
			    const struct ipa_gsi_endpoint_data *data)
{
	bool modem_alloc;
	int ret = 0;
	u32 i;

	 
	modem_alloc = gsi->version == IPA_VERSION_4_2;

	gsi->event_bitmap = gsi_event_bitmap_init(GSI_EVT_RING_COUNT_MAX);
	gsi->ieob_enabled_bitmap = 0;

	 
	for (i = 0; i < count; i++) {
		bool command = i == IPA_ENDPOINT_AP_COMMAND_TX;

		if (ipa_gsi_endpoint_data_empty(&data[i]))
			continue;	 

		 
		if (data[i].ee_id == GSI_EE_MODEM) {
			if (modem_alloc)
				gsi->modem_channel_bitmap |=
						BIT(data[i].channel_id);
			continue;
		}

		ret = gsi_channel_init_one(gsi, &data[i], command);
		if (ret)
			goto err_unwind;
	}

	return ret;

err_unwind:
	while (i--) {
		if (ipa_gsi_endpoint_data_empty(&data[i]))
			continue;
		if (modem_alloc && data[i].ee_id == GSI_EE_MODEM) {
			gsi->modem_channel_bitmap &= ~BIT(data[i].channel_id);
			continue;
		}
		gsi_channel_exit_one(&gsi->channel[data->channel_id]);
	}

	return ret;
}

 
static void gsi_channel_exit(struct gsi *gsi)
{
	u32 channel_id = GSI_CHANNEL_COUNT_MAX - 1;

	do
		gsi_channel_exit_one(&gsi->channel[channel_id]);
	while (channel_id--);
	gsi->modem_channel_bitmap = 0;
}

 
int gsi_init(struct gsi *gsi, struct platform_device *pdev,
	     enum ipa_version version, u32 count,
	     const struct ipa_gsi_endpoint_data *data)
{
	int ret;

	gsi_validate_build();

	gsi->dev = &pdev->dev;
	gsi->version = version;

	 
	init_dummy_netdev(&gsi->dummy_dev);
	init_completion(&gsi->completion);

	ret = gsi_reg_init(gsi, pdev);
	if (ret)
		return ret;

	ret = gsi_irq_init(gsi, pdev);	 
	if (ret)
		goto err_reg_exit;

	ret = gsi_channel_init(gsi, count, data);
	if (ret)
		goto err_reg_exit;

	mutex_init(&gsi->mutex);

	return 0;

err_reg_exit:
	gsi_reg_exit(gsi);

	return ret;
}

 
void gsi_exit(struct gsi *gsi)
{
	mutex_destroy(&gsi->mutex);
	gsi_channel_exit(gsi);
	gsi_reg_exit(gsi);
}

 
u32 gsi_channel_tre_max(struct gsi *gsi, u32 channel_id)
{
	struct gsi_channel *channel = &gsi->channel[channel_id];

	 
	return channel->tre_count - (channel->trans_tre_max - 1);
}
