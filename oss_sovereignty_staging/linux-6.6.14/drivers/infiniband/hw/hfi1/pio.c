
 

#include <linux/delay.h>
#include "hfi.h"
#include "qp.h"
#include "trace.h"

#define SC(name) SEND_CTXT_##name
 
static void sc_wait_for_packet_egress(struct send_context *sc, int pause);

 
void __cm_reset(struct hfi1_devdata *dd, u64 sendctrl)
{
	write_csr(dd, SEND_CTRL, sendctrl | SEND_CTRL_CM_RESET_SMASK);
	while (1) {
		udelay(1);
		sendctrl = read_csr(dd, SEND_CTRL);
		if ((sendctrl & SEND_CTRL_CM_RESET_SMASK) == 0)
			break;
	}
}

 
void pio_send_control(struct hfi1_devdata *dd, int op)
{
	u64 reg, mask;
	unsigned long flags;
	int write = 1;	 
	int flush = 0;	 
	int i;

	spin_lock_irqsave(&dd->sendctrl_lock, flags);

	reg = read_csr(dd, SEND_CTRL);
	switch (op) {
	case PSC_GLOBAL_ENABLE:
		reg |= SEND_CTRL_SEND_ENABLE_SMASK;
		fallthrough;
	case PSC_DATA_VL_ENABLE:
		mask = 0;
		for (i = 0; i < ARRAY_SIZE(dd->vld); i++)
			if (!dd->vld[i].mtu)
				mask |= BIT_ULL(i);
		 
		mask = (mask & SEND_CTRL_UNSUPPORTED_VL_MASK) <<
			SEND_CTRL_UNSUPPORTED_VL_SHIFT;
		reg = (reg & ~SEND_CTRL_UNSUPPORTED_VL_SMASK) | mask;
		break;
	case PSC_GLOBAL_DISABLE:
		reg &= ~SEND_CTRL_SEND_ENABLE_SMASK;
		break;
	case PSC_GLOBAL_VLARB_ENABLE:
		reg |= SEND_CTRL_VL_ARBITER_ENABLE_SMASK;
		break;
	case PSC_GLOBAL_VLARB_DISABLE:
		reg &= ~SEND_CTRL_VL_ARBITER_ENABLE_SMASK;
		break;
	case PSC_CM_RESET:
		__cm_reset(dd, reg);
		write = 0;  
		break;
	case PSC_DATA_VL_DISABLE:
		reg |= SEND_CTRL_UNSUPPORTED_VL_SMASK;
		flush = 1;
		break;
	default:
		dd_dev_err(dd, "%s: invalid control %d\n", __func__, op);
		break;
	}

	if (write) {
		write_csr(dd, SEND_CTRL, reg);
		if (flush)
			(void)read_csr(dd, SEND_CTRL);  
	}

	spin_unlock_irqrestore(&dd->sendctrl_lock, flags);
}

 
#define NUM_SC_POOLS 2

 
#define SCS_POOL_0 -1
#define SCS_POOL_1 -2

 
#define SCC_PER_VL -1
#define SCC_PER_CPU  -2
#define SCC_PER_KRCVQ  -3

 
#define SCS_ACK_CREDITS  32
#define SCS_VL15_CREDITS 102	 

#define PIO_THRESHOLD_CEILING 4096

#define PIO_WAIT_BATCH_SIZE 5

 
static struct sc_config_sizes sc_config_sizes[SC_MAX] = {
	[SC_KERNEL] = { .size  = SCS_POOL_0,	 
			.count = SCC_PER_VL },	 
	[SC_ACK]    = { .size  = SCS_ACK_CREDITS,
			.count = SCC_PER_KRCVQ },
	[SC_USER]   = { .size  = SCS_POOL_0,	 
			.count = SCC_PER_CPU },	 
	[SC_VL15]   = { .size  = SCS_VL15_CREDITS,
			.count = 1 },

};

 
struct mem_pool_config {
	int centipercent;	 
	int absolute_blocks;	 
};

 
static struct mem_pool_config sc_mem_pool_config[NUM_SC_POOLS] = {
	 
	{  10000,     -1 },		 
	{      0,     -1 },		 
};

 
struct mem_pool_info {
	int centipercent;	 
	int count;		 
	int blocks;		 
	int size;		 
};

 
static int wildcard_to_pool(int wc)
{
	if (wc >= 0)
		return -1;	 
	return -wc - 1;
}

static const char *sc_type_names[SC_MAX] = {
	"kernel",
	"ack",
	"user",
	"vl15"
};

static const char *sc_type_name(int index)
{
	if (index < 0 || index >= SC_MAX)
		return "unknown";
	return sc_type_names[index];
}

 
int init_sc_pools_and_sizes(struct hfi1_devdata *dd)
{
	struct mem_pool_info mem_pool_info[NUM_SC_POOLS] = { { 0 } };
	int total_blocks = (chip_pio_mem_size(dd) / PIO_BLOCK_SIZE) - 1;
	int total_contexts = 0;
	int fixed_blocks;
	int pool_blocks;
	int used_blocks;
	int cp_total;		 
	int ab_total;		 
	int extra;
	int i;

	 
	if (HFI1_CAP_IS_KSET(SDMA)) {
		u16 max_pkt_size = (piothreshold < PIO_THRESHOLD_CEILING) ?
					 piothreshold : PIO_THRESHOLD_CEILING;
		sc_config_sizes[SC_KERNEL].size =
			3 * (max_pkt_size + 128) / PIO_BLOCK_SIZE;
	}

	 
	cp_total = 0;
	ab_total = 0;
	for (i = 0; i < NUM_SC_POOLS; i++) {
		int cp = sc_mem_pool_config[i].centipercent;
		int ab = sc_mem_pool_config[i].absolute_blocks;

		 
		if (cp >= 0) {			 
			cp_total += cp;
		} else if (ab >= 0) {		 
			ab_total += ab;
		} else {			 
			dd_dev_err(
				dd,
				"Send context memory pool %d: both the block count and centipercent are invalid\n",
				i);
			return -EINVAL;
		}

		mem_pool_info[i].centipercent = cp;
		mem_pool_info[i].blocks = ab;
	}

	 
	if (cp_total != 0 && ab_total != 0) {
		dd_dev_err(
			dd,
			"All send context memory pools must be described as either centipercent or blocks, no mixing between pools\n");
		return -EINVAL;
	}

	 
	if (cp_total != 0 && cp_total != 10000) {
		dd_dev_err(
			dd,
			"Send context memory pool centipercent is %d, expecting 10000\n",
			cp_total);
		return -EINVAL;
	}

	 
	if (ab_total > total_blocks) {
		dd_dev_err(
			dd,
			"Send context memory pool absolute block count %d is larger than the memory size %d\n",
			ab_total, total_blocks);
		return -EINVAL;
	}

	 
	fixed_blocks = 0;
	for (i = 0; i < SC_MAX; i++) {
		int count = sc_config_sizes[i].count;
		int size = sc_config_sizes[i].size;
		int pool;

		 
		if (i == SC_ACK) {
			count = dd->n_krcv_queues;
		} else if (i == SC_KERNEL) {
			count = INIT_SC_PER_VL * num_vls;
		} else if (count == SCC_PER_CPU) {
			count = dd->num_rcv_contexts - dd->n_krcv_queues;
		} else if (count < 0) {
			dd_dev_err(
				dd,
				"%s send context invalid count wildcard %d\n",
				sc_type_name(i), count);
			return -EINVAL;
		}
		if (total_contexts + count > chip_send_contexts(dd))
			count = chip_send_contexts(dd) - total_contexts;

		total_contexts += count;

		 
		pool = wildcard_to_pool(size);
		if (pool == -1) {			 
			fixed_blocks += size * count;
		} else if (pool < NUM_SC_POOLS) {	 
			mem_pool_info[pool].count += count;
		} else {				 
			dd_dev_err(
				dd,
				"%s send context invalid pool wildcard %d\n",
				sc_type_name(i), size);
			return -EINVAL;
		}

		dd->sc_sizes[i].count = count;
		dd->sc_sizes[i].size = size;
	}
	if (fixed_blocks > total_blocks) {
		dd_dev_err(
			dd,
			"Send context fixed block count, %u, larger than total block count %u\n",
			fixed_blocks, total_blocks);
		return -EINVAL;
	}

	 
	pool_blocks = total_blocks - fixed_blocks;
	if (ab_total > pool_blocks) {
		dd_dev_err(
			dd,
			"Send context fixed pool sizes, %u, larger than pool block count %u\n",
			ab_total, pool_blocks);
		return -EINVAL;
	}
	 
	pool_blocks -= ab_total;

	for (i = 0; i < NUM_SC_POOLS; i++) {
		struct mem_pool_info *pi = &mem_pool_info[i];

		 
		if (pi->centipercent >= 0)
			pi->blocks = (pool_blocks * pi->centipercent) / 10000;

		if (pi->blocks == 0 && pi->count != 0) {
			dd_dev_err(
				dd,
				"Send context memory pool %d has %u contexts, but no blocks\n",
				i, pi->count);
			return -EINVAL;
		}
		if (pi->count == 0) {
			 
			if (pi->blocks != 0)
				dd_dev_err(
					dd,
					"Send context memory pool %d has %u blocks, but zero contexts\n",
					i, pi->blocks);
			pi->size = 0;
		} else {
			pi->size = pi->blocks / pi->count;
		}
	}

	 
	used_blocks = 0;
	for (i = 0; i < SC_MAX; i++) {
		if (dd->sc_sizes[i].size < 0) {
			unsigned pool = wildcard_to_pool(dd->sc_sizes[i].size);

			WARN_ON_ONCE(pool >= NUM_SC_POOLS);
			dd->sc_sizes[i].size = mem_pool_info[pool].size;
		}
		 
#define PIO_MAX_BLOCKS 1024
		if (dd->sc_sizes[i].size > PIO_MAX_BLOCKS)
			dd->sc_sizes[i].size = PIO_MAX_BLOCKS;

		 
		used_blocks += dd->sc_sizes[i].size * dd->sc_sizes[i].count;
	}
	extra = total_blocks - used_blocks;
	if (extra != 0)
		dd_dev_info(dd, "unused send context blocks: %d\n", extra);

	return total_contexts;
}

int init_send_contexts(struct hfi1_devdata *dd)
{
	u16 base;
	int ret, i, j, context;

	ret = init_credit_return(dd);
	if (ret)
		return ret;

	dd->hw_to_sw = kmalloc_array(TXE_NUM_CONTEXTS, sizeof(u8),
					GFP_KERNEL);
	dd->send_contexts = kcalloc(dd->num_send_contexts,
				    sizeof(struct send_context_info),
				    GFP_KERNEL);
	if (!dd->send_contexts || !dd->hw_to_sw) {
		kfree(dd->hw_to_sw);
		kfree(dd->send_contexts);
		free_credit_return(dd);
		return -ENOMEM;
	}

	 
	for (i = 0; i < TXE_NUM_CONTEXTS; i++)
		dd->hw_to_sw[i] = INVALID_SCI;

	 
	context = 0;
	base = 1;
	for (i = 0; i < SC_MAX; i++) {
		struct sc_config_sizes *scs = &dd->sc_sizes[i];

		for (j = 0; j < scs->count; j++) {
			struct send_context_info *sci =
						&dd->send_contexts[context];
			sci->type = i;
			sci->base = base;
			sci->credits = scs->size;

			context++;
			base += scs->size;
		}
	}

	return 0;
}

 
static int sc_hw_alloc(struct hfi1_devdata *dd, int type, u32 *sw_index,
		       u32 *hw_context)
{
	struct send_context_info *sci;
	u32 index;
	u32 context;

	for (index = 0, sci = &dd->send_contexts[0];
			index < dd->num_send_contexts; index++, sci++) {
		if (sci->type == type && sci->allocated == 0) {
			sci->allocated = 1;
			 
			context = chip_send_contexts(dd) - index - 1;
			dd->hw_to_sw[context] = index;
			*sw_index = index;
			*hw_context = context;
			return 0;  
		}
	}
	dd_dev_err(dd, "Unable to locate a free type %d send context\n", type);
	return -ENOSPC;
}

 
static void sc_hw_free(struct hfi1_devdata *dd, u32 sw_index, u32 hw_context)
{
	struct send_context_info *sci;

	sci = &dd->send_contexts[sw_index];
	if (!sci->allocated) {
		dd_dev_err(dd, "%s: sw_index %u not allocated? hw_context %u\n",
			   __func__, sw_index, hw_context);
	}
	sci->allocated = 0;
	dd->hw_to_sw[hw_context] = INVALID_SCI;
}

 
static inline u32 group_context(u32 context, u32 group)
{
	return (context >> group) << group;
}

 
static inline u32 group_size(u32 group)
{
	return 1 << group;
}

 
static void cr_group_addresses(struct send_context *sc, dma_addr_t *dma)
{
	u32 gc = group_context(sc->hw_context, sc->group);
	u32 index = sc->hw_context & 0x7;

	sc->hw_free = &sc->dd->cr_base[sc->node].va[gc].cr[index];
	*dma = (unsigned long)
	       &((struct credit_return *)sc->dd->cr_base[sc->node].dma)[gc];
}

 
static void sc_halted(struct work_struct *work)
{
	struct send_context *sc;

	sc = container_of(work, struct send_context, halt_work);
	sc_restart(sc);
}

 
u32 sc_mtu_to_threshold(struct send_context *sc, u32 mtu, u32 hdrqentsize)
{
	u32 release_credits;
	u32 threshold;

	 
	mtu += hdrqentsize << 2;
	release_credits = DIV_ROUND_UP(mtu, PIO_BLOCK_SIZE);

	 
	if (sc->credits <= release_credits)
		threshold = 1;
	else
		threshold = sc->credits - release_credits;

	return threshold;
}

 
u32 sc_percent_to_threshold(struct send_context *sc, u32 percent)
{
	return (sc->credits * percent) / 100;
}

 
void sc_set_cr_threshold(struct send_context *sc, u32 new_threshold)
{
	unsigned long flags;
	u32 old_threshold;
	int force_return = 0;

	spin_lock_irqsave(&sc->credit_ctrl_lock, flags);

	old_threshold = (sc->credit_ctrl >>
				SC(CREDIT_CTRL_THRESHOLD_SHIFT))
			 & SC(CREDIT_CTRL_THRESHOLD_MASK);

	if (new_threshold != old_threshold) {
		sc->credit_ctrl =
			(sc->credit_ctrl
				& ~SC(CREDIT_CTRL_THRESHOLD_SMASK))
			| ((new_threshold
				& SC(CREDIT_CTRL_THRESHOLD_MASK))
			   << SC(CREDIT_CTRL_THRESHOLD_SHIFT));
		write_kctxt_csr(sc->dd, sc->hw_context,
				SC(CREDIT_CTRL), sc->credit_ctrl);

		 
		force_return = 1;
	}

	spin_unlock_irqrestore(&sc->credit_ctrl_lock, flags);

	if (force_return)
		sc_return_credits(sc);
}

 
void set_pio_integrity(struct send_context *sc)
{
	struct hfi1_devdata *dd = sc->dd;
	u32 hw_context = sc->hw_context;
	int type = sc->type;

	write_kctxt_csr(dd, hw_context,
			SC(CHECK_ENABLE),
			hfi1_pkt_default_send_ctxt_mask(dd, type));
}

static u32 get_buffers_allocated(struct send_context *sc)
{
	int cpu;
	u32 ret = 0;

	for_each_possible_cpu(cpu)
		ret += *per_cpu_ptr(sc->buffers_allocated, cpu);
	return ret;
}

static void reset_buffers_allocated(struct send_context *sc)
{
	int cpu;

	for_each_possible_cpu(cpu)
		(*per_cpu_ptr(sc->buffers_allocated, cpu)) = 0;
}

 
struct send_context *sc_alloc(struct hfi1_devdata *dd, int type,
			      uint hdrqentsize, int numa)
{
	struct send_context_info *sci;
	struct send_context *sc = NULL;
	dma_addr_t dma;
	unsigned long flags;
	u64 reg;
	u32 thresh;
	u32 sw_index;
	u32 hw_context;
	int ret;
	u8 opval, opmask;

	 
	if (dd->flags & HFI1_FROZEN)
		return NULL;

	sc = kzalloc_node(sizeof(*sc), GFP_KERNEL, numa);
	if (!sc)
		return NULL;

	sc->buffers_allocated = alloc_percpu(u32);
	if (!sc->buffers_allocated) {
		kfree(sc);
		dd_dev_err(dd,
			   "Cannot allocate buffers_allocated per cpu counters\n"
			  );
		return NULL;
	}

	spin_lock_irqsave(&dd->sc_lock, flags);
	ret = sc_hw_alloc(dd, type, &sw_index, &hw_context);
	if (ret) {
		spin_unlock_irqrestore(&dd->sc_lock, flags);
		free_percpu(sc->buffers_allocated);
		kfree(sc);
		return NULL;
	}

	sci = &dd->send_contexts[sw_index];
	sci->sc = sc;

	sc->dd = dd;
	sc->node = numa;
	sc->type = type;
	spin_lock_init(&sc->alloc_lock);
	spin_lock_init(&sc->release_lock);
	spin_lock_init(&sc->credit_ctrl_lock);
	seqlock_init(&sc->waitlock);
	INIT_LIST_HEAD(&sc->piowait);
	INIT_WORK(&sc->halt_work, sc_halted);
	init_waitqueue_head(&sc->halt_wait);

	 
	sc->group = 0;

	sc->sw_index = sw_index;
	sc->hw_context = hw_context;
	cr_group_addresses(sc, &dma);
	sc->credits = sci->credits;
	sc->size = sc->credits * PIO_BLOCK_SIZE;

 
#define PIO_ADDR_CONTEXT_MASK 0xfful
#define PIO_ADDR_CONTEXT_SHIFT 16
	sc->base_addr = dd->piobase + ((hw_context & PIO_ADDR_CONTEXT_MASK)
					<< PIO_ADDR_CONTEXT_SHIFT);

	 
	reg = ((sci->credits & SC(CTRL_CTXT_DEPTH_MASK))
					<< SC(CTRL_CTXT_DEPTH_SHIFT))
		| ((sci->base & SC(CTRL_CTXT_BASE_MASK))
					<< SC(CTRL_CTXT_BASE_SHIFT));
	write_kctxt_csr(dd, hw_context, SC(CTRL), reg);

	set_pio_integrity(sc);

	 
	write_kctxt_csr(dd, hw_context, SC(ERR_MASK), (u64)-1);

	 
	write_kctxt_csr(dd, hw_context, SC(CHECK_PARTITION_KEY),
			(SC(CHECK_PARTITION_KEY_VALUE_MASK) &
			 DEFAULT_PKEY) <<
			SC(CHECK_PARTITION_KEY_VALUE_SHIFT));

	 
	if (type == SC_USER) {
		opval = USER_OPCODE_CHECK_VAL;
		opmask = USER_OPCODE_CHECK_MASK;
	} else {
		opval = OPCODE_CHECK_VAL_DISABLED;
		opmask = OPCODE_CHECK_MASK_DISABLED;
	}

	 
	write_kctxt_csr(dd, hw_context, SC(CHECK_OPCODE),
			((u64)opmask << SC(CHECK_OPCODE_MASK_SHIFT)) |
			((u64)opval << SC(CHECK_OPCODE_VALUE_SHIFT)));

	 
	reg = dma & SC(CREDIT_RETURN_ADDR_ADDRESS_SMASK);
	write_kctxt_csr(dd, hw_context, SC(CREDIT_RETURN_ADDR), reg);

	 
	if (type == SC_ACK) {
		thresh = sc_percent_to_threshold(sc, 50);
	} else if (type == SC_USER) {
		thresh = sc_percent_to_threshold(sc,
						 user_credit_return_threshold);
	} else {  
		thresh = min(sc_percent_to_threshold(sc, 50),
			     sc_mtu_to_threshold(sc, hfi1_max_mtu,
						 hdrqentsize));
	}
	reg = thresh << SC(CREDIT_CTRL_THRESHOLD_SHIFT);
	 
	if (type == SC_USER && HFI1_CAP_IS_USET(EARLY_CREDIT_RETURN))
		reg |= SC(CREDIT_CTRL_EARLY_RETURN_SMASK);
	else if (HFI1_CAP_IS_KSET(EARLY_CREDIT_RETURN))  
		reg |= SC(CREDIT_CTRL_EARLY_RETURN_SMASK);

	 
	sc->credit_ctrl = reg;
	write_kctxt_csr(dd, hw_context, SC(CREDIT_CTRL), reg);

	 
	if (type == SC_USER) {
		reg = 1ULL << 15;
		write_kctxt_csr(dd, hw_context, SC(CHECK_VL), reg);
	}

	spin_unlock_irqrestore(&dd->sc_lock, flags);

	 
	if (type != SC_USER) {
		 
		sc->sr_size = sci->credits + 1;
		sc->sr = kcalloc_node(sc->sr_size,
				      sizeof(union pio_shadow_ring),
				      GFP_KERNEL, numa);
		if (!sc->sr) {
			sc_free(sc);
			return NULL;
		}
	}

	hfi1_cdbg(PIO,
		  "Send context %u(%u) %s group %u credits %u credit_ctrl 0x%llx threshold %u",
		  sw_index,
		  hw_context,
		  sc_type_name(type),
		  sc->group,
		  sc->credits,
		  sc->credit_ctrl,
		  thresh);

	return sc;
}

 
void sc_free(struct send_context *sc)
{
	struct hfi1_devdata *dd;
	unsigned long flags;
	u32 sw_index;
	u32 hw_context;

	if (!sc)
		return;

	sc->flags |= SCF_IN_FREE;	 
	dd = sc->dd;
	if (!list_empty(&sc->piowait))
		dd_dev_err(dd, "piowait list not empty!\n");
	sw_index = sc->sw_index;
	hw_context = sc->hw_context;
	sc_disable(sc);	 
	flush_work(&sc->halt_work);

	spin_lock_irqsave(&dd->sc_lock, flags);
	dd->send_contexts[sw_index].sc = NULL;

	 
	write_kctxt_csr(dd, hw_context, SC(CTRL), 0);
	write_kctxt_csr(dd, hw_context, SC(CHECK_ENABLE), 0);
	write_kctxt_csr(dd, hw_context, SC(ERR_MASK), 0);
	write_kctxt_csr(dd, hw_context, SC(CHECK_PARTITION_KEY), 0);
	write_kctxt_csr(dd, hw_context, SC(CHECK_OPCODE), 0);
	write_kctxt_csr(dd, hw_context, SC(CREDIT_RETURN_ADDR), 0);
	write_kctxt_csr(dd, hw_context, SC(CREDIT_CTRL), 0);

	 
	sc_hw_free(dd, sw_index, hw_context);
	spin_unlock_irqrestore(&dd->sc_lock, flags);

	kfree(sc->sr);
	free_percpu(sc->buffers_allocated);
	kfree(sc);
}

 
void sc_disable(struct send_context *sc)
{
	u64 reg;
	struct pio_buf *pbuf;
	LIST_HEAD(wake_list);

	if (!sc)
		return;

	 
	spin_lock_irq(&sc->alloc_lock);
	reg = read_kctxt_csr(sc->dd, sc->hw_context, SC(CTRL));
	reg &= ~SC(CTRL_CTXT_ENABLE_SMASK);
	sc->flags &= ~SCF_ENABLED;
	sc_wait_for_packet_egress(sc, 1);
	write_kctxt_csr(sc->dd, sc->hw_context, SC(CTRL), reg);

	 
	udelay(1);
	spin_lock(&sc->release_lock);
	if (sc->sr) {	 
		while (sc->sr_tail != sc->sr_head) {
			pbuf = &sc->sr[sc->sr_tail].pbuf;
			if (pbuf->cb)
				(*pbuf->cb)(pbuf->arg, PRC_SC_DISABLE);
			sc->sr_tail++;
			if (sc->sr_tail >= sc->sr_size)
				sc->sr_tail = 0;
		}
	}
	spin_unlock(&sc->release_lock);

	write_seqlock(&sc->waitlock);
	list_splice_init(&sc->piowait, &wake_list);
	write_sequnlock(&sc->waitlock);
	while (!list_empty(&wake_list)) {
		struct iowait *wait;
		struct rvt_qp *qp;
		struct hfi1_qp_priv *priv;

		wait = list_first_entry(&wake_list, struct iowait, list);
		qp = iowait_to_qp(wait);
		priv = qp->priv;
		list_del_init(&priv->s_iowait.list);
		priv->s_iowait.lock = NULL;
		hfi1_qp_wakeup(qp, RVT_S_WAIT_PIO | HFI1_S_WAIT_PIO_DRAIN);
	}

	spin_unlock_irq(&sc->alloc_lock);
}

 
static u64 packet_occupancy(u64 reg)
{
	return (reg &
		SEND_EGRESS_CTXT_STATUS_CTXT_EGRESS_PACKET_OCCUPANCY_SMASK)
		>> SEND_EGRESS_CTXT_STATUS_CTXT_EGRESS_PACKET_OCCUPANCY_SHIFT;
}

 
static bool egress_halted(u64 reg)
{
	return !!(reg & SEND_EGRESS_CTXT_STATUS_CTXT_EGRESS_HALT_STATUS_SMASK);
}

 
static bool is_sc_halted(struct hfi1_devdata *dd, u32 hw_context)
{
	return !!(read_kctxt_csr(dd, hw_context, SC(STATUS)) &
		  SC(STATUS_CTXT_HALTED_SMASK));
}

 
static void sc_wait_for_packet_egress(struct send_context *sc, int pause)
{
	struct hfi1_devdata *dd = sc->dd;
	u64 reg = 0;
	u64 reg_prev;
	u32 loop = 0;

	while (1) {
		reg_prev = reg;
		reg = read_csr(dd, sc->hw_context * 8 +
			       SEND_EGRESS_CTXT_STATUS);
		 
		if (sc->flags & SCF_HALTED ||
		    is_sc_halted(dd, sc->hw_context) || egress_halted(reg))
			break;
		reg = packet_occupancy(reg);
		if (reg == 0)
			break;
		 
		if (reg != reg_prev)
			loop = 0;
		if (loop > 50000) {
			 
			dd_dev_err(dd,
				   "%s: context %u(%u) timeout waiting for packets to egress, remaining count %u, bouncing link\n",
				   __func__, sc->sw_index,
				   sc->hw_context, (u32)reg);
			queue_work(dd->pport->link_wq,
				   &dd->pport->link_bounce_work);
			break;
		}
		loop++;
		udelay(1);
	}

	if (pause)
		 
		pause_for_credit_return(dd);
}

void sc_wait(struct hfi1_devdata *dd)
{
	int i;

	for (i = 0; i < dd->num_send_contexts; i++) {
		struct send_context *sc = dd->send_contexts[i].sc;

		if (!sc)
			continue;
		sc_wait_for_packet_egress(sc, 0);
	}
}

 
int sc_restart(struct send_context *sc)
{
	struct hfi1_devdata *dd = sc->dd;
	u64 reg;
	u32 loop;
	int count;

	 
	if (!(sc->flags & SCF_HALTED) || (sc->flags & SCF_IN_FREE))
		return -EINVAL;

	dd_dev_info(dd, "restarting send context %u(%u)\n", sc->sw_index,
		    sc->hw_context);

	 
	loop = 0;
	while (1) {
		reg = read_kctxt_csr(dd, sc->hw_context, SC(STATUS));
		if (reg & SC(STATUS_CTXT_HALTED_SMASK))
			break;
		if (loop > 100) {
			dd_dev_err(dd, "%s: context %u(%u) not halting, skipping\n",
				   __func__, sc->sw_index, sc->hw_context);
			return -ETIME;
		}
		loop++;
		udelay(1);
	}

	 
	if (sc->type != SC_USER) {
		 
		loop = 0;
		while (1) {
			count = get_buffers_allocated(sc);
			if (count == 0)
				break;
			if (loop > 100) {
				dd_dev_err(dd,
					   "%s: context %u(%u) timeout waiting for PIO buffers to zero, remaining %d\n",
					   __func__, sc->sw_index,
					   sc->hw_context, count);
			}
			loop++;
			udelay(1);
		}
	}

	 
	sc_disable(sc);

	 
	return sc_enable(sc);
}

 
void pio_freeze(struct hfi1_devdata *dd)
{
	struct send_context *sc;
	int i;

	for (i = 0; i < dd->num_send_contexts; i++) {
		sc = dd->send_contexts[i].sc;
		 
		if (!sc || !(sc->flags & SCF_FROZEN) || sc->type == SC_USER)
			continue;

		 
		sc_disable(sc);
	}
}

 
void pio_kernel_unfreeze(struct hfi1_devdata *dd)
{
	struct send_context *sc;
	int i;

	for (i = 0; i < dd->num_send_contexts; i++) {
		sc = dd->send_contexts[i].sc;
		if (!sc || !(sc->flags & SCF_FROZEN) || sc->type == SC_USER)
			continue;
		if (sc->flags & SCF_LINK_DOWN)
			continue;

		sc_enable(sc);	 
	}
}

 
void pio_kernel_linkup(struct hfi1_devdata *dd)
{
	struct send_context *sc;
	int i;

	for (i = 0; i < dd->num_send_contexts; i++) {
		sc = dd->send_contexts[i].sc;
		if (!sc || !(sc->flags & SCF_LINK_DOWN) || sc->type == SC_USER)
			continue;

		sc_enable(sc);	 
	}
}

 
static int pio_init_wait_progress(struct hfi1_devdata *dd)
{
	u64 reg;
	int max, count = 0;

	 
	max = (dd->icode == ICODE_FPGA_EMULATION) ? 120 : 5;
	while (1) {
		reg = read_csr(dd, SEND_PIO_INIT_CTXT);
		if (!(reg & SEND_PIO_INIT_CTXT_PIO_INIT_IN_PROGRESS_SMASK))
			break;
		if (count >= max)
			return -ETIMEDOUT;
		udelay(5);
		count++;
	}

	return reg & SEND_PIO_INIT_CTXT_PIO_INIT_ERR_SMASK ? -EIO : 0;
}

 
void pio_reset_all(struct hfi1_devdata *dd)
{
	int ret;

	 
	ret = pio_init_wait_progress(dd);
	 
	if (ret == -EIO) {
		 
		write_csr(dd, SEND_PIO_ERR_CLEAR,
			  SEND_PIO_ERR_CLEAR_PIO_INIT_SM_IN_ERR_SMASK);
	}

	 
	write_csr(dd, SEND_PIO_INIT_CTXT,
		  SEND_PIO_INIT_CTXT_PIO_ALL_CTXT_INIT_SMASK);
	udelay(2);
	ret = pio_init_wait_progress(dd);
	if (ret < 0) {
		dd_dev_err(dd,
			   "PIO send context init %s while initializing all PIO blocks\n",
			   ret == -ETIMEDOUT ? "is stuck" : "had an error");
	}
}

 
int sc_enable(struct send_context *sc)
{
	u64 sc_ctrl, reg, pio;
	struct hfi1_devdata *dd;
	unsigned long flags;
	int ret = 0;

	if (!sc)
		return -EINVAL;
	dd = sc->dd;

	 
	spin_lock_irqsave(&sc->alloc_lock, flags);
	sc_ctrl = read_kctxt_csr(dd, sc->hw_context, SC(CTRL));
	if ((sc_ctrl & SC(CTRL_CTXT_ENABLE_SMASK)))
		goto unlock;  

	 

	*sc->hw_free = 0;
	sc->free = 0;
	sc->alloc_free = 0;
	sc->fill = 0;
	sc->fill_wrap = 0;
	sc->sr_head = 0;
	sc->sr_tail = 0;
	sc->flags = 0;
	 
	reset_buffers_allocated(sc);

	 
	reg = read_kctxt_csr(dd, sc->hw_context, SC(ERR_STATUS));
	if (reg)
		write_kctxt_csr(dd, sc->hw_context, SC(ERR_CLEAR), reg);

	 
	spin_lock(&dd->sc_init_lock);
	 
	pio = ((sc->hw_context & SEND_PIO_INIT_CTXT_PIO_CTXT_NUM_MASK) <<
	       SEND_PIO_INIT_CTXT_PIO_CTXT_NUM_SHIFT) |
		SEND_PIO_INIT_CTXT_PIO_SINGLE_CTXT_INIT_SMASK;
	write_csr(dd, SEND_PIO_INIT_CTXT, pio);
	 
	udelay(2);
	ret = pio_init_wait_progress(dd);
	spin_unlock(&dd->sc_init_lock);
	if (ret) {
		dd_dev_err(dd,
			   "sctxt%u(%u): Context not enabled due to init failure %d\n",
			   sc->sw_index, sc->hw_context, ret);
		goto unlock;
	}

	 
	sc_ctrl |= SC(CTRL_CTXT_ENABLE_SMASK);
	write_kctxt_csr(dd, sc->hw_context, SC(CTRL), sc_ctrl);
	 
	read_kctxt_csr(dd, sc->hw_context, SC(CTRL));
	sc->flags |= SCF_ENABLED;

unlock:
	spin_unlock_irqrestore(&sc->alloc_lock, flags);

	return ret;
}

 
void sc_return_credits(struct send_context *sc)
{
	if (!sc)
		return;

	 
	write_kctxt_csr(sc->dd, sc->hw_context, SC(CREDIT_FORCE),
			SC(CREDIT_FORCE_FORCE_RETURN_SMASK));
	 
	read_kctxt_csr(sc->dd, sc->hw_context, SC(CREDIT_FORCE));
	 
	write_kctxt_csr(sc->dd, sc->hw_context, SC(CREDIT_FORCE), 0);
}

 
void sc_flush(struct send_context *sc)
{
	if (!sc)
		return;

	sc_wait_for_packet_egress(sc, 1);
}

 
void sc_drop(struct send_context *sc)
{
	if (!sc)
		return;

	dd_dev_info(sc->dd, "%s: context %u(%u) - not implemented\n",
		    __func__, sc->sw_index, sc->hw_context);
}

 
void sc_stop(struct send_context *sc, int flag)
{
	unsigned long flags;

	 
	spin_lock_irqsave(&sc->alloc_lock, flags);
	 
	sc->flags |= flag;
	sc->flags &= ~SCF_ENABLED;
	spin_unlock_irqrestore(&sc->alloc_lock, flags);
	wake_up(&sc->halt_wait);
}

#define BLOCK_DWORDS (PIO_BLOCK_SIZE / sizeof(u32))
#define dwords_to_blocks(x) DIV_ROUND_UP(x, BLOCK_DWORDS)

 
struct pio_buf *sc_buffer_alloc(struct send_context *sc, u32 dw_len,
				pio_release_cb cb, void *arg)
{
	struct pio_buf *pbuf = NULL;
	unsigned long flags;
	unsigned long avail;
	unsigned long blocks = dwords_to_blocks(dw_len);
	u32 fill_wrap;
	int trycount = 0;
	u32 head, next;

	spin_lock_irqsave(&sc->alloc_lock, flags);
	if (!(sc->flags & SCF_ENABLED)) {
		spin_unlock_irqrestore(&sc->alloc_lock, flags);
		return ERR_PTR(-ECOMM);
	}

retry:
	avail = (unsigned long)sc->credits - (sc->fill - sc->alloc_free);
	if (blocks > avail) {
		 
		if (unlikely(trycount))	{  
			spin_unlock_irqrestore(&sc->alloc_lock, flags);
			goto done;
		}
		 
		sc->alloc_free = READ_ONCE(sc->free);
		avail =
			(unsigned long)sc->credits -
			(sc->fill - sc->alloc_free);
		if (blocks > avail) {
			 
			sc_release_update(sc);
			sc->alloc_free = READ_ONCE(sc->free);
			trycount++;
			goto retry;
		}
	}

	 

	preempt_disable();
	this_cpu_inc(*sc->buffers_allocated);

	 
	head = sc->sr_head;

	 
	sc->fill += blocks;
	fill_wrap = sc->fill_wrap;
	sc->fill_wrap += blocks;
	if (sc->fill_wrap >= sc->credits)
		sc->fill_wrap = sc->fill_wrap - sc->credits;

	 
	pbuf = &sc->sr[head].pbuf;
	pbuf->sent_at = sc->fill;
	pbuf->cb = cb;
	pbuf->arg = arg;
	pbuf->sc = sc;	 
	 

	 
	next = head + 1;
	if (next >= sc->sr_size)
		next = 0;
	 
	smp_wmb();
	sc->sr_head = next;
	spin_unlock_irqrestore(&sc->alloc_lock, flags);

	 
	pbuf->start = sc->base_addr + fill_wrap * PIO_BLOCK_SIZE;
	pbuf->end = sc->base_addr + sc->size;
	pbuf->qw_written = 0;
	pbuf->carry_bytes = 0;
	pbuf->carry.val64 = 0;
done:
	return pbuf;
}

 

 
void sc_add_credit_return_intr(struct send_context *sc)
{
	unsigned long flags;

	 
	spin_lock_irqsave(&sc->credit_ctrl_lock, flags);
	if (sc->credit_intr_count == 0) {
		sc->credit_ctrl |= SC(CREDIT_CTRL_CREDIT_INTR_SMASK);
		write_kctxt_csr(sc->dd, sc->hw_context,
				SC(CREDIT_CTRL), sc->credit_ctrl);
	}
	sc->credit_intr_count++;
	spin_unlock_irqrestore(&sc->credit_ctrl_lock, flags);
}

 
void sc_del_credit_return_intr(struct send_context *sc)
{
	unsigned long flags;

	WARN_ON(sc->credit_intr_count == 0);

	 
	spin_lock_irqsave(&sc->credit_ctrl_lock, flags);
	sc->credit_intr_count--;
	if (sc->credit_intr_count == 0) {
		sc->credit_ctrl &= ~SC(CREDIT_CTRL_CREDIT_INTR_SMASK);
		write_kctxt_csr(sc->dd, sc->hw_context,
				SC(CREDIT_CTRL), sc->credit_ctrl);
	}
	spin_unlock_irqrestore(&sc->credit_ctrl_lock, flags);
}

 
void hfi1_sc_wantpiobuf_intr(struct send_context *sc, u32 needint)
{
	if (needint)
		sc_add_credit_return_intr(sc);
	else
		sc_del_credit_return_intr(sc);
	trace_hfi1_wantpiointr(sc, needint, sc->credit_ctrl);
	if (needint)
		sc_return_credits(sc);
}

 
static void sc_piobufavail(struct send_context *sc)
{
	struct hfi1_devdata *dd = sc->dd;
	struct list_head *list;
	struct rvt_qp *qps[PIO_WAIT_BATCH_SIZE];
	struct rvt_qp *qp;
	struct hfi1_qp_priv *priv;
	unsigned long flags;
	uint i, n = 0, top_idx = 0;

	if (dd->send_contexts[sc->sw_index].type != SC_KERNEL &&
	    dd->send_contexts[sc->sw_index].type != SC_VL15)
		return;
	list = &sc->piowait;
	 
	write_seqlock_irqsave(&sc->waitlock, flags);
	while (!list_empty(list)) {
		struct iowait *wait;

		if (n == ARRAY_SIZE(qps))
			break;
		wait = list_first_entry(list, struct iowait, list);
		iowait_get_priority(wait);
		qp = iowait_to_qp(wait);
		priv = qp->priv;
		list_del_init(&priv->s_iowait.list);
		priv->s_iowait.lock = NULL;
		if (n) {
			priv = qps[top_idx]->priv;
			top_idx = iowait_priority_update_top(wait,
							     &priv->s_iowait,
							     n, top_idx);
		}

		 
		qps[n++] = qp;
	}
	 
	if (n) {
		hfi1_sc_wantpiobuf_intr(sc, 0);
		if (!list_empty(list))
			hfi1_sc_wantpiobuf_intr(sc, 1);
	}
	write_sequnlock_irqrestore(&sc->waitlock, flags);

	 
	if (n)
		hfi1_qp_wakeup(qps[top_idx],
			       RVT_S_WAIT_PIO | HFI1_S_WAIT_PIO_DRAIN);
	for (i = 0; i < n; i++)
		if (i != top_idx)
			hfi1_qp_wakeup(qps[i],
				       RVT_S_WAIT_PIO | HFI1_S_WAIT_PIO_DRAIN);
}

 
static inline int fill_code(u64 hw_free)
{
	int code = 0;

	if (hw_free & CR_STATUS_SMASK)
		code |= PRC_STATUS_ERR;
	if (hw_free & CR_CREDIT_RETURN_DUE_TO_PBC_SMASK)
		code |= PRC_PBC;
	if (hw_free & CR_CREDIT_RETURN_DUE_TO_THRESHOLD_SMASK)
		code |= PRC_THRESHOLD;
	if (hw_free & CR_CREDIT_RETURN_DUE_TO_ERR_SMASK)
		code |= PRC_FILL_ERR;
	if (hw_free & CR_CREDIT_RETURN_DUE_TO_FORCE_SMASK)
		code |= PRC_SC_DISABLE;
	return code;
}

 
#define sent_before(a, b) time_before(a, b)	 

 
void sc_release_update(struct send_context *sc)
{
	struct pio_buf *pbuf;
	u64 hw_free;
	u32 head, tail;
	unsigned long old_free;
	unsigned long free;
	unsigned long extra;
	unsigned long flags;
	int code;

	if (!sc)
		return;

	spin_lock_irqsave(&sc->release_lock, flags);
	 
	hw_free = le64_to_cpu(*sc->hw_free);		 
	old_free = sc->free;
	extra = (((hw_free & CR_COUNTER_SMASK) >> CR_COUNTER_SHIFT)
			- (old_free & CR_COUNTER_MASK))
				& CR_COUNTER_MASK;
	free = old_free + extra;
	trace_hfi1_piofree(sc, extra);

	 
	code = -1;				 
	head = READ_ONCE(sc->sr_head);	 
	tail = sc->sr_tail;
	while (head != tail) {
		pbuf = &sc->sr[tail].pbuf;

		if (sent_before(free, pbuf->sent_at)) {
			 
			break;
		}
		if (pbuf->cb) {
			if (code < 0)  
				code = fill_code(hw_free);
			(*pbuf->cb)(pbuf->arg, code);
		}

		tail++;
		if (tail >= sc->sr_size)
			tail = 0;
	}
	sc->sr_tail = tail;
	 
	smp_wmb();
	sc->free = free;
	spin_unlock_irqrestore(&sc->release_lock, flags);
	sc_piobufavail(sc);
}

 
void sc_group_release_update(struct hfi1_devdata *dd, u32 hw_context)
{
	struct send_context *sc;
	u32 sw_index;
	u32 gc, gc_end;

	spin_lock(&dd->sc_lock);
	sw_index = dd->hw_to_sw[hw_context];
	if (unlikely(sw_index >= dd->num_send_contexts)) {
		dd_dev_err(dd, "%s: invalid hw (%u) to sw (%u) mapping\n",
			   __func__, hw_context, sw_index);
		goto done;
	}
	sc = dd->send_contexts[sw_index].sc;
	if (unlikely(!sc))
		goto done;

	gc = group_context(hw_context, sc->group);
	gc_end = gc + group_size(sc->group);
	for (; gc < gc_end; gc++) {
		sw_index = dd->hw_to_sw[gc];
		if (unlikely(sw_index >= dd->num_send_contexts)) {
			dd_dev_err(dd,
				   "%s: invalid hw (%u) to sw (%u) mapping\n",
				   __func__, hw_context, sw_index);
			continue;
		}
		sc_release_update(dd->send_contexts[sw_index].sc);
	}
done:
	spin_unlock(&dd->sc_lock);
}

 
struct send_context *pio_select_send_context_vl(struct hfi1_devdata *dd,
						u32 selector, u8 vl)
{
	struct pio_vl_map *m;
	struct pio_map_elem *e;
	struct send_context *rval;

	 
	if (unlikely(vl >= num_vls)) {
		rval = NULL;
		goto done;
	}

	rcu_read_lock();
	m = rcu_dereference(dd->pio_map);
	if (unlikely(!m)) {
		rcu_read_unlock();
		return dd->vld[0].sc;
	}
	e = m->map[vl & m->mask];
	rval = e->ksc[selector & e->mask];
	rcu_read_unlock();

done:
	rval = !rval ? dd->vld[0].sc : rval;
	return rval;
}

 
struct send_context *pio_select_send_context_sc(struct hfi1_devdata *dd,
						u32 selector, u8 sc5)
{
	u8 vl = sc_to_vlt(dd, sc5);

	return pio_select_send_context_vl(dd, selector, vl);
}

 
static void pio_map_free(struct pio_vl_map *m)
{
	int i;

	for (i = 0; m && i < m->actual_vls; i++)
		kfree(m->map[i]);
	kfree(m);
}

 
static void pio_map_rcu_callback(struct rcu_head *list)
{
	struct pio_vl_map *m = container_of(list, struct pio_vl_map, list);

	pio_map_free(m);
}

 
static void set_threshold(struct hfi1_devdata *dd, int scontext, int i)
{
	u32 thres;

	thres = min(sc_percent_to_threshold(dd->kernel_send_context[scontext],
					    50),
		    sc_mtu_to_threshold(dd->kernel_send_context[scontext],
					dd->vld[i].mtu,
					dd->rcd[0]->rcvhdrqentsize));
	sc_set_cr_threshold(dd->kernel_send_context[scontext], thres);
}

 
int pio_map_init(struct hfi1_devdata *dd, u8 port, u8 num_vls, u8 *vl_scontexts)
{
	int i, j;
	int extra, sc_per_vl;
	int scontext = 1;
	int num_kernel_send_contexts = 0;
	u8 lvl_scontexts[OPA_MAX_VLS];
	struct pio_vl_map *oldmap, *newmap;

	if (!vl_scontexts) {
		for (i = 0; i < dd->num_send_contexts; i++)
			if (dd->send_contexts[i].type == SC_KERNEL)
				num_kernel_send_contexts++;
		 
		sc_per_vl = num_kernel_send_contexts / num_vls;
		 
		extra = num_kernel_send_contexts % num_vls;
		vl_scontexts = lvl_scontexts;
		 
		for (i = num_vls - 1; i >= 0; i--, extra--)
			vl_scontexts[i] = sc_per_vl + (extra > 0 ? 1 : 0);
	}
	 
	newmap = kzalloc(struct_size(newmap, map, roundup_pow_of_two(num_vls)),
			 GFP_KERNEL);
	if (!newmap)
		goto bail;
	newmap->actual_vls = num_vls;
	newmap->vls = roundup_pow_of_two(num_vls);
	newmap->mask = (1 << ilog2(newmap->vls)) - 1;
	for (i = 0; i < newmap->vls; i++) {
		 
		int first_scontext = scontext;

		if (i < newmap->actual_vls) {
			int sz = roundup_pow_of_two(vl_scontexts[i]);

			 
			newmap->map[i] = kzalloc(struct_size(newmap->map[i],
							     ksc, sz),
						 GFP_KERNEL);
			if (!newmap->map[i])
				goto bail;
			newmap->map[i]->mask = (1 << ilog2(sz)) - 1;
			 
			for (j = 0; j < sz; j++) {
				if (dd->kernel_send_context[scontext]) {
					newmap->map[i]->ksc[j] =
					dd->kernel_send_context[scontext];
					set_threshold(dd, scontext, i);
				}
				if (++scontext >= first_scontext +
						  vl_scontexts[i])
					 
					scontext = first_scontext;
			}
		} else {
			 
			newmap->map[i] = newmap->map[i % num_vls];
		}
		scontext = first_scontext + vl_scontexts[i];
	}
	 
	spin_lock_irq(&dd->pio_map_lock);
	oldmap = rcu_dereference_protected(dd->pio_map,
					   lockdep_is_held(&dd->pio_map_lock));

	 
	rcu_assign_pointer(dd->pio_map, newmap);

	spin_unlock_irq(&dd->pio_map_lock);
	 
	if (oldmap)
		call_rcu(&oldmap->list, pio_map_rcu_callback);
	return 0;
bail:
	 
	pio_map_free(newmap);
	return -ENOMEM;
}

void free_pio_map(struct hfi1_devdata *dd)
{
	 
	if (rcu_access_pointer(dd->pio_map)) {
		spin_lock_irq(&dd->pio_map_lock);
		pio_map_free(rcu_access_pointer(dd->pio_map));
		RCU_INIT_POINTER(dd->pio_map, NULL);
		spin_unlock_irq(&dd->pio_map_lock);
		synchronize_rcu();
	}
	kfree(dd->kernel_send_context);
	dd->kernel_send_context = NULL;
}

int init_pervl_scs(struct hfi1_devdata *dd)
{
	int i;
	u64 mask, all_vl_mask = (u64)0x80ff;  
	u64 data_vls_mask = (u64)0x00ff;  
	u32 ctxt;
	struct hfi1_pportdata *ppd = dd->pport;

	dd->vld[15].sc = sc_alloc(dd, SC_VL15,
				  dd->rcd[0]->rcvhdrqentsize, dd->node);
	if (!dd->vld[15].sc)
		return -ENOMEM;

	hfi1_init_ctxt(dd->vld[15].sc);
	dd->vld[15].mtu = enum_to_mtu(OPA_MTU_2048);

	dd->kernel_send_context = kcalloc_node(dd->num_send_contexts,
					       sizeof(struct send_context *),
					       GFP_KERNEL, dd->node);
	if (!dd->kernel_send_context)
		goto freesc15;

	dd->kernel_send_context[0] = dd->vld[15].sc;

	for (i = 0; i < num_vls; i++) {
		 
		dd->vld[i].sc = sc_alloc(dd, SC_KERNEL,
					 dd->rcd[0]->rcvhdrqentsize, dd->node);
		if (!dd->vld[i].sc)
			goto nomem;
		dd->kernel_send_context[i + 1] = dd->vld[i].sc;
		hfi1_init_ctxt(dd->vld[i].sc);
		 
		dd->vld[i].mtu = hfi1_max_mtu;
	}
	for (i = num_vls; i < INIT_SC_PER_VL * num_vls; i++) {
		dd->kernel_send_context[i + 1] =
		sc_alloc(dd, SC_KERNEL, dd->rcd[0]->rcvhdrqentsize, dd->node);
		if (!dd->kernel_send_context[i + 1])
			goto nomem;
		hfi1_init_ctxt(dd->kernel_send_context[i + 1]);
	}

	sc_enable(dd->vld[15].sc);
	ctxt = dd->vld[15].sc->hw_context;
	mask = all_vl_mask & ~(1LL << 15);
	write_kctxt_csr(dd, ctxt, SC(CHECK_VL), mask);
	dd_dev_info(dd,
		    "Using send context %u(%u) for VL15\n",
		    dd->vld[15].sc->sw_index, ctxt);

	for (i = 0; i < num_vls; i++) {
		sc_enable(dd->vld[i].sc);
		ctxt = dd->vld[i].sc->hw_context;
		mask = all_vl_mask & ~(data_vls_mask);
		write_kctxt_csr(dd, ctxt, SC(CHECK_VL), mask);
	}
	for (i = num_vls; i < INIT_SC_PER_VL * num_vls; i++) {
		sc_enable(dd->kernel_send_context[i + 1]);
		ctxt = dd->kernel_send_context[i + 1]->hw_context;
		mask = all_vl_mask & ~(data_vls_mask);
		write_kctxt_csr(dd, ctxt, SC(CHECK_VL), mask);
	}

	if (pio_map_init(dd, ppd->port - 1, num_vls, NULL))
		goto nomem;
	return 0;

nomem:
	for (i = 0; i < num_vls; i++) {
		sc_free(dd->vld[i].sc);
		dd->vld[i].sc = NULL;
	}

	for (i = num_vls; i < INIT_SC_PER_VL * num_vls; i++)
		sc_free(dd->kernel_send_context[i + 1]);

	kfree(dd->kernel_send_context);
	dd->kernel_send_context = NULL;

freesc15:
	sc_free(dd->vld[15].sc);
	return -ENOMEM;
}

int init_credit_return(struct hfi1_devdata *dd)
{
	int ret;
	int i;

	dd->cr_base = kcalloc(
		node_affinity.num_possible_nodes,
		sizeof(struct credit_return_base),
		GFP_KERNEL);
	if (!dd->cr_base) {
		ret = -ENOMEM;
		goto done;
	}
	for_each_node_with_cpus(i) {
		int bytes = TXE_NUM_CONTEXTS * sizeof(struct credit_return);

		set_dev_node(&dd->pcidev->dev, i);
		dd->cr_base[i].va = dma_alloc_coherent(&dd->pcidev->dev,
						       bytes,
						       &dd->cr_base[i].dma,
						       GFP_KERNEL);
		if (!dd->cr_base[i].va) {
			set_dev_node(&dd->pcidev->dev, dd->node);
			dd_dev_err(dd,
				   "Unable to allocate credit return DMA range for NUMA %d\n",
				   i);
			ret = -ENOMEM;
			goto done;
		}
	}
	set_dev_node(&dd->pcidev->dev, dd->node);

	ret = 0;
done:
	return ret;
}

void free_credit_return(struct hfi1_devdata *dd)
{
	int i;

	if (!dd->cr_base)
		return;
	for (i = 0; i < node_affinity.num_possible_nodes; i++) {
		if (dd->cr_base[i].va) {
			dma_free_coherent(&dd->pcidev->dev,
					  TXE_NUM_CONTEXTS *
					  sizeof(struct credit_return),
					  dd->cr_base[i].va,
					  dd->cr_base[i].dma);
		}
	}
	kfree(dd->cr_base);
	dd->cr_base = NULL;
}

void seqfile_dump_sci(struct seq_file *s, u32 i,
		      struct send_context_info *sci)
{
	struct send_context *sc = sci->sc;
	u64 reg;

	seq_printf(s, "SCI %u: type %u base %u credits %u\n",
		   i, sci->type, sci->base, sci->credits);
	seq_printf(s, "  flags 0x%x sw_inx %u hw_ctxt %u grp %u\n",
		   sc->flags,  sc->sw_index, sc->hw_context, sc->group);
	seq_printf(s, "  sr_size %u credits %u sr_head %u sr_tail %u\n",
		   sc->sr_size, sc->credits, sc->sr_head, sc->sr_tail);
	seq_printf(s, "  fill %lu free %lu fill_wrap %u alloc_free %lu\n",
		   sc->fill, sc->free, sc->fill_wrap, sc->alloc_free);
	seq_printf(s, "  credit_intr_count %u credit_ctrl 0x%llx\n",
		   sc->credit_intr_count, sc->credit_ctrl);
	reg = read_kctxt_csr(sc->dd, sc->hw_context, SC(CREDIT_STATUS));
	seq_printf(s, "  *hw_free %llu CurrentFree %llu LastReturned %llu\n",
		   (le64_to_cpu(*sc->hw_free) & CR_COUNTER_SMASK) >>
		    CR_COUNTER_SHIFT,
		   (reg >> SC(CREDIT_STATUS_CURRENT_FREE_COUNTER_SHIFT)) &
		    SC(CREDIT_STATUS_CURRENT_FREE_COUNTER_MASK),
		   reg & SC(CREDIT_STATUS_LAST_RETURNED_COUNTER_SMASK));
}
