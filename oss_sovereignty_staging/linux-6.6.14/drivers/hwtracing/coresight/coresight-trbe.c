
 
#define DRVNAME "arm_trbe"

#define pr_fmt(fmt) DRVNAME ": " fmt

#include <asm/barrier.h>
#include <asm/cpufeature.h>

#include "coresight-self-hosted-trace.h"
#include "coresight-trbe.h"

#define PERF_IDX2OFF(idx, buf) ((idx) % ((buf)->nr_pages << PAGE_SHIFT))

 
#define ETE_IGNORE_PACKET		0x70

 
#define TRBE_TRACE_MIN_BUF_SIZE		64

enum trbe_fault_action {
	TRBE_FAULT_ACT_WRAP,
	TRBE_FAULT_ACT_SPURIOUS,
	TRBE_FAULT_ACT_FATAL,
};

struct trbe_buf {
	 
	unsigned long trbe_base;
	 
	unsigned long trbe_hw_base;
	unsigned long trbe_limit;
	unsigned long trbe_write;
	int nr_pages;
	void **pages;
	bool snapshot;
	struct trbe_cpudata *cpudata;
};

 
#define TRBE_WORKAROUND_OVERWRITE_FILL_MODE	0
#define TRBE_WORKAROUND_WRITE_OUT_OF_RANGE	1
#define TRBE_NEEDS_DRAIN_AFTER_DISABLE		2
#define TRBE_NEEDS_CTXT_SYNC_AFTER_ENABLE	3
#define TRBE_IS_BROKEN				4

static int trbe_errata_cpucaps[] = {
	[TRBE_WORKAROUND_OVERWRITE_FILL_MODE] = ARM64_WORKAROUND_TRBE_OVERWRITE_FILL_MODE,
	[TRBE_WORKAROUND_WRITE_OUT_OF_RANGE] = ARM64_WORKAROUND_TRBE_WRITE_OUT_OF_RANGE,
	[TRBE_NEEDS_DRAIN_AFTER_DISABLE] = ARM64_WORKAROUND_2064142,
	[TRBE_NEEDS_CTXT_SYNC_AFTER_ENABLE] = ARM64_WORKAROUND_2038923,
	[TRBE_IS_BROKEN] = ARM64_WORKAROUND_1902691,
	-1,		 
};

 
#define TRBE_ERRATA_MAX			(ARRAY_SIZE(trbe_errata_cpucaps) - 1)

 
#define TRBE_WORKAROUND_OVERWRITE_FILL_MODE_SKIP_BYTES	256

 
struct trbe_cpudata {
	bool trbe_flag;
	u64 trbe_hw_align;
	u64 trbe_align;
	int cpu;
	enum cs_mode mode;
	struct trbe_buf *buf;
	struct trbe_drvdata *drvdata;
	DECLARE_BITMAP(errata, TRBE_ERRATA_MAX);
};

struct trbe_drvdata {
	struct trbe_cpudata __percpu *cpudata;
	struct perf_output_handle * __percpu *handle;
	struct hlist_node hotplug_node;
	int irq;
	cpumask_t supported_cpus;
	enum cpuhp_state trbe_online;
	struct platform_device *pdev;
};

static void trbe_check_errata(struct trbe_cpudata *cpudata)
{
	int i;

	for (i = 0; i < TRBE_ERRATA_MAX; i++) {
		int cap = trbe_errata_cpucaps[i];

		if (WARN_ON_ONCE(cap < 0))
			return;
		if (this_cpu_has_cap(cap))
			set_bit(i, cpudata->errata);
	}
}

static inline bool trbe_has_erratum(struct trbe_cpudata *cpudata, int i)
{
	return (i < TRBE_ERRATA_MAX) && test_bit(i, cpudata->errata);
}

static inline bool trbe_may_overwrite_in_fill_mode(struct trbe_cpudata *cpudata)
{
	return trbe_has_erratum(cpudata, TRBE_WORKAROUND_OVERWRITE_FILL_MODE);
}

static inline bool trbe_may_write_out_of_range(struct trbe_cpudata *cpudata)
{
	return trbe_has_erratum(cpudata, TRBE_WORKAROUND_WRITE_OUT_OF_RANGE);
}

static inline bool trbe_needs_drain_after_disable(struct trbe_cpudata *cpudata)
{
	 
	return trbe_has_erratum(cpudata, TRBE_NEEDS_DRAIN_AFTER_DISABLE);
}

static inline bool trbe_needs_ctxt_sync_after_enable(struct trbe_cpudata *cpudata)
{
	 
	return trbe_has_erratum(cpudata, TRBE_NEEDS_CTXT_SYNC_AFTER_ENABLE);
}

static inline bool trbe_is_broken(struct trbe_cpudata *cpudata)
{
	return trbe_has_erratum(cpudata, TRBE_IS_BROKEN);
}

static int trbe_alloc_node(struct perf_event *event)
{
	if (event->cpu == -1)
		return NUMA_NO_NODE;
	return cpu_to_node(event->cpu);
}

static inline void trbe_drain_buffer(void)
{
	tsb_csync();
	dsb(nsh);
}

static inline void set_trbe_enabled(struct trbe_cpudata *cpudata, u64 trblimitr)
{
	 
	trblimitr |= TRBLIMITR_EL1_E;
	write_sysreg_s(trblimitr, SYS_TRBLIMITR_EL1);

	 
	isb();

	if (trbe_needs_ctxt_sync_after_enable(cpudata))
		isb();
}

static inline void set_trbe_disabled(struct trbe_cpudata *cpudata)
{
	u64 trblimitr = read_sysreg_s(SYS_TRBLIMITR_EL1);

	 
	trblimitr &= ~TRBLIMITR_EL1_E;
	write_sysreg_s(trblimitr, SYS_TRBLIMITR_EL1);

	if (trbe_needs_drain_after_disable(cpudata))
		trbe_drain_buffer();
	isb();
}

static void trbe_drain_and_disable_local(struct trbe_cpudata *cpudata)
{
	trbe_drain_buffer();
	set_trbe_disabled(cpudata);
}

static void trbe_reset_local(struct trbe_cpudata *cpudata)
{
	trbe_drain_and_disable_local(cpudata);
	write_sysreg_s(0, SYS_TRBLIMITR_EL1);
	write_sysreg_s(0, SYS_TRBPTR_EL1);
	write_sysreg_s(0, SYS_TRBBASER_EL1);
	write_sysreg_s(0, SYS_TRBSR_EL1);
}

static void trbe_report_wrap_event(struct perf_output_handle *handle)
{
	 
	perf_aux_output_flag(handle, PERF_AUX_FLAG_COLLISION);
}

static void trbe_stop_and_truncate_event(struct perf_output_handle *handle)
{
	struct trbe_buf *buf = etm_perf_sink_config(handle);

	 
	trbe_drain_and_disable_local(buf->cpudata);
	perf_aux_output_flag(handle, PERF_AUX_FLAG_TRUNCATED);
	perf_aux_output_end(handle, 0);
	*this_cpu_ptr(buf->cpudata->drvdata->handle) = NULL;
}

 

static void __trbe_pad_buf(struct trbe_buf *buf, u64 offset, int len)
{
	memset((void *)buf->trbe_base + offset, ETE_IGNORE_PACKET, len);
}

static void trbe_pad_buf(struct perf_output_handle *handle, int len)
{
	struct trbe_buf *buf = etm_perf_sink_config(handle);
	u64 head = PERF_IDX2OFF(handle->head, buf);

	__trbe_pad_buf(buf, head, len);
	if (!buf->snapshot)
		perf_aux_output_skip(handle, len);
}

static unsigned long trbe_snapshot_offset(struct perf_output_handle *handle)
{
	struct trbe_buf *buf = etm_perf_sink_config(handle);

	 
	return buf->nr_pages * PAGE_SIZE;
}

static u64 trbe_min_trace_buf_size(struct perf_output_handle *handle)
{
	u64 size = TRBE_TRACE_MIN_BUF_SIZE;
	struct trbe_buf *buf = etm_perf_sink_config(handle);
	struct trbe_cpudata *cpudata = buf->cpudata;

	 
	if (trbe_may_write_out_of_range(cpudata))
		size += PAGE_SIZE;
	return size;
}

 
static unsigned long __trbe_normal_offset(struct perf_output_handle *handle)
{
	struct trbe_buf *buf = etm_perf_sink_config(handle);
	struct trbe_cpudata *cpudata = buf->cpudata;
	const u64 bufsize = buf->nr_pages * PAGE_SIZE;
	u64 limit = bufsize;
	u64 head, tail, wakeup;

	head = PERF_IDX2OFF(handle->head, buf);

	 
	if (!IS_ALIGNED(head, cpudata->trbe_align)) {
		unsigned long delta = roundup(head, cpudata->trbe_align) - head;

		delta = min(delta, handle->size);
		trbe_pad_buf(handle, delta);
		head = PERF_IDX2OFF(handle->head, buf);
	}

	 
	if (!handle->size)
		return 0;

	 
	tail = PERF_IDX2OFF(handle->head + handle->size, buf);
	wakeup = PERF_IDX2OFF(handle->wakeup, buf);

	 
	if (head < tail)
		limit = round_down(tail, PAGE_SIZE);

	 
	if (handle->wakeup < (handle->head + handle->size) && head <= wakeup)
		limit = min(limit, round_up(wakeup, PAGE_SIZE));

	 
	if (limit > head)
		return limit;

	trbe_pad_buf(handle, handle->size);
	return 0;
}

static unsigned long trbe_normal_offset(struct perf_output_handle *handle)
{
	struct trbe_buf *buf = etm_perf_sink_config(handle);
	u64 limit = __trbe_normal_offset(handle);
	u64 head = PERF_IDX2OFF(handle->head, buf);

	 
	while (limit && ((limit - head) < trbe_min_trace_buf_size(handle))) {
		trbe_pad_buf(handle, limit - head);
		limit = __trbe_normal_offset(handle);
		head = PERF_IDX2OFF(handle->head, buf);
	}
	return limit;
}

static unsigned long compute_trbe_buffer_limit(struct perf_output_handle *handle)
{
	struct trbe_buf *buf = etm_perf_sink_config(handle);
	unsigned long offset;

	if (buf->snapshot)
		offset = trbe_snapshot_offset(handle);
	else
		offset = trbe_normal_offset(handle);
	return buf->trbe_base + offset;
}

static void clr_trbe_status(void)
{
	u64 trbsr = read_sysreg_s(SYS_TRBSR_EL1);

	WARN_ON(is_trbe_enabled());
	trbsr &= ~TRBSR_EL1_IRQ;
	trbsr &= ~TRBSR_EL1_TRG;
	trbsr &= ~TRBSR_EL1_WRAP;
	trbsr &= ~TRBSR_EL1_EC_MASK;
	trbsr &= ~TRBSR_EL1_BSC_MASK;
	trbsr &= ~TRBSR_EL1_S;
	write_sysreg_s(trbsr, SYS_TRBSR_EL1);
}

static void set_trbe_limit_pointer_enabled(struct trbe_buf *buf)
{
	u64 trblimitr = read_sysreg_s(SYS_TRBLIMITR_EL1);
	unsigned long addr = buf->trbe_limit;

	WARN_ON(!IS_ALIGNED(addr, (1UL << TRBLIMITR_EL1_LIMIT_SHIFT)));
	WARN_ON(!IS_ALIGNED(addr, PAGE_SIZE));

	trblimitr &= ~TRBLIMITR_EL1_nVM;
	trblimitr &= ~TRBLIMITR_EL1_FM_MASK;
	trblimitr &= ~TRBLIMITR_EL1_TM_MASK;
	trblimitr &= ~TRBLIMITR_EL1_LIMIT_MASK;

	 
	trblimitr |= (TRBLIMITR_EL1_FM_FILL << TRBLIMITR_EL1_FM_SHIFT) &
		     TRBLIMITR_EL1_FM_MASK;

	 
	trblimitr |= (TRBLIMITR_EL1_TM_IGNR << TRBLIMITR_EL1_TM_SHIFT) &
		     TRBLIMITR_EL1_TM_MASK;
	trblimitr |= (addr & PAGE_MASK);
	set_trbe_enabled(buf->cpudata, trblimitr);
}

static void trbe_enable_hw(struct trbe_buf *buf)
{
	WARN_ON(buf->trbe_hw_base < buf->trbe_base);
	WARN_ON(buf->trbe_write < buf->trbe_hw_base);
	WARN_ON(buf->trbe_write >= buf->trbe_limit);
	set_trbe_disabled(buf->cpudata);
	clr_trbe_status();
	set_trbe_base_pointer(buf->trbe_hw_base);
	set_trbe_write_pointer(buf->trbe_write);

	 
	isb();
	set_trbe_limit_pointer_enabled(buf);
}

static enum trbe_fault_action trbe_get_fault_act(struct perf_output_handle *handle,
						 u64 trbsr)
{
	int ec = get_trbe_ec(trbsr);
	int bsc = get_trbe_bsc(trbsr);
	struct trbe_buf *buf = etm_perf_sink_config(handle);
	struct trbe_cpudata *cpudata = buf->cpudata;

	WARN_ON(is_trbe_running(trbsr));
	if (is_trbe_trg(trbsr) || is_trbe_abort(trbsr))
		return TRBE_FAULT_ACT_FATAL;

	if ((ec == TRBE_EC_STAGE1_ABORT) || (ec == TRBE_EC_STAGE2_ABORT))
		return TRBE_FAULT_ACT_FATAL;

	 
	if ((is_trbe_wrap(trbsr) && (ec == TRBE_EC_OTHERS) && (bsc == TRBE_BSC_FILLED)) &&
	    (trbe_may_overwrite_in_fill_mode(cpudata) ||
	     get_trbe_write_pointer() == get_trbe_base_pointer()))
		return TRBE_FAULT_ACT_WRAP;

	return TRBE_FAULT_ACT_SPURIOUS;
}

static unsigned long trbe_get_trace_size(struct perf_output_handle *handle,
					 struct trbe_buf *buf, bool wrap)
{
	u64 write;
	u64 start_off, end_off;
	u64 size;
	u64 overwrite_skip = TRBE_WORKAROUND_OVERWRITE_FILL_MODE_SKIP_BYTES;

	 
	if (wrap)
		write = get_trbe_limit_pointer();
	else
		write = get_trbe_write_pointer();

	 
	end_off = write - buf->trbe_base;
	start_off = PERF_IDX2OFF(handle->head, buf);

	if (WARN_ON_ONCE(end_off < start_off))
		return 0;

	size = end_off - start_off;
	 
	if (trbe_has_erratum(buf->cpudata, TRBE_WORKAROUND_OVERWRITE_FILL_MODE) &&
	    !WARN_ON(size < overwrite_skip))
		__trbe_pad_buf(buf, start_off, overwrite_skip);

	return size;
}

static void *arm_trbe_alloc_buffer(struct coresight_device *csdev,
				   struct perf_event *event, void **pages,
				   int nr_pages, bool snapshot)
{
	struct trbe_buf *buf;
	struct page **pglist;
	int i;

	 
	if (nr_pages < 2)
		return NULL;

	buf = kzalloc_node(sizeof(*buf), GFP_KERNEL, trbe_alloc_node(event));
	if (!buf)
		return ERR_PTR(-ENOMEM);

	pglist = kcalloc(nr_pages, sizeof(*pglist), GFP_KERNEL);
	if (!pglist) {
		kfree(buf);
		return ERR_PTR(-ENOMEM);
	}

	for (i = 0; i < nr_pages; i++)
		pglist[i] = virt_to_page(pages[i]);

	buf->trbe_base = (unsigned long)vmap(pglist, nr_pages, VM_MAP, PAGE_KERNEL);
	if (!buf->trbe_base) {
		kfree(pglist);
		kfree(buf);
		return ERR_PTR(-ENOMEM);
	}
	buf->trbe_limit = buf->trbe_base + nr_pages * PAGE_SIZE;
	buf->trbe_write = buf->trbe_base;
	buf->snapshot = snapshot;
	buf->nr_pages = nr_pages;
	buf->pages = pages;
	kfree(pglist);
	return buf;
}

static void arm_trbe_free_buffer(void *config)
{
	struct trbe_buf *buf = config;

	vunmap((void *)buf->trbe_base);
	kfree(buf);
}

static unsigned long arm_trbe_update_buffer(struct coresight_device *csdev,
					    struct perf_output_handle *handle,
					    void *config)
{
	struct trbe_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);
	struct trbe_cpudata *cpudata = dev_get_drvdata(&csdev->dev);
	struct trbe_buf *buf = config;
	enum trbe_fault_action act;
	unsigned long size, status;
	unsigned long flags;
	bool wrap = false;

	WARN_ON(buf->cpudata != cpudata);
	WARN_ON(cpudata->cpu != smp_processor_id());
	WARN_ON(cpudata->drvdata != drvdata);
	if (cpudata->mode != CS_MODE_PERF)
		return 0;

	 
	local_irq_save(flags);

	 
	if (!is_trbe_enabled()) {
		size = 0;
		goto done;
	}
	 
	trbe_drain_and_disable_local(cpudata);

	 
	status = read_sysreg_s(SYS_TRBSR_EL1);
	if (is_trbe_irq(status)) {

		 
		clr_trbe_irq();
		isb();

		act = trbe_get_fault_act(handle, status);
		 
		if (act != TRBE_FAULT_ACT_WRAP) {
			size = 0;
			goto done;
		}

		trbe_report_wrap_event(handle);
		wrap = true;
	}

	size = trbe_get_trace_size(handle, buf, wrap);

done:
	local_irq_restore(flags);

	if (buf->snapshot)
		handle->head += size;
	return size;
}


static int trbe_apply_work_around_before_enable(struct trbe_buf *buf)
{
	 
	if (trbe_has_erratum(buf->cpudata, TRBE_WORKAROUND_OVERWRITE_FILL_MODE)) {
		if (WARN_ON(!IS_ALIGNED(buf->trbe_write, PAGE_SIZE)))
			return -EINVAL;
		buf->trbe_hw_base = buf->trbe_write;
		buf->trbe_write += TRBE_WORKAROUND_OVERWRITE_FILL_MODE_SKIP_BYTES;
	}

	 
	if (trbe_has_erratum(buf->cpudata, TRBE_WORKAROUND_WRITE_OUT_OF_RANGE)) {
		s64 space = buf->trbe_limit - buf->trbe_write;
		 
		if (WARN_ON(space <= PAGE_SIZE ||
			    !IS_ALIGNED(buf->trbe_limit, PAGE_SIZE)))
			return -EINVAL;
		buf->trbe_limit -= PAGE_SIZE;
	}

	return 0;
}

static int __arm_trbe_enable(struct trbe_buf *buf,
			     struct perf_output_handle *handle)
{
	int ret = 0;

	perf_aux_output_flag(handle, PERF_AUX_FLAG_CORESIGHT_FORMAT_RAW);
	buf->trbe_limit = compute_trbe_buffer_limit(handle);
	buf->trbe_write = buf->trbe_base + PERF_IDX2OFF(handle->head, buf);
	if (buf->trbe_limit == buf->trbe_base) {
		ret = -ENOSPC;
		goto err;
	}
	 
	buf->trbe_hw_base = buf->trbe_base;

	ret = trbe_apply_work_around_before_enable(buf);
	if (ret)
		goto err;

	*this_cpu_ptr(buf->cpudata->drvdata->handle) = handle;
	trbe_enable_hw(buf);
	return 0;
err:
	trbe_stop_and_truncate_event(handle);
	return ret;
}

static int arm_trbe_enable(struct coresight_device *csdev, enum cs_mode mode,
			   void *data)
{
	struct trbe_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);
	struct trbe_cpudata *cpudata = dev_get_drvdata(&csdev->dev);
	struct perf_output_handle *handle = data;
	struct trbe_buf *buf = etm_perf_sink_config(handle);

	WARN_ON(cpudata->cpu != smp_processor_id());
	WARN_ON(cpudata->drvdata != drvdata);
	if (mode != CS_MODE_PERF)
		return -EINVAL;

	cpudata->buf = buf;
	cpudata->mode = mode;
	buf->cpudata = cpudata;

	return __arm_trbe_enable(buf, handle);
}

static int arm_trbe_disable(struct coresight_device *csdev)
{
	struct trbe_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);
	struct trbe_cpudata *cpudata = dev_get_drvdata(&csdev->dev);
	struct trbe_buf *buf = cpudata->buf;

	WARN_ON(buf->cpudata != cpudata);
	WARN_ON(cpudata->cpu != smp_processor_id());
	WARN_ON(cpudata->drvdata != drvdata);
	if (cpudata->mode != CS_MODE_PERF)
		return -EINVAL;

	trbe_drain_and_disable_local(cpudata);
	buf->cpudata = NULL;
	cpudata->buf = NULL;
	cpudata->mode = CS_MODE_DISABLED;
	return 0;
}

static void trbe_handle_spurious(struct perf_output_handle *handle)
{
	struct trbe_buf *buf = etm_perf_sink_config(handle);
	u64 trblimitr = read_sysreg_s(SYS_TRBLIMITR_EL1);

	 
	set_trbe_enabled(buf->cpudata, trblimitr);
}

static int trbe_handle_overflow(struct perf_output_handle *handle)
{
	struct perf_event *event = handle->event;
	struct trbe_buf *buf = etm_perf_sink_config(handle);
	unsigned long size;
	struct etm_event_data *event_data;

	size = trbe_get_trace_size(handle, buf, true);
	if (buf->snapshot)
		handle->head += size;

	trbe_report_wrap_event(handle);
	perf_aux_output_end(handle, size);
	event_data = perf_aux_output_begin(handle, event);
	if (!event_data) {
		 
		trbe_drain_and_disable_local(buf->cpudata);
		*this_cpu_ptr(buf->cpudata->drvdata->handle) = NULL;
		return -EINVAL;
	}

	return __arm_trbe_enable(buf, handle);
}

static bool is_perf_trbe(struct perf_output_handle *handle)
{
	struct trbe_buf *buf = etm_perf_sink_config(handle);
	struct trbe_cpudata *cpudata = buf->cpudata;
	struct trbe_drvdata *drvdata = cpudata->drvdata;
	int cpu = smp_processor_id();

	WARN_ON(buf->trbe_hw_base != get_trbe_base_pointer());
	WARN_ON(buf->trbe_limit != get_trbe_limit_pointer());

	if (cpudata->mode != CS_MODE_PERF)
		return false;

	if (cpudata->cpu != cpu)
		return false;

	if (!cpumask_test_cpu(cpu, &drvdata->supported_cpus))
		return false;

	return true;
}

static irqreturn_t arm_trbe_irq_handler(int irq, void *dev)
{
	struct perf_output_handle **handle_ptr = dev;
	struct perf_output_handle *handle = *handle_ptr;
	struct trbe_buf *buf = etm_perf_sink_config(handle);
	enum trbe_fault_action act;
	u64 status;
	bool truncated = false;
	u64 trfcr;

	 
	status = read_sysreg_s(SYS_TRBSR_EL1);
	 
	if (!is_trbe_irq(status))
		return IRQ_NONE;

	 
	trfcr = cpu_prohibit_trace();
	 
	trbe_drain_and_disable_local(buf->cpudata);
	clr_trbe_irq();
	isb();

	if (WARN_ON_ONCE(!handle) || !perf_get_aux(handle))
		return IRQ_NONE;

	if (!is_perf_trbe(handle))
		return IRQ_NONE;

	act = trbe_get_fault_act(handle, status);
	switch (act) {
	case TRBE_FAULT_ACT_WRAP:
		truncated = !!trbe_handle_overflow(handle);
		break;
	case TRBE_FAULT_ACT_SPURIOUS:
		trbe_handle_spurious(handle);
		break;
	case TRBE_FAULT_ACT_FATAL:
		trbe_stop_and_truncate_event(handle);
		truncated = true;
		break;
	}

	 
	if (truncated)
		irq_work_run();
	else
		write_trfcr(trfcr);

	return IRQ_HANDLED;
}

static const struct coresight_ops_sink arm_trbe_sink_ops = {
	.enable		= arm_trbe_enable,
	.disable	= arm_trbe_disable,
	.alloc_buffer	= arm_trbe_alloc_buffer,
	.free_buffer	= arm_trbe_free_buffer,
	.update_buffer	= arm_trbe_update_buffer,
};

static const struct coresight_ops arm_trbe_cs_ops = {
	.sink_ops	= &arm_trbe_sink_ops,
};

static ssize_t align_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct trbe_cpudata *cpudata = dev_get_drvdata(dev);

	return sprintf(buf, "%llx\n", cpudata->trbe_hw_align);
}
static DEVICE_ATTR_RO(align);

static ssize_t flag_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct trbe_cpudata *cpudata = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", cpudata->trbe_flag);
}
static DEVICE_ATTR_RO(flag);

static struct attribute *arm_trbe_attrs[] = {
	&dev_attr_align.attr,
	&dev_attr_flag.attr,
	NULL,
};

static const struct attribute_group arm_trbe_group = {
	.attrs = arm_trbe_attrs,
};

static const struct attribute_group *arm_trbe_groups[] = {
	&arm_trbe_group,
	NULL,
};

static void arm_trbe_enable_cpu(void *info)
{
	struct trbe_drvdata *drvdata = info;
	struct trbe_cpudata *cpudata = this_cpu_ptr(drvdata->cpudata);

	trbe_reset_local(cpudata);
	enable_percpu_irq(drvdata->irq, IRQ_TYPE_NONE);
}

static void arm_trbe_disable_cpu(void *info)
{
	struct trbe_drvdata *drvdata = info;
	struct trbe_cpudata *cpudata = this_cpu_ptr(drvdata->cpudata);

	disable_percpu_irq(drvdata->irq);
	trbe_reset_local(cpudata);
}


static void arm_trbe_register_coresight_cpu(struct trbe_drvdata *drvdata, int cpu)
{
	struct trbe_cpudata *cpudata = per_cpu_ptr(drvdata->cpudata, cpu);
	struct coresight_device *trbe_csdev = coresight_get_percpu_sink(cpu);
	struct coresight_desc desc = { 0 };
	struct device *dev;

	if (WARN_ON(trbe_csdev))
		return;

	 
	if (WARN_ON(!cpudata->drvdata))
		return;

	dev = &cpudata->drvdata->pdev->dev;
	desc.name = devm_kasprintf(dev, GFP_KERNEL, "trbe%d", cpu);
	if (!desc.name)
		goto cpu_clear;

	desc.pdata = coresight_get_platform_data(dev);
	if (IS_ERR(desc.pdata))
		goto cpu_clear;

	desc.type = CORESIGHT_DEV_TYPE_SINK;
	desc.subtype.sink_subtype = CORESIGHT_DEV_SUBTYPE_SINK_PERCPU_SYSMEM;
	desc.ops = &arm_trbe_cs_ops;
	desc.groups = arm_trbe_groups;
	desc.dev = dev;
	trbe_csdev = coresight_register(&desc);
	if (IS_ERR(trbe_csdev))
		goto cpu_clear;

	dev_set_drvdata(&trbe_csdev->dev, cpudata);
	coresight_set_percpu_sink(cpu, trbe_csdev);
	return;
cpu_clear:
	cpumask_clear_cpu(cpu, &drvdata->supported_cpus);
}

 
static void arm_trbe_probe_cpu(void *info)
{
	struct trbe_drvdata *drvdata = info;
	int cpu = smp_processor_id();
	struct trbe_cpudata *cpudata = per_cpu_ptr(drvdata->cpudata, cpu);
	u64 trbidr;

	if (WARN_ON(!cpudata))
		goto cpu_clear;

	if (!is_trbe_available()) {
		pr_err("TRBE is not implemented on cpu %d\n", cpu);
		goto cpu_clear;
	}

	trbidr = read_sysreg_s(SYS_TRBIDR_EL1);
	if (!is_trbe_programmable(trbidr)) {
		pr_err("TRBE is owned in higher exception level on cpu %d\n", cpu);
		goto cpu_clear;
	}

	cpudata->trbe_hw_align = 1ULL << get_trbe_address_align(trbidr);
	if (cpudata->trbe_hw_align > SZ_2K) {
		pr_err("Unsupported alignment on cpu %d\n", cpu);
		goto cpu_clear;
	}

	 
	trbe_check_errata(cpudata);

	if (trbe_is_broken(cpudata)) {
		pr_err("Disabling TRBE on cpu%d due to erratum\n", cpu);
		goto cpu_clear;
	}

	 
	if (trbe_may_overwrite_in_fill_mode(cpudata))
		cpudata->trbe_align = PAGE_SIZE;
	else
		cpudata->trbe_align = cpudata->trbe_hw_align;

	cpudata->trbe_flag = get_trbe_flag_update(trbidr);
	cpudata->cpu = cpu;
	cpudata->drvdata = drvdata;
	return;
cpu_clear:
	cpumask_clear_cpu(cpu, &drvdata->supported_cpus);
}

static void arm_trbe_remove_coresight_cpu(struct trbe_drvdata *drvdata, int cpu)
{
	struct coresight_device *trbe_csdev = coresight_get_percpu_sink(cpu);

	if (trbe_csdev) {
		coresight_unregister(trbe_csdev);
		coresight_set_percpu_sink(cpu, NULL);
	}
}

static int arm_trbe_probe_coresight(struct trbe_drvdata *drvdata)
{
	int cpu;

	drvdata->cpudata = alloc_percpu(typeof(*drvdata->cpudata));
	if (!drvdata->cpudata)
		return -ENOMEM;

	for_each_cpu(cpu, &drvdata->supported_cpus) {
		 
		if (smp_call_function_single(cpu, arm_trbe_probe_cpu, drvdata, 1))
			continue;
		if (cpumask_test_cpu(cpu, &drvdata->supported_cpus))
			arm_trbe_register_coresight_cpu(drvdata, cpu);
		if (cpumask_test_cpu(cpu, &drvdata->supported_cpus))
			smp_call_function_single(cpu, arm_trbe_enable_cpu, drvdata, 1);
	}
	return 0;
}

static int arm_trbe_remove_coresight(struct trbe_drvdata *drvdata)
{
	int cpu;

	for_each_cpu(cpu, &drvdata->supported_cpus) {
		smp_call_function_single(cpu, arm_trbe_disable_cpu, drvdata, 1);
		arm_trbe_remove_coresight_cpu(drvdata, cpu);
	}
	free_percpu(drvdata->cpudata);
	return 0;
}

static void arm_trbe_probe_hotplugged_cpu(struct trbe_drvdata *drvdata)
{
	preempt_disable();
	arm_trbe_probe_cpu(drvdata);
	preempt_enable();
}

static int arm_trbe_cpu_startup(unsigned int cpu, struct hlist_node *node)
{
	struct trbe_drvdata *drvdata = hlist_entry_safe(node, struct trbe_drvdata, hotplug_node);

	if (cpumask_test_cpu(cpu, &drvdata->supported_cpus)) {

		 
		if (!coresight_get_percpu_sink(cpu)) {
			arm_trbe_probe_hotplugged_cpu(drvdata);
			if (cpumask_test_cpu(cpu, &drvdata->supported_cpus))
				arm_trbe_register_coresight_cpu(drvdata, cpu);
			if (cpumask_test_cpu(cpu, &drvdata->supported_cpus))
				arm_trbe_enable_cpu(drvdata);
		} else {
			arm_trbe_enable_cpu(drvdata);
		}
	}
	return 0;
}

static int arm_trbe_cpu_teardown(unsigned int cpu, struct hlist_node *node)
{
	struct trbe_drvdata *drvdata = hlist_entry_safe(node, struct trbe_drvdata, hotplug_node);

	if (cpumask_test_cpu(cpu, &drvdata->supported_cpus))
		arm_trbe_disable_cpu(drvdata);
	return 0;
}

static int arm_trbe_probe_cpuhp(struct trbe_drvdata *drvdata)
{
	enum cpuhp_state trbe_online;
	int ret;

	trbe_online = cpuhp_setup_state_multi(CPUHP_AP_ONLINE_DYN, DRVNAME,
					      arm_trbe_cpu_startup, arm_trbe_cpu_teardown);
	if (trbe_online < 0)
		return trbe_online;

	ret = cpuhp_state_add_instance(trbe_online, &drvdata->hotplug_node);
	if (ret) {
		cpuhp_remove_multi_state(trbe_online);
		return ret;
	}
	drvdata->trbe_online = trbe_online;
	return 0;
}

static void arm_trbe_remove_cpuhp(struct trbe_drvdata *drvdata)
{
	cpuhp_state_remove_instance(drvdata->trbe_online, &drvdata->hotplug_node);
	cpuhp_remove_multi_state(drvdata->trbe_online);
}

static int arm_trbe_probe_irq(struct platform_device *pdev,
			      struct trbe_drvdata *drvdata)
{
	int ret;

	drvdata->irq = platform_get_irq(pdev, 0);
	if (drvdata->irq < 0) {
		pr_err("IRQ not found for the platform device\n");
		return drvdata->irq;
	}

	if (!irq_is_percpu(drvdata->irq)) {
		pr_err("IRQ is not a PPI\n");
		return -EINVAL;
	}

	if (irq_get_percpu_devid_partition(drvdata->irq, &drvdata->supported_cpus))
		return -EINVAL;

	drvdata->handle = alloc_percpu(struct perf_output_handle *);
	if (!drvdata->handle)
		return -ENOMEM;

	ret = request_percpu_irq(drvdata->irq, arm_trbe_irq_handler, DRVNAME, drvdata->handle);
	if (ret) {
		free_percpu(drvdata->handle);
		return ret;
	}
	return 0;
}

static void arm_trbe_remove_irq(struct trbe_drvdata *drvdata)
{
	free_percpu_irq(drvdata->irq, drvdata->handle);
	free_percpu(drvdata->handle);
}

static int arm_trbe_device_probe(struct platform_device *pdev)
{
	struct trbe_drvdata *drvdata;
	struct device *dev = &pdev->dev;
	int ret;

	 
	if (arm64_kernel_unmapped_at_el0()) {
		pr_err("TRBE wouldn't work if kernel gets unmapped at EL0\n");
		return -EOPNOTSUPP;
	}

	drvdata = devm_kzalloc(dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;

	dev_set_drvdata(dev, drvdata);
	drvdata->pdev = pdev;
	ret = arm_trbe_probe_irq(pdev, drvdata);
	if (ret)
		return ret;

	ret = arm_trbe_probe_coresight(drvdata);
	if (ret)
		goto probe_failed;

	ret = arm_trbe_probe_cpuhp(drvdata);
	if (ret)
		goto cpuhp_failed;

	return 0;
cpuhp_failed:
	arm_trbe_remove_coresight(drvdata);
probe_failed:
	arm_trbe_remove_irq(drvdata);
	return ret;
}

static int arm_trbe_device_remove(struct platform_device *pdev)
{
	struct trbe_drvdata *drvdata = platform_get_drvdata(pdev);

	arm_trbe_remove_cpuhp(drvdata);
	arm_trbe_remove_coresight(drvdata);
	arm_trbe_remove_irq(drvdata);
	return 0;
}

static const struct of_device_id arm_trbe_of_match[] = {
	{ .compatible = "arm,trace-buffer-extension"},
	{},
};
MODULE_DEVICE_TABLE(of, arm_trbe_of_match);

static struct platform_driver arm_trbe_driver = {
	.driver	= {
		.name = DRVNAME,
		.of_match_table = of_match_ptr(arm_trbe_of_match),
		.suppress_bind_attrs = true,
	},
	.probe	= arm_trbe_device_probe,
	.remove	= arm_trbe_device_remove,
};

static int __init arm_trbe_init(void)
{
	int ret;

	ret = platform_driver_register(&arm_trbe_driver);
	if (!ret)
		return 0;

	pr_err("Error registering %s platform driver\n", DRVNAME);
	return ret;
}

static void __exit arm_trbe_exit(void)
{
	platform_driver_unregister(&arm_trbe_driver);
}
module_init(arm_trbe_init);
module_exit(arm_trbe_exit);

MODULE_AUTHOR("Anshuman Khandual <anshuman.khandual@arm.com>");
MODULE_DESCRIPTION("Arm Trace Buffer Extension (TRBE) driver");
MODULE_LICENSE("GPL v2");
