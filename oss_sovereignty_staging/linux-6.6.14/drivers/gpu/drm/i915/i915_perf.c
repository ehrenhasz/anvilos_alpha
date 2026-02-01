 


 

 

#include <linux/anon_inodes.h>
#include <linux/nospec.h>
#include <linux/sizes.h>
#include <linux/uuid.h>

#include "gem/i915_gem_context.h"
#include "gem/i915_gem_internal.h"
#include "gt/intel_engine_pm.h"
#include "gt/intel_engine_regs.h"
#include "gt/intel_engine_user.h"
#include "gt/intel_execlists_submission.h"
#include "gt/intel_gpu_commands.h"
#include "gt/intel_gt.h"
#include "gt/intel_gt_clock_utils.h"
#include "gt/intel_gt_mcr.h"
#include "gt/intel_gt_regs.h"
#include "gt/intel_lrc.h"
#include "gt/intel_lrc_reg.h"
#include "gt/intel_rc6.h"
#include "gt/intel_ring.h"
#include "gt/uc/intel_guc_slpc.h"

#include "i915_drv.h"
#include "i915_file_private.h"
#include "i915_perf.h"
#include "i915_perf_oa_regs.h"
#include "i915_reg.h"

 
#define OA_BUFFER_SIZE		SZ_16M

#define OA_TAKEN(tail, head)	((tail - head) & (OA_BUFFER_SIZE - 1))

 
#define OA_TAIL_MARGIN_NSEC	100000ULL
#define INVALID_TAIL_PTR	0xffffffff

 
#define DEFAULT_POLL_FREQUENCY_HZ 200
#define DEFAULT_POLL_PERIOD_NS (NSEC_PER_SEC / DEFAULT_POLL_FREQUENCY_HZ)

 
static u32 i915_perf_stream_paranoid = true;

 
#define OA_EXPONENT_MAX 31

#define INVALID_CTX_ID 0xffffffff

 
#define OAREPORT_REASON_MASK           0x3f
#define OAREPORT_REASON_MASK_EXTENDED  0x7f
#define OAREPORT_REASON_SHIFT          19
#define OAREPORT_REASON_TIMER          (1<<0)
#define OAREPORT_REASON_CTX_SWITCH     (1<<3)
#define OAREPORT_REASON_CLK_RATIO      (1<<5)

#define HAS_MI_SET_PREDICATE(i915) (GRAPHICS_VER_FULL(i915) >= IP_VER(12, 50))

 
static int oa_sample_rate_hard_limit;

 
static u32 i915_oa_max_sample_rate = 100000;

 
static const struct i915_oa_format oa_formats[I915_OA_FORMAT_MAX] = {
	[I915_OA_FORMAT_A13]	    = { 0, 64 },
	[I915_OA_FORMAT_A29]	    = { 1, 128 },
	[I915_OA_FORMAT_A13_B8_C8]  = { 2, 128 },
	 
	[I915_OA_FORMAT_B4_C8]	    = { 4, 64 },
	[I915_OA_FORMAT_A45_B8_C8]  = { 5, 256 },
	[I915_OA_FORMAT_B4_C8_A16]  = { 6, 128 },
	[I915_OA_FORMAT_C4_B8]	    = { 7, 64 },
	[I915_OA_FORMAT_A12]		    = { 0, 64 },
	[I915_OA_FORMAT_A12_B8_C8]	    = { 2, 128 },
	[I915_OA_FORMAT_A32u40_A4u32_B8_C8] = { 5, 256 },
	[I915_OAR_FORMAT_A32u40_A4u32_B8_C8]    = { 5, 256 },
	[I915_OA_FORMAT_A24u40_A14u32_B8_C8]    = { 5, 256 },
	[I915_OAM_FORMAT_MPEC8u64_B8_C8]	= { 1, 192, TYPE_OAM, HDR_64_BIT },
	[I915_OAM_FORMAT_MPEC8u32_B8_C8]	= { 2, 128, TYPE_OAM, HDR_64_BIT },
};

static const u32 mtl_oa_base[] = {
	[PERF_GROUP_OAM_SAMEDIA_0] = 0x393000,
};

#define SAMPLE_OA_REPORT      (1<<0)

 
struct perf_open_properties {
	u32 sample_flags;

	u64 single_context:1;
	u64 hold_preemption:1;
	u64 ctx_handle;

	 
	int metrics_set;
	int oa_format;
	bool oa_periodic;
	int oa_period_exponent;

	struct intel_engine_cs *engine;

	bool has_sseu;
	struct intel_sseu sseu;

	u64 poll_oa_period;
};

struct i915_oa_config_bo {
	struct llist_node node;

	struct i915_oa_config *oa_config;
	struct i915_vma *vma;
};

static struct ctl_table_header *sysctl_header;

static enum hrtimer_restart oa_poll_check_timer_cb(struct hrtimer *hrtimer);

void i915_oa_config_release(struct kref *ref)
{
	struct i915_oa_config *oa_config =
		container_of(ref, typeof(*oa_config), ref);

	kfree(oa_config->flex_regs);
	kfree(oa_config->b_counter_regs);
	kfree(oa_config->mux_regs);

	kfree_rcu(oa_config, rcu);
}

struct i915_oa_config *
i915_perf_get_oa_config(struct i915_perf *perf, int metrics_set)
{
	struct i915_oa_config *oa_config;

	rcu_read_lock();
	oa_config = idr_find(&perf->metrics_idr, metrics_set);
	if (oa_config)
		oa_config = i915_oa_config_get(oa_config);
	rcu_read_unlock();

	return oa_config;
}

static void free_oa_config_bo(struct i915_oa_config_bo *oa_bo)
{
	i915_oa_config_put(oa_bo->oa_config);
	i915_vma_put(oa_bo->vma);
	kfree(oa_bo);
}

static inline const
struct i915_perf_regs *__oa_regs(struct i915_perf_stream *stream)
{
	return &stream->engine->oa_group->regs;
}

static u32 gen12_oa_hw_tail_read(struct i915_perf_stream *stream)
{
	struct intel_uncore *uncore = stream->uncore;

	return intel_uncore_read(uncore, __oa_regs(stream)->oa_tail_ptr) &
	       GEN12_OAG_OATAILPTR_MASK;
}

static u32 gen8_oa_hw_tail_read(struct i915_perf_stream *stream)
{
	struct intel_uncore *uncore = stream->uncore;

	return intel_uncore_read(uncore, GEN8_OATAILPTR) & GEN8_OATAILPTR_MASK;
}

static u32 gen7_oa_hw_tail_read(struct i915_perf_stream *stream)
{
	struct intel_uncore *uncore = stream->uncore;
	u32 oastatus1 = intel_uncore_read(uncore, GEN7_OASTATUS1);

	return oastatus1 & GEN7_OASTATUS1_TAIL_MASK;
}

#define oa_report_header_64bit(__s) \
	((__s)->oa_buffer.format->header == HDR_64_BIT)

static u64 oa_report_id(struct i915_perf_stream *stream, void *report)
{
	return oa_report_header_64bit(stream) ? *(u64 *)report : *(u32 *)report;
}

static u64 oa_report_reason(struct i915_perf_stream *stream, void *report)
{
	return (oa_report_id(stream, report) >> OAREPORT_REASON_SHIFT) &
	       (GRAPHICS_VER(stream->perf->i915) == 12 ?
		OAREPORT_REASON_MASK_EXTENDED :
		OAREPORT_REASON_MASK);
}

static void oa_report_id_clear(struct i915_perf_stream *stream, u32 *report)
{
	if (oa_report_header_64bit(stream))
		*(u64 *)report = 0;
	else
		*report = 0;
}

static bool oa_report_ctx_invalid(struct i915_perf_stream *stream, void *report)
{
	return !(oa_report_id(stream, report) &
	       stream->perf->gen8_valid_ctx_bit);
}

static u64 oa_timestamp(struct i915_perf_stream *stream, void *report)
{
	return oa_report_header_64bit(stream) ?
		*((u64 *)report + 1) :
		*((u32 *)report + 1);
}

static void oa_timestamp_clear(struct i915_perf_stream *stream, u32 *report)
{
	if (oa_report_header_64bit(stream))
		*(u64 *)&report[2] = 0;
	else
		report[1] = 0;
}

static u32 oa_context_id(struct i915_perf_stream *stream, u32 *report)
{
	u32 ctx_id = oa_report_header_64bit(stream) ? report[4] : report[2];

	return ctx_id & stream->specific_ctx_id_mask;
}

static void oa_context_id_squash(struct i915_perf_stream *stream, u32 *report)
{
	if (oa_report_header_64bit(stream))
		report[4] = INVALID_CTX_ID;
	else
		report[2] = INVALID_CTX_ID;
}

 
static bool oa_buffer_check_unlocked(struct i915_perf_stream *stream)
{
	u32 gtt_offset = i915_ggtt_offset(stream->oa_buffer.vma);
	int report_size = stream->oa_buffer.format->size;
	u32 head, tail, read_tail;
	unsigned long flags;
	bool pollin;
	u32 hw_tail;
	u32 partial_report_size;

	 
	spin_lock_irqsave(&stream->oa_buffer.ptr_lock, flags);

	hw_tail = stream->perf->ops.oa_hw_tail_read(stream);

	 
	partial_report_size = OA_TAKEN(hw_tail, stream->oa_buffer.tail);
	partial_report_size %= report_size;

	 
	hw_tail = OA_TAKEN(hw_tail, partial_report_size);

	 
	head = stream->oa_buffer.head - gtt_offset;
	read_tail = stream->oa_buffer.tail - gtt_offset;

	tail = hw_tail;

	 
	while (OA_TAKEN(tail, read_tail) >= report_size) {
		void *report = stream->oa_buffer.vaddr + tail;

		if (oa_report_id(stream, report) ||
		    oa_timestamp(stream, report))
			break;

		tail = (tail - report_size) & (OA_BUFFER_SIZE - 1);
	}

	if (OA_TAKEN(hw_tail, tail) > report_size &&
	    __ratelimit(&stream->perf->tail_pointer_race))
		drm_notice(&stream->uncore->i915->drm,
			   "unlanded report(s) head=0x%x tail=0x%x hw_tail=0x%x\n",
		 head, tail, hw_tail);

	stream->oa_buffer.tail = gtt_offset + tail;

	pollin = OA_TAKEN(stream->oa_buffer.tail,
			  stream->oa_buffer.head) >= report_size;

	spin_unlock_irqrestore(&stream->oa_buffer.ptr_lock, flags);

	return pollin;
}

 
static int append_oa_status(struct i915_perf_stream *stream,
			    char __user *buf,
			    size_t count,
			    size_t *offset,
			    enum drm_i915_perf_record_type type)
{
	struct drm_i915_perf_record_header header = { type, 0, sizeof(header) };

	if ((count - *offset) < header.size)
		return -ENOSPC;

	if (copy_to_user(buf + *offset, &header, sizeof(header)))
		return -EFAULT;

	(*offset) += header.size;

	return 0;
}

 
static int append_oa_sample(struct i915_perf_stream *stream,
			    char __user *buf,
			    size_t count,
			    size_t *offset,
			    const u8 *report)
{
	int report_size = stream->oa_buffer.format->size;
	struct drm_i915_perf_record_header header;
	int report_size_partial;
	u8 *oa_buf_end;

	header.type = DRM_I915_PERF_RECORD_SAMPLE;
	header.pad = 0;
	header.size = stream->sample_size;

	if ((count - *offset) < header.size)
		return -ENOSPC;

	buf += *offset;
	if (copy_to_user(buf, &header, sizeof(header)))
		return -EFAULT;
	buf += sizeof(header);

	oa_buf_end = stream->oa_buffer.vaddr + OA_BUFFER_SIZE;
	report_size_partial = oa_buf_end - report;

	if (report_size_partial < report_size) {
		if (copy_to_user(buf, report, report_size_partial))
			return -EFAULT;
		buf += report_size_partial;

		if (copy_to_user(buf, stream->oa_buffer.vaddr,
				 report_size - report_size_partial))
			return -EFAULT;
	} else if (copy_to_user(buf, report, report_size)) {
		return -EFAULT;
	}

	(*offset) += header.size;

	return 0;
}

 
static int gen8_append_oa_reports(struct i915_perf_stream *stream,
				  char __user *buf,
				  size_t count,
				  size_t *offset)
{
	struct intel_uncore *uncore = stream->uncore;
	int report_size = stream->oa_buffer.format->size;
	u8 *oa_buf_base = stream->oa_buffer.vaddr;
	u32 gtt_offset = i915_ggtt_offset(stream->oa_buffer.vma);
	u32 mask = (OA_BUFFER_SIZE - 1);
	size_t start_offset = *offset;
	unsigned long flags;
	u32 head, tail;
	int ret = 0;

	if (drm_WARN_ON(&uncore->i915->drm, !stream->enabled))
		return -EIO;

	spin_lock_irqsave(&stream->oa_buffer.ptr_lock, flags);

	head = stream->oa_buffer.head;
	tail = stream->oa_buffer.tail;

	spin_unlock_irqrestore(&stream->oa_buffer.ptr_lock, flags);

	 
	head -= gtt_offset;
	tail -= gtt_offset;

	 
	if (drm_WARN_ONCE(&uncore->i915->drm,
			  head > OA_BUFFER_SIZE ||
			  tail > OA_BUFFER_SIZE,
			  "Inconsistent OA buffer pointers: head = %u, tail = %u\n",
			  head, tail))
		return -EIO;


	for ( ;
	     OA_TAKEN(tail, head);
	     head = (head + report_size) & mask) {
		u8 *report = oa_buf_base + head;
		u32 *report32 = (void *)report;
		u32 ctx_id;
		u64 reason;

		 
		reason = oa_report_reason(stream, report);
		ctx_id = oa_context_id(stream, report32);

		 

		if (oa_report_ctx_invalid(stream, report) &&
		    GRAPHICS_VER_FULL(stream->engine->i915) < IP_VER(12, 50)) {
			ctx_id = INVALID_CTX_ID;
			oa_context_id_squash(stream, report32);
		}

		 
		if (!stream->ctx ||
		    stream->specific_ctx_id == ctx_id ||
		    stream->oa_buffer.last_ctx_id == stream->specific_ctx_id ||
		    reason & OAREPORT_REASON_CTX_SWITCH) {

			 
			if (stream->ctx &&
			    stream->specific_ctx_id != ctx_id) {
				oa_context_id_squash(stream, report32);
			}

			ret = append_oa_sample(stream, buf, count, offset,
					       report);
			if (ret)
				break;

			stream->oa_buffer.last_ctx_id = ctx_id;
		}

		if (is_power_of_2(report_size)) {
			 
			oa_report_id_clear(stream, report32);
			oa_timestamp_clear(stream, report32);
		} else {
			u8 *oa_buf_end = stream->oa_buffer.vaddr +
					 OA_BUFFER_SIZE;
			u32 part = oa_buf_end - (u8 *)report32;

			 
			if (report_size <= part) {
				memset(report32, 0, report_size);
			} else {
				memset(report32, 0, part);
				memset(oa_buf_base, 0, report_size - part);
			}
		}
	}

	if (start_offset != *offset) {
		i915_reg_t oaheadptr;

		oaheadptr = GRAPHICS_VER(stream->perf->i915) == 12 ?
			    __oa_regs(stream)->oa_head_ptr :
			    GEN8_OAHEADPTR;

		spin_lock_irqsave(&stream->oa_buffer.ptr_lock, flags);

		 
		head += gtt_offset;
		intel_uncore_write(uncore, oaheadptr,
				   head & GEN12_OAG_OAHEADPTR_MASK);
		stream->oa_buffer.head = head;

		spin_unlock_irqrestore(&stream->oa_buffer.ptr_lock, flags);
	}

	return ret;
}

 
static int gen8_oa_read(struct i915_perf_stream *stream,
			char __user *buf,
			size_t count,
			size_t *offset)
{
	struct intel_uncore *uncore = stream->uncore;
	u32 oastatus;
	i915_reg_t oastatus_reg;
	int ret;

	if (drm_WARN_ON(&uncore->i915->drm, !stream->oa_buffer.vaddr))
		return -EIO;

	oastatus_reg = GRAPHICS_VER(stream->perf->i915) == 12 ?
		       __oa_regs(stream)->oa_status :
		       GEN8_OASTATUS;

	oastatus = intel_uncore_read(uncore, oastatus_reg);

	 
	if (oastatus & GEN8_OASTATUS_OABUFFER_OVERFLOW) {
		ret = append_oa_status(stream, buf, count, offset,
				       DRM_I915_PERF_RECORD_OA_BUFFER_LOST);
		if (ret)
			return ret;

		drm_dbg(&stream->perf->i915->drm,
			"OA buffer overflow (exponent = %d): force restart\n",
			stream->period_exponent);

		stream->perf->ops.oa_disable(stream);
		stream->perf->ops.oa_enable(stream);

		 
		oastatus = intel_uncore_read(uncore, oastatus_reg);
	}

	if (oastatus & GEN8_OASTATUS_REPORT_LOST) {
		ret = append_oa_status(stream, buf, count, offset,
				       DRM_I915_PERF_RECORD_OA_REPORT_LOST);
		if (ret)
			return ret;

		intel_uncore_rmw(uncore, oastatus_reg,
				 GEN8_OASTATUS_COUNTER_OVERFLOW |
				 GEN8_OASTATUS_REPORT_LOST,
				 IS_GRAPHICS_VER(uncore->i915, 8, 11) ?
				 (GEN8_OASTATUS_HEAD_POINTER_WRAP |
				  GEN8_OASTATUS_TAIL_POINTER_WRAP) : 0);
	}

	return gen8_append_oa_reports(stream, buf, count, offset);
}

 
static int gen7_append_oa_reports(struct i915_perf_stream *stream,
				  char __user *buf,
				  size_t count,
				  size_t *offset)
{
	struct intel_uncore *uncore = stream->uncore;
	int report_size = stream->oa_buffer.format->size;
	u8 *oa_buf_base = stream->oa_buffer.vaddr;
	u32 gtt_offset = i915_ggtt_offset(stream->oa_buffer.vma);
	u32 mask = (OA_BUFFER_SIZE - 1);
	size_t start_offset = *offset;
	unsigned long flags;
	u32 head, tail;
	int ret = 0;

	if (drm_WARN_ON(&uncore->i915->drm, !stream->enabled))
		return -EIO;

	spin_lock_irqsave(&stream->oa_buffer.ptr_lock, flags);

	head = stream->oa_buffer.head;
	tail = stream->oa_buffer.tail;

	spin_unlock_irqrestore(&stream->oa_buffer.ptr_lock, flags);

	 
	head -= gtt_offset;
	tail -= gtt_offset;

	 
	if (drm_WARN_ONCE(&uncore->i915->drm,
			  head > OA_BUFFER_SIZE || head % report_size ||
			  tail > OA_BUFFER_SIZE || tail % report_size,
			  "Inconsistent OA buffer pointers: head = %u, tail = %u\n",
			  head, tail))
		return -EIO;


	for ( ;
	     OA_TAKEN(tail, head);
	     head = (head + report_size) & mask) {
		u8 *report = oa_buf_base + head;
		u32 *report32 = (void *)report;

		 
		if (drm_WARN_ON(&uncore->i915->drm,
				(OA_BUFFER_SIZE - head) < report_size)) {
			drm_err(&uncore->i915->drm,
				"Spurious OA head ptr: non-integral report offset\n");
			break;
		}

		 
		if (report32[0] == 0) {
			if (__ratelimit(&stream->perf->spurious_report_rs))
				drm_notice(&uncore->i915->drm,
					   "Skipping spurious, invalid OA report\n");
			continue;
		}

		ret = append_oa_sample(stream, buf, count, offset, report);
		if (ret)
			break;

		 
		report32[0] = 0;
		report32[1] = 0;
	}

	if (start_offset != *offset) {
		spin_lock_irqsave(&stream->oa_buffer.ptr_lock, flags);

		 
		head += gtt_offset;

		intel_uncore_write(uncore, GEN7_OASTATUS2,
				   (head & GEN7_OASTATUS2_HEAD_MASK) |
				   GEN7_OASTATUS2_MEM_SELECT_GGTT);
		stream->oa_buffer.head = head;

		spin_unlock_irqrestore(&stream->oa_buffer.ptr_lock, flags);
	}

	return ret;
}

 
static int gen7_oa_read(struct i915_perf_stream *stream,
			char __user *buf,
			size_t count,
			size_t *offset)
{
	struct intel_uncore *uncore = stream->uncore;
	u32 oastatus1;
	int ret;

	if (drm_WARN_ON(&uncore->i915->drm, !stream->oa_buffer.vaddr))
		return -EIO;

	oastatus1 = intel_uncore_read(uncore, GEN7_OASTATUS1);

	 
	oastatus1 &= ~stream->perf->gen7_latched_oastatus1;

	 
	if (unlikely(oastatus1 & GEN7_OASTATUS1_OABUFFER_OVERFLOW)) {
		ret = append_oa_status(stream, buf, count, offset,
				       DRM_I915_PERF_RECORD_OA_BUFFER_LOST);
		if (ret)
			return ret;

		drm_dbg(&stream->perf->i915->drm,
			"OA buffer overflow (exponent = %d): force restart\n",
			stream->period_exponent);

		stream->perf->ops.oa_disable(stream);
		stream->perf->ops.oa_enable(stream);

		oastatus1 = intel_uncore_read(uncore, GEN7_OASTATUS1);
	}

	if (unlikely(oastatus1 & GEN7_OASTATUS1_REPORT_LOST)) {
		ret = append_oa_status(stream, buf, count, offset,
				       DRM_I915_PERF_RECORD_OA_REPORT_LOST);
		if (ret)
			return ret;
		stream->perf->gen7_latched_oastatus1 |=
			GEN7_OASTATUS1_REPORT_LOST;
	}

	return gen7_append_oa_reports(stream, buf, count, offset);
}

 
static int i915_oa_wait_unlocked(struct i915_perf_stream *stream)
{
	 
	if (!stream->periodic)
		return -EIO;

	return wait_event_interruptible(stream->poll_wq,
					oa_buffer_check_unlocked(stream));
}

 
static void i915_oa_poll_wait(struct i915_perf_stream *stream,
			      struct file *file,
			      poll_table *wait)
{
	poll_wait(file, &stream->poll_wq, wait);
}

 
static int i915_oa_read(struct i915_perf_stream *stream,
			char __user *buf,
			size_t count,
			size_t *offset)
{
	return stream->perf->ops.read(stream, buf, count, offset);
}

static struct intel_context *oa_pin_context(struct i915_perf_stream *stream)
{
	struct i915_gem_engines_iter it;
	struct i915_gem_context *ctx = stream->ctx;
	struct intel_context *ce;
	struct i915_gem_ww_ctx ww;
	int err = -ENODEV;

	for_each_gem_engine(ce, i915_gem_context_lock_engines(ctx), it) {
		if (ce->engine != stream->engine)  
			continue;

		err = 0;
		break;
	}
	i915_gem_context_unlock_engines(ctx);

	if (err)
		return ERR_PTR(err);

	i915_gem_ww_ctx_init(&ww, true);
retry:
	 
	err = intel_context_pin_ww(ce, &ww);
	if (err == -EDEADLK) {
		err = i915_gem_ww_ctx_backoff(&ww);
		if (!err)
			goto retry;
	}
	i915_gem_ww_ctx_fini(&ww);

	if (err)
		return ERR_PTR(err);

	stream->pinned_ctx = ce;
	return stream->pinned_ctx;
}

static int
__store_reg_to_mem(struct i915_request *rq, i915_reg_t reg, u32 ggtt_offset)
{
	u32 *cs, cmd;

	cmd = MI_STORE_REGISTER_MEM | MI_SRM_LRM_GLOBAL_GTT;
	if (GRAPHICS_VER(rq->i915) >= 8)
		cmd++;

	cs = intel_ring_begin(rq, 4);
	if (IS_ERR(cs))
		return PTR_ERR(cs);

	*cs++ = cmd;
	*cs++ = i915_mmio_reg_offset(reg);
	*cs++ = ggtt_offset;
	*cs++ = 0;

	intel_ring_advance(rq, cs);

	return 0;
}

static int
__read_reg(struct intel_context *ce, i915_reg_t reg, u32 ggtt_offset)
{
	struct i915_request *rq;
	int err;

	rq = i915_request_create(ce);
	if (IS_ERR(rq))
		return PTR_ERR(rq);

	i915_request_get(rq);

	err = __store_reg_to_mem(rq, reg, ggtt_offset);

	i915_request_add(rq);
	if (!err && i915_request_wait(rq, 0, HZ / 2) < 0)
		err = -ETIME;

	i915_request_put(rq);

	return err;
}

static int
gen12_guc_sw_ctx_id(struct intel_context *ce, u32 *ctx_id)
{
	struct i915_vma *scratch;
	u32 *val;
	int err;

	scratch = __vm_create_scratch_for_read_pinned(&ce->engine->gt->ggtt->vm, 4);
	if (IS_ERR(scratch))
		return PTR_ERR(scratch);

	err = i915_vma_sync(scratch);
	if (err)
		goto err_scratch;

	err = __read_reg(ce, RING_EXECLIST_STATUS_HI(ce->engine->mmio_base),
			 i915_ggtt_offset(scratch));
	if (err)
		goto err_scratch;

	val = i915_gem_object_pin_map_unlocked(scratch->obj, I915_MAP_WB);
	if (IS_ERR(val)) {
		err = PTR_ERR(val);
		goto err_scratch;
	}

	*ctx_id = *val;
	i915_gem_object_unpin_map(scratch->obj);

err_scratch:
	i915_vma_unpin_and_release(&scratch, 0);
	return err;
}

 
static int gen12_get_render_context_id(struct i915_perf_stream *stream)
{
	u32 ctx_id, mask;
	int ret;

	if (intel_engine_uses_guc(stream->engine)) {
		ret = gen12_guc_sw_ctx_id(stream->pinned_ctx, &ctx_id);
		if (ret)
			return ret;

		mask = ((1U << GEN12_GUC_SW_CTX_ID_WIDTH) - 1) <<
			(GEN12_GUC_SW_CTX_ID_SHIFT - 32);
	} else if (GRAPHICS_VER_FULL(stream->engine->i915) >= IP_VER(12, 50)) {
		ctx_id = (XEHP_MAX_CONTEXT_HW_ID - 1) <<
			(XEHP_SW_CTX_ID_SHIFT - 32);

		mask = ((1U << XEHP_SW_CTX_ID_WIDTH) - 1) <<
			(XEHP_SW_CTX_ID_SHIFT - 32);
	} else {
		ctx_id = (GEN12_MAX_CONTEXT_HW_ID - 1) <<
			 (GEN11_SW_CTX_ID_SHIFT - 32);

		mask = ((1U << GEN11_SW_CTX_ID_WIDTH) - 1) <<
			(GEN11_SW_CTX_ID_SHIFT - 32);
	}
	stream->specific_ctx_id = ctx_id & mask;
	stream->specific_ctx_id_mask = mask;

	return 0;
}

static bool oa_find_reg_in_lri(u32 *state, u32 reg, u32 *offset, u32 end)
{
	u32 idx = *offset;
	u32 len = min(MI_LRI_LEN(state[idx]) + idx, end);
	bool found = false;

	idx++;
	for (; idx < len; idx += 2) {
		if (state[idx] == reg) {
			found = true;
			break;
		}
	}

	*offset = idx;
	return found;
}

static u32 oa_context_image_offset(struct intel_context *ce, u32 reg)
{
	u32 offset, len = (ce->engine->context_size - PAGE_SIZE) / 4;
	u32 *state = ce->lrc_reg_state;

	if (drm_WARN_ON(&ce->engine->i915->drm, !state))
		return U32_MAX;

	for (offset = 0; offset < len; ) {
		if (IS_MI_LRI_CMD(state[offset])) {
			 
			drm_WARN_ON(&ce->engine->i915->drm,
				    MI_LRI_LEN(state[offset]) & 0x1);

			if (oa_find_reg_in_lri(state, reg, &offset, len))
				break;
		} else {
			offset++;
		}
	}

	return offset < len ? offset : U32_MAX;
}

static int set_oa_ctx_ctrl_offset(struct intel_context *ce)
{
	i915_reg_t reg = GEN12_OACTXCONTROL(ce->engine->mmio_base);
	struct i915_perf *perf = &ce->engine->i915->perf;
	u32 offset = perf->ctx_oactxctrl_offset;

	 
	if (offset)
		goto exit;

	offset = oa_context_image_offset(ce, i915_mmio_reg_offset(reg));
	perf->ctx_oactxctrl_offset = offset;

	drm_dbg(&ce->engine->i915->drm,
		"%s oa ctx control at 0x%08x dword offset\n",
		ce->engine->name, offset);

exit:
	return offset && offset != U32_MAX ? 0 : -ENODEV;
}

static bool engine_supports_mi_query(struct intel_engine_cs *engine)
{
	return engine->class == RENDER_CLASS;
}

 
static int oa_get_render_ctx_id(struct i915_perf_stream *stream)
{
	struct intel_context *ce;
	int ret = 0;

	ce = oa_pin_context(stream);
	if (IS_ERR(ce))
		return PTR_ERR(ce);

	if (engine_supports_mi_query(stream->engine) &&
	    HAS_LOGICAL_RING_CONTEXTS(stream->perf->i915)) {
		 
		ret = set_oa_ctx_ctrl_offset(ce);
		if (ret) {
			intel_context_unpin(ce);
			drm_err(&stream->perf->i915->drm,
				"Enabling perf query failed for %s\n",
				stream->engine->name);
			return ret;
		}
	}

	switch (GRAPHICS_VER(ce->engine->i915)) {
	case 7: {
		 
		stream->specific_ctx_id = i915_ggtt_offset(ce->state);
		stream->specific_ctx_id_mask = 0;
		break;
	}

	case 8:
	case 9:
		if (intel_engine_uses_guc(ce->engine)) {
			 
			stream->specific_ctx_id = ce->lrc.lrca >> 12;

			 
			stream->specific_ctx_id_mask =
				(1U << (GEN8_CTX_ID_WIDTH - 1)) - 1;
		} else {
			stream->specific_ctx_id_mask =
				(1U << GEN8_CTX_ID_WIDTH) - 1;
			stream->specific_ctx_id = stream->specific_ctx_id_mask;
		}
		break;

	case 11:
	case 12:
		ret = gen12_get_render_context_id(stream);
		break;

	default:
		MISSING_CASE(GRAPHICS_VER(ce->engine->i915));
	}

	ce->tag = stream->specific_ctx_id;

	drm_dbg(&stream->perf->i915->drm,
		"filtering on ctx_id=0x%x ctx_id_mask=0x%x\n",
		stream->specific_ctx_id,
		stream->specific_ctx_id_mask);

	return ret;
}

 
static void oa_put_render_ctx_id(struct i915_perf_stream *stream)
{
	struct intel_context *ce;

	ce = fetch_and_zero(&stream->pinned_ctx);
	if (ce) {
		ce->tag = 0;  
		intel_context_unpin(ce);
	}

	stream->specific_ctx_id = INVALID_CTX_ID;
	stream->specific_ctx_id_mask = 0;
}

static void
free_oa_buffer(struct i915_perf_stream *stream)
{
	i915_vma_unpin_and_release(&stream->oa_buffer.vma,
				   I915_VMA_RELEASE_MAP);

	stream->oa_buffer.vaddr = NULL;
}

static void
free_oa_configs(struct i915_perf_stream *stream)
{
	struct i915_oa_config_bo *oa_bo, *tmp;

	i915_oa_config_put(stream->oa_config);
	llist_for_each_entry_safe(oa_bo, tmp, stream->oa_config_bos.first, node)
		free_oa_config_bo(oa_bo);
}

static void
free_noa_wait(struct i915_perf_stream *stream)
{
	i915_vma_unpin_and_release(&stream->noa_wait, 0);
}

static bool engine_supports_oa(const struct intel_engine_cs *engine)
{
	return engine->oa_group;
}

static bool engine_supports_oa_format(struct intel_engine_cs *engine, int type)
{
	return engine->oa_group && engine->oa_group->type == type;
}

static void i915_oa_stream_destroy(struct i915_perf_stream *stream)
{
	struct i915_perf *perf = stream->perf;
	struct intel_gt *gt = stream->engine->gt;
	struct i915_perf_group *g = stream->engine->oa_group;

	if (WARN_ON(stream != g->exclusive_stream))
		return;

	 
	WRITE_ONCE(g->exclusive_stream, NULL);
	perf->ops.disable_metric_set(stream);

	free_oa_buffer(stream);

	 
	if (stream->override_gucrc)
		drm_WARN_ON(&gt->i915->drm,
			    intel_guc_slpc_unset_gucrc_mode(&gt->uc.guc.slpc));

	intel_uncore_forcewake_put(stream->uncore, FORCEWAKE_ALL);
	intel_engine_pm_put(stream->engine);

	if (stream->ctx)
		oa_put_render_ctx_id(stream);

	free_oa_configs(stream);
	free_noa_wait(stream);

	if (perf->spurious_report_rs.missed) {
		drm_notice(&gt->i915->drm,
			   "%d spurious OA report notices suppressed due to ratelimiting\n",
			   perf->spurious_report_rs.missed);
	}
}

static void gen7_init_oa_buffer(struct i915_perf_stream *stream)
{
	struct intel_uncore *uncore = stream->uncore;
	u32 gtt_offset = i915_ggtt_offset(stream->oa_buffer.vma);
	unsigned long flags;

	spin_lock_irqsave(&stream->oa_buffer.ptr_lock, flags);

	 
	intel_uncore_write(uncore, GEN7_OASTATUS2,  
			   gtt_offset | GEN7_OASTATUS2_MEM_SELECT_GGTT);
	stream->oa_buffer.head = gtt_offset;

	intel_uncore_write(uncore, GEN7_OABUFFER, gtt_offset);

	intel_uncore_write(uncore, GEN7_OASTATUS1,  
			   gtt_offset | OABUFFER_SIZE_16M);

	 
	stream->oa_buffer.tail = gtt_offset;

	spin_unlock_irqrestore(&stream->oa_buffer.ptr_lock, flags);

	 
	stream->perf->gen7_latched_oastatus1 = 0;

	 
	memset(stream->oa_buffer.vaddr, 0, OA_BUFFER_SIZE);
}

static void gen8_init_oa_buffer(struct i915_perf_stream *stream)
{
	struct intel_uncore *uncore = stream->uncore;
	u32 gtt_offset = i915_ggtt_offset(stream->oa_buffer.vma);
	unsigned long flags;

	spin_lock_irqsave(&stream->oa_buffer.ptr_lock, flags);

	intel_uncore_write(uncore, GEN8_OASTATUS, 0);
	intel_uncore_write(uncore, GEN8_OAHEADPTR, gtt_offset);
	stream->oa_buffer.head = gtt_offset;

	intel_uncore_write(uncore, GEN8_OABUFFER_UDW, 0);

	 
	intel_uncore_write(uncore, GEN8_OABUFFER, gtt_offset |
		   OABUFFER_SIZE_16M | GEN8_OABUFFER_MEM_SELECT_GGTT);
	intel_uncore_write(uncore, GEN8_OATAILPTR, gtt_offset & GEN8_OATAILPTR_MASK);

	 
	stream->oa_buffer.tail = gtt_offset;

	 
	stream->oa_buffer.last_ctx_id = INVALID_CTX_ID;

	spin_unlock_irqrestore(&stream->oa_buffer.ptr_lock, flags);

	 
	memset(stream->oa_buffer.vaddr, 0, OA_BUFFER_SIZE);
}

static void gen12_init_oa_buffer(struct i915_perf_stream *stream)
{
	struct intel_uncore *uncore = stream->uncore;
	u32 gtt_offset = i915_ggtt_offset(stream->oa_buffer.vma);
	unsigned long flags;

	spin_lock_irqsave(&stream->oa_buffer.ptr_lock, flags);

	intel_uncore_write(uncore, __oa_regs(stream)->oa_status, 0);
	intel_uncore_write(uncore, __oa_regs(stream)->oa_head_ptr,
			   gtt_offset & GEN12_OAG_OAHEADPTR_MASK);
	stream->oa_buffer.head = gtt_offset;

	 
	intel_uncore_write(uncore, __oa_regs(stream)->oa_buffer, gtt_offset |
			   OABUFFER_SIZE_16M | GEN8_OABUFFER_MEM_SELECT_GGTT);
	intel_uncore_write(uncore, __oa_regs(stream)->oa_tail_ptr,
			   gtt_offset & GEN12_OAG_OATAILPTR_MASK);

	 
	stream->oa_buffer.tail = gtt_offset;

	 
	stream->oa_buffer.last_ctx_id = INVALID_CTX_ID;

	spin_unlock_irqrestore(&stream->oa_buffer.ptr_lock, flags);

	 
	memset(stream->oa_buffer.vaddr, 0,
	       stream->oa_buffer.vma->size);
}

static int alloc_oa_buffer(struct i915_perf_stream *stream)
{
	struct drm_i915_private *i915 = stream->perf->i915;
	struct intel_gt *gt = stream->engine->gt;
	struct drm_i915_gem_object *bo;
	struct i915_vma *vma;
	int ret;

	if (drm_WARN_ON(&i915->drm, stream->oa_buffer.vma))
		return -ENODEV;

	BUILD_BUG_ON_NOT_POWER_OF_2(OA_BUFFER_SIZE);
	BUILD_BUG_ON(OA_BUFFER_SIZE < SZ_128K || OA_BUFFER_SIZE > SZ_16M);

	bo = i915_gem_object_create_shmem(stream->perf->i915, OA_BUFFER_SIZE);
	if (IS_ERR(bo)) {
		drm_err(&i915->drm, "Failed to allocate OA buffer\n");
		return PTR_ERR(bo);
	}

	i915_gem_object_set_cache_coherency(bo, I915_CACHE_LLC);

	 
	vma = i915_vma_instance(bo, &gt->ggtt->vm, NULL);
	if (IS_ERR(vma)) {
		ret = PTR_ERR(vma);
		goto err_unref;
	}

	 
	ret = i915_vma_pin(vma, 0, SZ_16M, PIN_GLOBAL | PIN_HIGH);
	if (ret) {
		drm_err(&gt->i915->drm, "Failed to pin OA buffer %d\n", ret);
		goto err_unref;
	}

	stream->oa_buffer.vma = vma;

	stream->oa_buffer.vaddr =
		i915_gem_object_pin_map_unlocked(bo, I915_MAP_WB);
	if (IS_ERR(stream->oa_buffer.vaddr)) {
		ret = PTR_ERR(stream->oa_buffer.vaddr);
		goto err_unpin;
	}

	return 0;

err_unpin:
	__i915_vma_unpin(vma);

err_unref:
	i915_gem_object_put(bo);

	stream->oa_buffer.vaddr = NULL;
	stream->oa_buffer.vma = NULL;

	return ret;
}

static u32 *save_restore_register(struct i915_perf_stream *stream, u32 *cs,
				  bool save, i915_reg_t reg, u32 offset,
				  u32 dword_count)
{
	u32 cmd;
	u32 d;

	cmd = save ? MI_STORE_REGISTER_MEM : MI_LOAD_REGISTER_MEM;
	cmd |= MI_SRM_LRM_GLOBAL_GTT;
	if (GRAPHICS_VER(stream->perf->i915) >= 8)
		cmd++;

	for (d = 0; d < dword_count; d++) {
		*cs++ = cmd;
		*cs++ = i915_mmio_reg_offset(reg) + 4 * d;
		*cs++ = i915_ggtt_offset(stream->noa_wait) + offset + 4 * d;
		*cs++ = 0;
	}

	return cs;
}

static int alloc_noa_wait(struct i915_perf_stream *stream)
{
	struct drm_i915_private *i915 = stream->perf->i915;
	struct intel_gt *gt = stream->engine->gt;
	struct drm_i915_gem_object *bo;
	struct i915_vma *vma;
	const u64 delay_ticks = 0xffffffffffffffff -
		intel_gt_ns_to_clock_interval(to_gt(stream->perf->i915),
		atomic64_read(&stream->perf->noa_programming_delay));
	const u32 base = stream->engine->mmio_base;
#define CS_GPR(x) GEN8_RING_CS_GPR(base, x)
	u32 *batch, *ts0, *cs, *jump;
	struct i915_gem_ww_ctx ww;
	int ret, i;
	enum {
		START_TS,
		NOW_TS,
		DELTA_TS,
		JUMP_PREDICATE,
		DELTA_TARGET,
		N_CS_GPR
	};
	i915_reg_t mi_predicate_result = HAS_MI_SET_PREDICATE(i915) ?
					  MI_PREDICATE_RESULT_2_ENGINE(base) :
					  MI_PREDICATE_RESULT_1(RENDER_RING_BASE);

	 
	bo = i915_gem_object_create_internal(i915, 8192);
	if (IS_ERR(bo)) {
		drm_err(&i915->drm,
			"Failed to allocate NOA wait batchbuffer\n");
		return PTR_ERR(bo);
	}

	i915_gem_ww_ctx_init(&ww, true);
retry:
	ret = i915_gem_object_lock(bo, &ww);
	if (ret)
		goto out_ww;

	 
	vma = i915_vma_instance(bo, &gt->ggtt->vm, NULL);
	if (IS_ERR(vma)) {
		ret = PTR_ERR(vma);
		goto out_ww;
	}

	ret = i915_vma_pin_ww(vma, &ww, 0, 0, PIN_GLOBAL | PIN_HIGH);
	if (ret)
		goto out_ww;

	batch = cs = i915_gem_object_pin_map(bo, I915_MAP_WB);
	if (IS_ERR(batch)) {
		ret = PTR_ERR(batch);
		goto err_unpin;
	}

	stream->noa_wait = vma;

#define GPR_SAVE_OFFSET 4096
#define PREDICATE_SAVE_OFFSET 4160

	 
	for (i = 0; i < N_CS_GPR; i++)
		cs = save_restore_register(
			stream, cs, true  , CS_GPR(i),
			GPR_SAVE_OFFSET + 8 * i, 2);
	cs = save_restore_register(
		stream, cs, true  , mi_predicate_result,
		PREDICATE_SAVE_OFFSET, 1);

	 
	ts0 = cs;

	 
	*cs++ = MI_LOAD_REGISTER_IMM(1);
	*cs++ = i915_mmio_reg_offset(CS_GPR(START_TS)) + 4;
	*cs++ = 0;
	*cs++ = MI_LOAD_REGISTER_REG | (3 - 2);
	*cs++ = i915_mmio_reg_offset(RING_TIMESTAMP(base));
	*cs++ = i915_mmio_reg_offset(CS_GPR(START_TS));

	 
	jump = cs;

	 
	*cs++ = MI_LOAD_REGISTER_IMM(1);
	*cs++ = i915_mmio_reg_offset(CS_GPR(NOW_TS)) + 4;
	*cs++ = 0;
	*cs++ = MI_LOAD_REGISTER_REG | (3 - 2);
	*cs++ = i915_mmio_reg_offset(RING_TIMESTAMP(base));
	*cs++ = i915_mmio_reg_offset(CS_GPR(NOW_TS));

	 
	*cs++ = MI_MATH(5);
	*cs++ = MI_MATH_LOAD(MI_MATH_REG_SRCA, MI_MATH_REG(NOW_TS));
	*cs++ = MI_MATH_LOAD(MI_MATH_REG_SRCB, MI_MATH_REG(START_TS));
	*cs++ = MI_MATH_SUB;
	*cs++ = MI_MATH_STORE(MI_MATH_REG(DELTA_TS), MI_MATH_REG_ACCU);
	*cs++ = MI_MATH_STORE(MI_MATH_REG(JUMP_PREDICATE), MI_MATH_REG_CF);

	 
	*cs++ = MI_LOAD_REGISTER_REG | (3 - 2);
	*cs++ = i915_mmio_reg_offset(CS_GPR(JUMP_PREDICATE));
	*cs++ = i915_mmio_reg_offset(mi_predicate_result);

	if (HAS_MI_SET_PREDICATE(i915))
		*cs++ = MI_SET_PREDICATE | 1;

	 
	*cs++ = (GRAPHICS_VER(i915) < 8 ?
		 MI_BATCH_BUFFER_START :
		 MI_BATCH_BUFFER_START_GEN8) |
		MI_BATCH_PREDICATE;
	*cs++ = i915_ggtt_offset(vma) + (ts0 - batch) * 4;
	*cs++ = 0;

	if (HAS_MI_SET_PREDICATE(i915))
		*cs++ = MI_SET_PREDICATE;

	 
	*cs++ = MI_LOAD_REGISTER_IMM(2);
	*cs++ = i915_mmio_reg_offset(CS_GPR(DELTA_TARGET));
	*cs++ = lower_32_bits(delay_ticks);
	*cs++ = i915_mmio_reg_offset(CS_GPR(DELTA_TARGET)) + 4;
	*cs++ = upper_32_bits(delay_ticks);

	*cs++ = MI_MATH(4);
	*cs++ = MI_MATH_LOAD(MI_MATH_REG_SRCA, MI_MATH_REG(DELTA_TS));
	*cs++ = MI_MATH_LOAD(MI_MATH_REG_SRCB, MI_MATH_REG(DELTA_TARGET));
	*cs++ = MI_MATH_ADD;
	*cs++ = MI_MATH_STOREINV(MI_MATH_REG(JUMP_PREDICATE), MI_MATH_REG_CF);

	*cs++ = MI_ARB_CHECK;

	 
	*cs++ = MI_LOAD_REGISTER_REG | (3 - 2);
	*cs++ = i915_mmio_reg_offset(CS_GPR(JUMP_PREDICATE));
	*cs++ = i915_mmio_reg_offset(mi_predicate_result);

	if (HAS_MI_SET_PREDICATE(i915))
		*cs++ = MI_SET_PREDICATE | 1;

	 
	*cs++ = (GRAPHICS_VER(i915) < 8 ?
		 MI_BATCH_BUFFER_START :
		 MI_BATCH_BUFFER_START_GEN8) |
		MI_BATCH_PREDICATE;
	*cs++ = i915_ggtt_offset(vma) + (jump - batch) * 4;
	*cs++ = 0;

	if (HAS_MI_SET_PREDICATE(i915))
		*cs++ = MI_SET_PREDICATE;

	 
	for (i = 0; i < N_CS_GPR; i++)
		cs = save_restore_register(
			stream, cs, false  , CS_GPR(i),
			GPR_SAVE_OFFSET + 8 * i, 2);
	cs = save_restore_register(
		stream, cs, false  , mi_predicate_result,
		PREDICATE_SAVE_OFFSET, 1);

	 
	*cs++ = MI_BATCH_BUFFER_END;

	GEM_BUG_ON(cs - batch > PAGE_SIZE / sizeof(*batch));

	i915_gem_object_flush_map(bo);
	__i915_gem_object_release_map(bo);

	goto out_ww;

err_unpin:
	i915_vma_unpin_and_release(&vma, 0);
out_ww:
	if (ret == -EDEADLK) {
		ret = i915_gem_ww_ctx_backoff(&ww);
		if (!ret)
			goto retry;
	}
	i915_gem_ww_ctx_fini(&ww);
	if (ret)
		i915_gem_object_put(bo);
	return ret;
}

static u32 *write_cs_mi_lri(u32 *cs,
			    const struct i915_oa_reg *reg_data,
			    u32 n_regs)
{
	u32 i;

	for (i = 0; i < n_regs; i++) {
		if ((i % MI_LOAD_REGISTER_IMM_MAX_REGS) == 0) {
			u32 n_lri = min_t(u32,
					  n_regs - i,
					  MI_LOAD_REGISTER_IMM_MAX_REGS);

			*cs++ = MI_LOAD_REGISTER_IMM(n_lri);
		}
		*cs++ = i915_mmio_reg_offset(reg_data[i].addr);
		*cs++ = reg_data[i].value;
	}

	return cs;
}

static int num_lri_dwords(int num_regs)
{
	int count = 0;

	if (num_regs > 0) {
		count += DIV_ROUND_UP(num_regs, MI_LOAD_REGISTER_IMM_MAX_REGS);
		count += num_regs * 2;
	}

	return count;
}

static struct i915_oa_config_bo *
alloc_oa_config_buffer(struct i915_perf_stream *stream,
		       struct i915_oa_config *oa_config)
{
	struct drm_i915_gem_object *obj;
	struct i915_oa_config_bo *oa_bo;
	struct i915_gem_ww_ctx ww;
	size_t config_length = 0;
	u32 *cs;
	int err;

	oa_bo = kzalloc(sizeof(*oa_bo), GFP_KERNEL);
	if (!oa_bo)
		return ERR_PTR(-ENOMEM);

	config_length += num_lri_dwords(oa_config->mux_regs_len);
	config_length += num_lri_dwords(oa_config->b_counter_regs_len);
	config_length += num_lri_dwords(oa_config->flex_regs_len);
	config_length += 3;  
	config_length = ALIGN(sizeof(u32) * config_length, I915_GTT_PAGE_SIZE);

	obj = i915_gem_object_create_shmem(stream->perf->i915, config_length);
	if (IS_ERR(obj)) {
		err = PTR_ERR(obj);
		goto err_free;
	}

	i915_gem_ww_ctx_init(&ww, true);
retry:
	err = i915_gem_object_lock(obj, &ww);
	if (err)
		goto out_ww;

	cs = i915_gem_object_pin_map(obj, I915_MAP_WB);
	if (IS_ERR(cs)) {
		err = PTR_ERR(cs);
		goto out_ww;
	}

	cs = write_cs_mi_lri(cs,
			     oa_config->mux_regs,
			     oa_config->mux_regs_len);
	cs = write_cs_mi_lri(cs,
			     oa_config->b_counter_regs,
			     oa_config->b_counter_regs_len);
	cs = write_cs_mi_lri(cs,
			     oa_config->flex_regs,
			     oa_config->flex_regs_len);

	 
	*cs++ = (GRAPHICS_VER(stream->perf->i915) < 8 ?
		 MI_BATCH_BUFFER_START :
		 MI_BATCH_BUFFER_START_GEN8);
	*cs++ = i915_ggtt_offset(stream->noa_wait);
	*cs++ = 0;

	i915_gem_object_flush_map(obj);
	__i915_gem_object_release_map(obj);

	oa_bo->vma = i915_vma_instance(obj,
				       &stream->engine->gt->ggtt->vm,
				       NULL);
	if (IS_ERR(oa_bo->vma)) {
		err = PTR_ERR(oa_bo->vma);
		goto out_ww;
	}

	oa_bo->oa_config = i915_oa_config_get(oa_config);
	llist_add(&oa_bo->node, &stream->oa_config_bos);

out_ww:
	if (err == -EDEADLK) {
		err = i915_gem_ww_ctx_backoff(&ww);
		if (!err)
			goto retry;
	}
	i915_gem_ww_ctx_fini(&ww);

	if (err)
		i915_gem_object_put(obj);
err_free:
	if (err) {
		kfree(oa_bo);
		return ERR_PTR(err);
	}
	return oa_bo;
}

static struct i915_vma *
get_oa_vma(struct i915_perf_stream *stream, struct i915_oa_config *oa_config)
{
	struct i915_oa_config_bo *oa_bo;

	 
	llist_for_each_entry(oa_bo, stream->oa_config_bos.first, node) {
		if (oa_bo->oa_config == oa_config &&
		    memcmp(oa_bo->oa_config->uuid,
			   oa_config->uuid,
			   sizeof(oa_config->uuid)) == 0)
			goto out;
	}

	oa_bo = alloc_oa_config_buffer(stream, oa_config);
	if (IS_ERR(oa_bo))
		return ERR_CAST(oa_bo);

out:
	return i915_vma_get(oa_bo->vma);
}

static int
emit_oa_config(struct i915_perf_stream *stream,
	       struct i915_oa_config *oa_config,
	       struct intel_context *ce,
	       struct i915_active *active)
{
	struct i915_request *rq;
	struct i915_vma *vma;
	struct i915_gem_ww_ctx ww;
	int err;

	vma = get_oa_vma(stream, oa_config);
	if (IS_ERR(vma))
		return PTR_ERR(vma);

	i915_gem_ww_ctx_init(&ww, true);
retry:
	err = i915_gem_object_lock(vma->obj, &ww);
	if (err)
		goto err;

	err = i915_vma_pin_ww(vma, &ww, 0, 0, PIN_GLOBAL | PIN_HIGH);
	if (err)
		goto err;

	intel_engine_pm_get(ce->engine);
	rq = i915_request_create(ce);
	intel_engine_pm_put(ce->engine);
	if (IS_ERR(rq)) {
		err = PTR_ERR(rq);
		goto err_vma_unpin;
	}

	if (!IS_ERR_OR_NULL(active)) {
		 
		err = i915_request_await_active(rq, active,
						I915_ACTIVE_AWAIT_ACTIVE);
		if (err)
			goto err_add_request;

		err = i915_active_add_request(active, rq);
		if (err)
			goto err_add_request;
	}

	err = i915_vma_move_to_active(vma, rq, 0);
	if (err)
		goto err_add_request;

	err = rq->engine->emit_bb_start(rq,
					i915_vma_offset(vma), 0,
					I915_DISPATCH_SECURE);
	if (err)
		goto err_add_request;

err_add_request:
	i915_request_add(rq);
err_vma_unpin:
	i915_vma_unpin(vma);
err:
	if (err == -EDEADLK) {
		err = i915_gem_ww_ctx_backoff(&ww);
		if (!err)
			goto retry;
	}

	i915_gem_ww_ctx_fini(&ww);
	i915_vma_put(vma);
	return err;
}

static struct intel_context *oa_context(struct i915_perf_stream *stream)
{
	return stream->pinned_ctx ?: stream->engine->kernel_context;
}

static int
hsw_enable_metric_set(struct i915_perf_stream *stream,
		      struct i915_active *active)
{
	struct intel_uncore *uncore = stream->uncore;

	 
	intel_uncore_rmw(uncore, GEN7_MISCCPCTL,
			 GEN7_DOP_CLOCK_GATE_ENABLE, 0);
	intel_uncore_rmw(uncore, GEN6_UCGCTL1,
			 0, GEN6_CSUNIT_CLOCK_GATE_DISABLE);

	return emit_oa_config(stream,
			      stream->oa_config, oa_context(stream),
			      active);
}

static void hsw_disable_metric_set(struct i915_perf_stream *stream)
{
	struct intel_uncore *uncore = stream->uncore;

	intel_uncore_rmw(uncore, GEN6_UCGCTL1,
			 GEN6_CSUNIT_CLOCK_GATE_DISABLE, 0);
	intel_uncore_rmw(uncore, GEN7_MISCCPCTL,
			 0, GEN7_DOP_CLOCK_GATE_ENABLE);

	intel_uncore_rmw(uncore, GDT_CHICKEN_BITS, GT_NOA_ENABLE, 0);
}

static u32 oa_config_flex_reg(const struct i915_oa_config *oa_config,
			      i915_reg_t reg)
{
	u32 mmio = i915_mmio_reg_offset(reg);
	int i;

	 
	if (!oa_config)
		return 0;

	for (i = 0; i < oa_config->flex_regs_len; i++) {
		if (i915_mmio_reg_offset(oa_config->flex_regs[i].addr) == mmio)
			return oa_config->flex_regs[i].value;
	}

	return 0;
}
 
static void
gen8_update_reg_state_unlocked(const struct intel_context *ce,
			       const struct i915_perf_stream *stream)
{
	u32 ctx_oactxctrl = stream->perf->ctx_oactxctrl_offset;
	u32 ctx_flexeu0 = stream->perf->ctx_flexeu0_offset;
	 
	static const i915_reg_t flex_regs[] = {
		EU_PERF_CNTL0,
		EU_PERF_CNTL1,
		EU_PERF_CNTL2,
		EU_PERF_CNTL3,
		EU_PERF_CNTL4,
		EU_PERF_CNTL5,
		EU_PERF_CNTL6,
	};
	u32 *reg_state = ce->lrc_reg_state;
	int i;

	reg_state[ctx_oactxctrl + 1] =
		(stream->period_exponent << GEN8_OA_TIMER_PERIOD_SHIFT) |
		(stream->periodic ? GEN8_OA_TIMER_ENABLE : 0) |
		GEN8_OA_COUNTER_RESUME;

	for (i = 0; i < ARRAY_SIZE(flex_regs); i++)
		reg_state[ctx_flexeu0 + i * 2 + 1] =
			oa_config_flex_reg(stream->oa_config, flex_regs[i]);
}

struct flex {
	i915_reg_t reg;
	u32 offset;
	u32 value;
};

static int
gen8_store_flex(struct i915_request *rq,
		struct intel_context *ce,
		const struct flex *flex, unsigned int count)
{
	u32 offset;
	u32 *cs;

	cs = intel_ring_begin(rq, 4 * count);
	if (IS_ERR(cs))
		return PTR_ERR(cs);

	offset = i915_ggtt_offset(ce->state) + LRC_STATE_OFFSET;
	do {
		*cs++ = MI_STORE_DWORD_IMM_GEN4 | MI_USE_GGTT;
		*cs++ = offset + flex->offset * sizeof(u32);
		*cs++ = 0;
		*cs++ = flex->value;
	} while (flex++, --count);

	intel_ring_advance(rq, cs);

	return 0;
}

static int
gen8_load_flex(struct i915_request *rq,
	       struct intel_context *ce,
	       const struct flex *flex, unsigned int count)
{
	u32 *cs;

	GEM_BUG_ON(!count || count > 63);

	cs = intel_ring_begin(rq, 2 * count + 2);
	if (IS_ERR(cs))
		return PTR_ERR(cs);

	*cs++ = MI_LOAD_REGISTER_IMM(count);
	do {
		*cs++ = i915_mmio_reg_offset(flex->reg);
		*cs++ = flex->value;
	} while (flex++, --count);
	*cs++ = MI_NOOP;

	intel_ring_advance(rq, cs);

	return 0;
}

static int gen8_modify_context(struct intel_context *ce,
			       const struct flex *flex, unsigned int count)
{
	struct i915_request *rq;
	int err;

	rq = intel_engine_create_kernel_request(ce->engine);
	if (IS_ERR(rq))
		return PTR_ERR(rq);

	 
	err = intel_context_prepare_remote_request(ce, rq);
	if (err == 0)
		err = gen8_store_flex(rq, ce, flex, count);

	i915_request_add(rq);
	return err;
}

static int
gen8_modify_self(struct intel_context *ce,
		 const struct flex *flex, unsigned int count,
		 struct i915_active *active)
{
	struct i915_request *rq;
	int err;

	intel_engine_pm_get(ce->engine);
	rq = i915_request_create(ce);
	intel_engine_pm_put(ce->engine);
	if (IS_ERR(rq))
		return PTR_ERR(rq);

	if (!IS_ERR_OR_NULL(active)) {
		err = i915_active_add_request(active, rq);
		if (err)
			goto err_add_request;
	}

	err = gen8_load_flex(rq, ce, flex, count);
	if (err)
		goto err_add_request;

err_add_request:
	i915_request_add(rq);
	return err;
}

static int gen8_configure_context(struct i915_perf_stream *stream,
				  struct i915_gem_context *ctx,
				  struct flex *flex, unsigned int count)
{
	struct i915_gem_engines_iter it;
	struct intel_context *ce;
	int err = 0;

	for_each_gem_engine(ce, i915_gem_context_lock_engines(ctx), it) {
		GEM_BUG_ON(ce == ce->engine->kernel_context);

		if (ce->engine->class != RENDER_CLASS)
			continue;

		 
		if (!intel_context_pin_if_active(ce))
			continue;

		flex->value = intel_sseu_make_rpcs(ce->engine->gt, &ce->sseu);
		err = gen8_modify_context(ce, flex, count);

		intel_context_unpin(ce);
		if (err)
			break;
	}
	i915_gem_context_unlock_engines(ctx);

	return err;
}

static int gen12_configure_oar_context(struct i915_perf_stream *stream,
				       struct i915_active *active)
{
	int err;
	struct intel_context *ce = stream->pinned_ctx;
	u32 format = stream->oa_buffer.format->format;
	u32 offset = stream->perf->ctx_oactxctrl_offset;
	struct flex regs_context[] = {
		{
			GEN8_OACTXCONTROL,
			offset + 1,
			active ? GEN8_OA_COUNTER_RESUME : 0,
		},
	};
	 
#define GEN12_OAR_OACONTROL_OFFSET 0x5B0
	struct flex regs_lri[] = {
		{
			GEN12_OAR_OACONTROL,
			GEN12_OAR_OACONTROL_OFFSET + 1,
			(format << GEN12_OAR_OACONTROL_COUNTER_FORMAT_SHIFT) |
			(active ? GEN12_OAR_OACONTROL_COUNTER_ENABLE : 0)
		},
		{
			RING_CONTEXT_CONTROL(ce->engine->mmio_base),
			CTX_CONTEXT_CONTROL,
			_MASKED_FIELD(GEN12_CTX_CTRL_OAR_CONTEXT_ENABLE,
				      active ?
				      GEN12_CTX_CTRL_OAR_CONTEXT_ENABLE :
				      0)
		},
	};

	 
	err = intel_context_lock_pinned(ce);
	if (err)
		return err;

	err = gen8_modify_context(ce, regs_context,
				  ARRAY_SIZE(regs_context));
	intel_context_unlock_pinned(ce);
	if (err)
		return err;

	 
	return gen8_modify_self(ce, regs_lri, ARRAY_SIZE(regs_lri), active);
}

 
static int
oa_configure_all_contexts(struct i915_perf_stream *stream,
			  struct flex *regs,
			  size_t num_regs,
			  struct i915_active *active)
{
	struct drm_i915_private *i915 = stream->perf->i915;
	struct intel_engine_cs *engine;
	struct intel_gt *gt = stream->engine->gt;
	struct i915_gem_context *ctx, *cn;
	int err;

	lockdep_assert_held(&gt->perf.lock);

	 
	spin_lock(&i915->gem.contexts.lock);
	list_for_each_entry_safe(ctx, cn, &i915->gem.contexts.list, link) {
		if (!kref_get_unless_zero(&ctx->ref))
			continue;

		spin_unlock(&i915->gem.contexts.lock);

		err = gen8_configure_context(stream, ctx, regs, num_regs);
		if (err) {
			i915_gem_context_put(ctx);
			return err;
		}

		spin_lock(&i915->gem.contexts.lock);
		list_safe_reset_next(ctx, cn, link);
		i915_gem_context_put(ctx);
	}
	spin_unlock(&i915->gem.contexts.lock);

	 
	for_each_uabi_engine(engine, i915) {
		struct intel_context *ce = engine->kernel_context;

		if (engine->class != RENDER_CLASS)
			continue;

		regs[0].value = intel_sseu_make_rpcs(engine->gt, &ce->sseu);

		err = gen8_modify_self(ce, regs, num_regs, active);
		if (err)
			return err;
	}

	return 0;
}

static int
gen12_configure_all_contexts(struct i915_perf_stream *stream,
			     const struct i915_oa_config *oa_config,
			     struct i915_active *active)
{
	struct flex regs[] = {
		{
			GEN8_R_PWR_CLK_STATE(RENDER_RING_BASE),
			CTX_R_PWR_CLK_STATE,
		},
	};

	if (stream->engine->class != RENDER_CLASS)
		return 0;

	return oa_configure_all_contexts(stream,
					 regs, ARRAY_SIZE(regs),
					 active);
}

static int
lrc_configure_all_contexts(struct i915_perf_stream *stream,
			   const struct i915_oa_config *oa_config,
			   struct i915_active *active)
{
	u32 ctx_oactxctrl = stream->perf->ctx_oactxctrl_offset;
	 
	const u32 ctx_flexeu0 = stream->perf->ctx_flexeu0_offset;
#define ctx_flexeuN(N) (ctx_flexeu0 + 2 * (N) + 1)
	struct flex regs[] = {
		{
			GEN8_R_PWR_CLK_STATE(RENDER_RING_BASE),
			CTX_R_PWR_CLK_STATE,
		},
		{
			GEN8_OACTXCONTROL,
			ctx_oactxctrl + 1,
		},
		{ EU_PERF_CNTL0, ctx_flexeuN(0) },
		{ EU_PERF_CNTL1, ctx_flexeuN(1) },
		{ EU_PERF_CNTL2, ctx_flexeuN(2) },
		{ EU_PERF_CNTL3, ctx_flexeuN(3) },
		{ EU_PERF_CNTL4, ctx_flexeuN(4) },
		{ EU_PERF_CNTL5, ctx_flexeuN(5) },
		{ EU_PERF_CNTL6, ctx_flexeuN(6) },
	};
#undef ctx_flexeuN
	int i;

	regs[1].value =
		(stream->period_exponent << GEN8_OA_TIMER_PERIOD_SHIFT) |
		(stream->periodic ? GEN8_OA_TIMER_ENABLE : 0) |
		GEN8_OA_COUNTER_RESUME;

	for (i = 2; i < ARRAY_SIZE(regs); i++)
		regs[i].value = oa_config_flex_reg(oa_config, regs[i].reg);

	return oa_configure_all_contexts(stream,
					 regs, ARRAY_SIZE(regs),
					 active);
}

static int
gen8_enable_metric_set(struct i915_perf_stream *stream,
		       struct i915_active *active)
{
	struct intel_uncore *uncore = stream->uncore;
	struct i915_oa_config *oa_config = stream->oa_config;
	int ret;

	 
	if (IS_GRAPHICS_VER(stream->perf->i915, 9, 11)) {
		intel_uncore_write(uncore, GEN8_OA_DEBUG,
				   _MASKED_BIT_ENABLE(GEN9_OA_DEBUG_DISABLE_CLK_RATIO_REPORTS |
						      GEN9_OA_DEBUG_INCLUDE_CLK_RATIO));
	}

	 
	ret = lrc_configure_all_contexts(stream, oa_config, active);
	if (ret)
		return ret;

	return emit_oa_config(stream,
			      stream->oa_config, oa_context(stream),
			      active);
}

static u32 oag_report_ctx_switches(const struct i915_perf_stream *stream)
{
	return _MASKED_FIELD(GEN12_OAG_OA_DEBUG_DISABLE_CTX_SWITCH_REPORTS,
			     (stream->sample_flags & SAMPLE_OA_REPORT) ?
			     0 : GEN12_OAG_OA_DEBUG_DISABLE_CTX_SWITCH_REPORTS);
}

static int
gen12_enable_metric_set(struct i915_perf_stream *stream,
			struct i915_active *active)
{
	struct drm_i915_private *i915 = stream->perf->i915;
	struct intel_uncore *uncore = stream->uncore;
	struct i915_oa_config *oa_config = stream->oa_config;
	bool periodic = stream->periodic;
	u32 period_exponent = stream->period_exponent;
	u32 sqcnt1;
	int ret;

	 
	if (IS_XEHPSDV(i915) || IS_DG2(i915)) {
		intel_gt_mcr_multicast_write(uncore->gt, GEN8_ROW_CHICKEN,
					     _MASKED_BIT_ENABLE(STALL_DOP_GATING_DISABLE));
		intel_uncore_write(uncore, GEN7_ROW_CHICKEN2,
				   _MASKED_BIT_ENABLE(GEN12_DISABLE_DOP_GATING));
	}

	intel_uncore_write(uncore, __oa_regs(stream)->oa_debug,
			    
			   _MASKED_BIT_ENABLE(GEN12_OAG_OA_DEBUG_DISABLE_CLK_RATIO_REPORTS |
					      GEN12_OAG_OA_DEBUG_INCLUDE_CLK_RATIO) |
			    
			   oag_report_ctx_switches(stream));

	intel_uncore_write(uncore, __oa_regs(stream)->oa_ctx_ctrl, periodic ?
			   (GEN12_OAG_OAGLBCTXCTRL_COUNTER_RESUME |
			    GEN12_OAG_OAGLBCTXCTRL_TIMER_ENABLE |
			    (period_exponent << GEN12_OAG_OAGLBCTXCTRL_TIMER_PERIOD_SHIFT))
			    : 0);

	 
	sqcnt1 = GEN12_SQCNT1_PMON_ENABLE |
		 (HAS_OA_BPC_REPORTING(i915) ? GEN12_SQCNT1_OABPC : 0);

	intel_uncore_rmw(uncore, GEN12_SQCNT1, 0, sqcnt1);

	 
	ret = gen12_configure_all_contexts(stream, oa_config, active);
	if (ret)
		return ret;

	 
	if (stream->ctx) {
		ret = gen12_configure_oar_context(stream, active);
		if (ret)
			return ret;
	}

	return emit_oa_config(stream,
			      stream->oa_config, oa_context(stream),
			      active);
}

static void gen8_disable_metric_set(struct i915_perf_stream *stream)
{
	struct intel_uncore *uncore = stream->uncore;

	 
	lrc_configure_all_contexts(stream, NULL, NULL);

	intel_uncore_rmw(uncore, GDT_CHICKEN_BITS, GT_NOA_ENABLE, 0);
}

static void gen11_disable_metric_set(struct i915_perf_stream *stream)
{
	struct intel_uncore *uncore = stream->uncore;

	 
	lrc_configure_all_contexts(stream, NULL, NULL);

	 
	intel_uncore_rmw(uncore, RPM_CONFIG1, GEN10_GT_NOA_ENABLE, 0);
}

static void gen12_disable_metric_set(struct i915_perf_stream *stream)
{
	struct intel_uncore *uncore = stream->uncore;
	struct drm_i915_private *i915 = stream->perf->i915;
	u32 sqcnt1;

	 
	if (IS_XEHPSDV(i915) || IS_DG2(i915)) {
		intel_gt_mcr_multicast_write(uncore->gt, GEN8_ROW_CHICKEN,
					     _MASKED_BIT_DISABLE(STALL_DOP_GATING_DISABLE));
		intel_uncore_write(uncore, GEN7_ROW_CHICKEN2,
				   _MASKED_BIT_DISABLE(GEN12_DISABLE_DOP_GATING));
	}

	 
	gen12_configure_all_contexts(stream, NULL, NULL);

	 
	if (stream->ctx)
		gen12_configure_oar_context(stream, NULL);

	 
	intel_uncore_rmw(uncore, RPM_CONFIG1, GEN10_GT_NOA_ENABLE, 0);

	sqcnt1 = GEN12_SQCNT1_PMON_ENABLE |
		 (HAS_OA_BPC_REPORTING(i915) ? GEN12_SQCNT1_OABPC : 0);

	 
	intel_uncore_rmw(uncore, GEN12_SQCNT1, sqcnt1, 0);
}

static void gen7_oa_enable(struct i915_perf_stream *stream)
{
	struct intel_uncore *uncore = stream->uncore;
	struct i915_gem_context *ctx = stream->ctx;
	u32 ctx_id = stream->specific_ctx_id;
	bool periodic = stream->periodic;
	u32 period_exponent = stream->period_exponent;
	u32 report_format = stream->oa_buffer.format->format;

	 
	gen7_init_oa_buffer(stream);

	intel_uncore_write(uncore, GEN7_OACONTROL,
			   (ctx_id & GEN7_OACONTROL_CTX_MASK) |
			   (period_exponent <<
			    GEN7_OACONTROL_TIMER_PERIOD_SHIFT) |
			   (periodic ? GEN7_OACONTROL_TIMER_ENABLE : 0) |
			   (report_format << GEN7_OACONTROL_FORMAT_SHIFT) |
			   (ctx ? GEN7_OACONTROL_PER_CTX_ENABLE : 0) |
			   GEN7_OACONTROL_ENABLE);
}

static void gen8_oa_enable(struct i915_perf_stream *stream)
{
	struct intel_uncore *uncore = stream->uncore;
	u32 report_format = stream->oa_buffer.format->format;

	 
	gen8_init_oa_buffer(stream);

	 
	intel_uncore_write(uncore, GEN8_OACONTROL,
			   (report_format << GEN8_OA_REPORT_FORMAT_SHIFT) |
			   GEN8_OA_COUNTER_ENABLE);
}

static void gen12_oa_enable(struct i915_perf_stream *stream)
{
	const struct i915_perf_regs *regs;
	u32 val;

	 
	if (!(stream->sample_flags & SAMPLE_OA_REPORT))
		return;

	gen12_init_oa_buffer(stream);

	regs = __oa_regs(stream);
	val = (stream->oa_buffer.format->format << regs->oa_ctrl_counter_format_shift) |
	      GEN12_OAG_OACONTROL_OA_COUNTER_ENABLE;

	intel_uncore_write(stream->uncore, regs->oa_ctrl, val);
}

 
static void i915_oa_stream_enable(struct i915_perf_stream *stream)
{
	stream->pollin = false;

	stream->perf->ops.oa_enable(stream);

	if (stream->sample_flags & SAMPLE_OA_REPORT)
		hrtimer_start(&stream->poll_check_timer,
			      ns_to_ktime(stream->poll_oa_period),
			      HRTIMER_MODE_REL_PINNED);
}

static void gen7_oa_disable(struct i915_perf_stream *stream)
{
	struct intel_uncore *uncore = stream->uncore;

	intel_uncore_write(uncore, GEN7_OACONTROL, 0);
	if (intel_wait_for_register(uncore,
				    GEN7_OACONTROL, GEN7_OACONTROL_ENABLE, 0,
				    50))
		drm_err(&stream->perf->i915->drm,
			"wait for OA to be disabled timed out\n");
}

static void gen8_oa_disable(struct i915_perf_stream *stream)
{
	struct intel_uncore *uncore = stream->uncore;

	intel_uncore_write(uncore, GEN8_OACONTROL, 0);
	if (intel_wait_for_register(uncore,
				    GEN8_OACONTROL, GEN8_OA_COUNTER_ENABLE, 0,
				    50))
		drm_err(&stream->perf->i915->drm,
			"wait for OA to be disabled timed out\n");
}

static void gen12_oa_disable(struct i915_perf_stream *stream)
{
	struct intel_uncore *uncore = stream->uncore;

	intel_uncore_write(uncore, __oa_regs(stream)->oa_ctrl, 0);
	if (intel_wait_for_register(uncore,
				    __oa_regs(stream)->oa_ctrl,
				    GEN12_OAG_OACONTROL_OA_COUNTER_ENABLE, 0,
				    50))
		drm_err(&stream->perf->i915->drm,
			"wait for OA to be disabled timed out\n");

	intel_uncore_write(uncore, GEN12_OA_TLB_INV_CR, 1);
	if (intel_wait_for_register(uncore,
				    GEN12_OA_TLB_INV_CR,
				    1, 0,
				    50))
		drm_err(&stream->perf->i915->drm,
			"wait for OA tlb invalidate timed out\n");
}

 
static void i915_oa_stream_disable(struct i915_perf_stream *stream)
{
	stream->perf->ops.oa_disable(stream);

	if (stream->sample_flags & SAMPLE_OA_REPORT)
		hrtimer_cancel(&stream->poll_check_timer);
}

static const struct i915_perf_stream_ops i915_oa_stream_ops = {
	.destroy = i915_oa_stream_destroy,
	.enable = i915_oa_stream_enable,
	.disable = i915_oa_stream_disable,
	.wait_unlocked = i915_oa_wait_unlocked,
	.poll_wait = i915_oa_poll_wait,
	.read = i915_oa_read,
};

static int i915_perf_stream_enable_sync(struct i915_perf_stream *stream)
{
	struct i915_active *active;
	int err;

	active = i915_active_create();
	if (!active)
		return -ENOMEM;

	err = stream->perf->ops.enable_metric_set(stream, active);
	if (err == 0)
		__i915_active_wait(active, TASK_UNINTERRUPTIBLE);

	i915_active_put(active);
	return err;
}

static void
get_default_sseu_config(struct intel_sseu *out_sseu,
			struct intel_engine_cs *engine)
{
	const struct sseu_dev_info *devinfo_sseu = &engine->gt->info.sseu;

	*out_sseu = intel_sseu_from_device_info(devinfo_sseu);

	if (GRAPHICS_VER(engine->i915) == 11) {
		 
		out_sseu->subslice_mask =
			~(~0 << (hweight8(out_sseu->subslice_mask) / 2));
		out_sseu->slice_mask = 0x1;
	}
}

static int
get_sseu_config(struct intel_sseu *out_sseu,
		struct intel_engine_cs *engine,
		const struct drm_i915_gem_context_param_sseu *drm_sseu)
{
	if (drm_sseu->engine.engine_class != engine->uabi_class ||
	    drm_sseu->engine.engine_instance != engine->uabi_instance)
		return -EINVAL;

	return i915_gem_user_to_context_sseu(engine->gt, drm_sseu, out_sseu);
}

 
u32 i915_perf_oa_timestamp_frequency(struct drm_i915_private *i915)
{
	 
	if (IS_DG2(i915) || IS_METEORLAKE(i915)) {
		intel_wakeref_t wakeref;
		u32 reg, shift;

		with_intel_runtime_pm(to_gt(i915)->uncore->rpm, wakeref)
			reg = intel_uncore_read(to_gt(i915)->uncore, RPM_CONFIG0);

		shift = REG_FIELD_GET(GEN10_RPM_CONFIG0_CTC_SHIFT_PARAMETER_MASK,
				      reg);

		return to_gt(i915)->clock_frequency << (3 - shift);
	}

	return to_gt(i915)->clock_frequency;
}

 
static int i915_oa_stream_init(struct i915_perf_stream *stream,
			       struct drm_i915_perf_open_param *param,
			       struct perf_open_properties *props)
{
	struct drm_i915_private *i915 = stream->perf->i915;
	struct i915_perf *perf = stream->perf;
	struct i915_perf_group *g;
	struct intel_gt *gt;
	int ret;

	if (!props->engine) {
		drm_dbg(&stream->perf->i915->drm,
			"OA engine not specified\n");
		return -EINVAL;
	}
	gt = props->engine->gt;
	g = props->engine->oa_group;

	 
	if (!perf->metrics_kobj) {
		drm_dbg(&stream->perf->i915->drm,
			"OA metrics weren't advertised via sysfs\n");
		return -EINVAL;
	}

	if (!(props->sample_flags & SAMPLE_OA_REPORT) &&
	    (GRAPHICS_VER(perf->i915) < 12 || !stream->ctx)) {
		drm_dbg(&stream->perf->i915->drm,
			"Only OA report sampling supported\n");
		return -EINVAL;
	}

	if (!perf->ops.enable_metric_set) {
		drm_dbg(&stream->perf->i915->drm,
			"OA unit not supported\n");
		return -ENODEV;
	}

	 
	if (g->exclusive_stream) {
		drm_dbg(&stream->perf->i915->drm,
			"OA unit already in use\n");
		return -EBUSY;
	}

	if (!props->oa_format) {
		drm_dbg(&stream->perf->i915->drm,
			"OA report format not specified\n");
		return -EINVAL;
	}

	stream->engine = props->engine;
	stream->uncore = stream->engine->gt->uncore;

	stream->sample_size = sizeof(struct drm_i915_perf_record_header);

	stream->oa_buffer.format = &perf->oa_formats[props->oa_format];
	if (drm_WARN_ON(&i915->drm, stream->oa_buffer.format->size == 0))
		return -EINVAL;

	stream->sample_flags = props->sample_flags;
	stream->sample_size += stream->oa_buffer.format->size;

	stream->hold_preemption = props->hold_preemption;

	stream->periodic = props->oa_periodic;
	if (stream->periodic)
		stream->period_exponent = props->oa_period_exponent;

	if (stream->ctx) {
		ret = oa_get_render_ctx_id(stream);
		if (ret) {
			drm_dbg(&stream->perf->i915->drm,
				"Invalid context id to filter with\n");
			return ret;
		}
	}

	ret = alloc_noa_wait(stream);
	if (ret) {
		drm_dbg(&stream->perf->i915->drm,
			"Unable to allocate NOA wait batch buffer\n");
		goto err_noa_wait_alloc;
	}

	stream->oa_config = i915_perf_get_oa_config(perf, props->metrics_set);
	if (!stream->oa_config) {
		drm_dbg(&stream->perf->i915->drm,
			"Invalid OA config id=%i\n", props->metrics_set);
		ret = -EINVAL;
		goto err_config;
	}

	 
	intel_engine_pm_get(stream->engine);
	intel_uncore_forcewake_get(stream->uncore, FORCEWAKE_ALL);

	 
	if (intel_uc_uses_guc_rc(&gt->uc) &&
	    (IS_DG2_GRAPHICS_STEP(gt->i915, G10, STEP_A0, STEP_C0) ||
	     IS_DG2_GRAPHICS_STEP(gt->i915, G11, STEP_A0, STEP_B0))) {
		ret = intel_guc_slpc_override_gucrc_mode(&gt->uc.guc.slpc,
							 SLPC_GUCRC_MODE_GUCRC_NO_RC6);
		if (ret) {
			drm_dbg(&stream->perf->i915->drm,
				"Unable to override gucrc mode\n");
			goto err_gucrc;
		}

		stream->override_gucrc = true;
	}

	ret = alloc_oa_buffer(stream);
	if (ret)
		goto err_oa_buf_alloc;

	stream->ops = &i915_oa_stream_ops;

	stream->engine->gt->perf.sseu = props->sseu;
	WRITE_ONCE(g->exclusive_stream, stream);

	ret = i915_perf_stream_enable_sync(stream);
	if (ret) {
		drm_dbg(&stream->perf->i915->drm,
			"Unable to enable metric set\n");
		goto err_enable;
	}

	drm_dbg(&stream->perf->i915->drm,
		"opening stream oa config uuid=%s\n",
		  stream->oa_config->uuid);

	hrtimer_init(&stream->poll_check_timer,
		     CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	stream->poll_check_timer.function = oa_poll_check_timer_cb;
	init_waitqueue_head(&stream->poll_wq);
	spin_lock_init(&stream->oa_buffer.ptr_lock);
	mutex_init(&stream->lock);

	return 0;

err_enable:
	WRITE_ONCE(g->exclusive_stream, NULL);
	perf->ops.disable_metric_set(stream);

	free_oa_buffer(stream);

err_oa_buf_alloc:
	if (stream->override_gucrc)
		intel_guc_slpc_unset_gucrc_mode(&gt->uc.guc.slpc);

err_gucrc:
	intel_uncore_forcewake_put(stream->uncore, FORCEWAKE_ALL);
	intel_engine_pm_put(stream->engine);

	free_oa_configs(stream);

err_config:
	free_noa_wait(stream);

err_noa_wait_alloc:
	if (stream->ctx)
		oa_put_render_ctx_id(stream);

	return ret;
}

void i915_oa_init_reg_state(const struct intel_context *ce,
			    const struct intel_engine_cs *engine)
{
	struct i915_perf_stream *stream;

	if (engine->class != RENDER_CLASS)
		return;

	 
	stream = READ_ONCE(engine->oa_group->exclusive_stream);
	if (stream && GRAPHICS_VER(stream->perf->i915) < 12)
		gen8_update_reg_state_unlocked(ce, stream);
}

 
static ssize_t i915_perf_read(struct file *file,
			      char __user *buf,
			      size_t count,
			      loff_t *ppos)
{
	struct i915_perf_stream *stream = file->private_data;
	size_t offset = 0;
	int ret;

	 
	if (!stream->enabled || !(stream->sample_flags & SAMPLE_OA_REPORT))
		return -EIO;

	if (!(file->f_flags & O_NONBLOCK)) {
		 
		do {
			ret = stream->ops->wait_unlocked(stream);
			if (ret)
				return ret;

			mutex_lock(&stream->lock);
			ret = stream->ops->read(stream, buf, count, &offset);
			mutex_unlock(&stream->lock);
		} while (!offset && !ret);
	} else {
		mutex_lock(&stream->lock);
		ret = stream->ops->read(stream, buf, count, &offset);
		mutex_unlock(&stream->lock);
	}

	 
	if (ret != -ENOSPC)
		stream->pollin = false;

	 
	return offset ?: (ret ?: -EAGAIN);
}

static enum hrtimer_restart oa_poll_check_timer_cb(struct hrtimer *hrtimer)
{
	struct i915_perf_stream *stream =
		container_of(hrtimer, typeof(*stream), poll_check_timer);

	if (oa_buffer_check_unlocked(stream)) {
		stream->pollin = true;
		wake_up(&stream->poll_wq);
	}

	hrtimer_forward_now(hrtimer,
			    ns_to_ktime(stream->poll_oa_period));

	return HRTIMER_RESTART;
}

 
static __poll_t i915_perf_poll_locked(struct i915_perf_stream *stream,
				      struct file *file,
				      poll_table *wait)
{
	__poll_t events = 0;

	stream->ops->poll_wait(stream, file, wait);

	 
	if (stream->pollin)
		events |= EPOLLIN;

	return events;
}

 
static __poll_t i915_perf_poll(struct file *file, poll_table *wait)
{
	struct i915_perf_stream *stream = file->private_data;
	__poll_t ret;

	mutex_lock(&stream->lock);
	ret = i915_perf_poll_locked(stream, file, wait);
	mutex_unlock(&stream->lock);

	return ret;
}

 
static void i915_perf_enable_locked(struct i915_perf_stream *stream)
{
	if (stream->enabled)
		return;

	 
	stream->enabled = true;

	if (stream->ops->enable)
		stream->ops->enable(stream);

	if (stream->hold_preemption)
		intel_context_set_nopreempt(stream->pinned_ctx);
}

 
static void i915_perf_disable_locked(struct i915_perf_stream *stream)
{
	if (!stream->enabled)
		return;

	 
	stream->enabled = false;

	if (stream->hold_preemption)
		intel_context_clear_nopreempt(stream->pinned_ctx);

	if (stream->ops->disable)
		stream->ops->disable(stream);
}

static long i915_perf_config_locked(struct i915_perf_stream *stream,
				    unsigned long metrics_set)
{
	struct i915_oa_config *config;
	long ret = stream->oa_config->id;

	config = i915_perf_get_oa_config(stream->perf, metrics_set);
	if (!config)
		return -EINVAL;

	if (config != stream->oa_config) {
		int err;

		 
		err = emit_oa_config(stream, config, oa_context(stream), NULL);
		if (!err)
			config = xchg(&stream->oa_config, config);
		else
			ret = err;
	}

	i915_oa_config_put(config);

	return ret;
}

 
static long i915_perf_ioctl_locked(struct i915_perf_stream *stream,
				   unsigned int cmd,
				   unsigned long arg)
{
	switch (cmd) {
	case I915_PERF_IOCTL_ENABLE:
		i915_perf_enable_locked(stream);
		return 0;
	case I915_PERF_IOCTL_DISABLE:
		i915_perf_disable_locked(stream);
		return 0;
	case I915_PERF_IOCTL_CONFIG:
		return i915_perf_config_locked(stream, arg);
	}

	return -EINVAL;
}

 
static long i915_perf_ioctl(struct file *file,
			    unsigned int cmd,
			    unsigned long arg)
{
	struct i915_perf_stream *stream = file->private_data;
	long ret;

	mutex_lock(&stream->lock);
	ret = i915_perf_ioctl_locked(stream, cmd, arg);
	mutex_unlock(&stream->lock);

	return ret;
}

 
static void i915_perf_destroy_locked(struct i915_perf_stream *stream)
{
	if (stream->enabled)
		i915_perf_disable_locked(stream);

	if (stream->ops->destroy)
		stream->ops->destroy(stream);

	if (stream->ctx)
		i915_gem_context_put(stream->ctx);

	kfree(stream);
}

 
static int i915_perf_release(struct inode *inode, struct file *file)
{
	struct i915_perf_stream *stream = file->private_data;
	struct i915_perf *perf = stream->perf;
	struct intel_gt *gt = stream->engine->gt;

	 
	mutex_lock(&gt->perf.lock);
	i915_perf_destroy_locked(stream);
	mutex_unlock(&gt->perf.lock);

	 
	drm_dev_put(&perf->i915->drm);

	return 0;
}


static const struct file_operations fops = {
	.owner		= THIS_MODULE,
	.llseek		= no_llseek,
	.release	= i915_perf_release,
	.poll		= i915_perf_poll,
	.read		= i915_perf_read,
	.unlocked_ioctl	= i915_perf_ioctl,
	 
	.compat_ioctl   = i915_perf_ioctl,
};


 
static int
i915_perf_open_ioctl_locked(struct i915_perf *perf,
			    struct drm_i915_perf_open_param *param,
			    struct perf_open_properties *props,
			    struct drm_file *file)
{
	struct i915_gem_context *specific_ctx = NULL;
	struct i915_perf_stream *stream = NULL;
	unsigned long f_flags = 0;
	bool privileged_op = true;
	int stream_fd;
	int ret;

	if (props->single_context) {
		u32 ctx_handle = props->ctx_handle;
		struct drm_i915_file_private *file_priv = file->driver_priv;

		specific_ctx = i915_gem_context_lookup(file_priv, ctx_handle);
		if (IS_ERR(specific_ctx)) {
			drm_dbg(&perf->i915->drm,
				"Failed to look up context with ID %u for opening perf stream\n",
				  ctx_handle);
			ret = PTR_ERR(specific_ctx);
			goto err;
		}
	}

	 
	if (IS_HASWELL(perf->i915) && specific_ctx)
		privileged_op = false;
	else if (GRAPHICS_VER(perf->i915) == 12 && specific_ctx &&
		 (props->sample_flags & SAMPLE_OA_REPORT) == 0)
		privileged_op = false;

	if (props->hold_preemption) {
		if (!props->single_context) {
			drm_dbg(&perf->i915->drm,
				"preemption disable with no context\n");
			ret = -EINVAL;
			goto err;
		}
		privileged_op = true;
	}

	 
	if (props->has_sseu)
		privileged_op = true;
	else
		get_default_sseu_config(&props->sseu, props->engine);

	 
	if (privileged_op &&
	    i915_perf_stream_paranoid && !perfmon_capable()) {
		drm_dbg(&perf->i915->drm,
			"Insufficient privileges to open i915 perf stream\n");
		ret = -EACCES;
		goto err_ctx;
	}

	stream = kzalloc(sizeof(*stream), GFP_KERNEL);
	if (!stream) {
		ret = -ENOMEM;
		goto err_ctx;
	}

	stream->perf = perf;
	stream->ctx = specific_ctx;
	stream->poll_oa_period = props->poll_oa_period;

	ret = i915_oa_stream_init(stream, param, props);
	if (ret)
		goto err_alloc;

	 
	if (WARN_ON(stream->sample_flags != props->sample_flags)) {
		ret = -ENODEV;
		goto err_flags;
	}

	if (param->flags & I915_PERF_FLAG_FD_CLOEXEC)
		f_flags |= O_CLOEXEC;
	if (param->flags & I915_PERF_FLAG_FD_NONBLOCK)
		f_flags |= O_NONBLOCK;

	stream_fd = anon_inode_getfd("[i915_perf]", &fops, stream, f_flags);
	if (stream_fd < 0) {
		ret = stream_fd;
		goto err_flags;
	}

	if (!(param->flags & I915_PERF_FLAG_DISABLED))
		i915_perf_enable_locked(stream);

	 
	drm_dev_get(&perf->i915->drm);

	return stream_fd;

err_flags:
	if (stream->ops->destroy)
		stream->ops->destroy(stream);
err_alloc:
	kfree(stream);
err_ctx:
	if (specific_ctx)
		i915_gem_context_put(specific_ctx);
err:
	return ret;
}

static u64 oa_exponent_to_ns(struct i915_perf *perf, int exponent)
{
	u64 nom = (2ULL << exponent) * NSEC_PER_SEC;
	u32 den = i915_perf_oa_timestamp_frequency(perf->i915);

	return div_u64(nom + den - 1, den);
}

static __always_inline bool
oa_format_valid(struct i915_perf *perf, enum drm_i915_oa_format format)
{
	return test_bit(format, perf->format_mask);
}

static __always_inline void
oa_format_add(struct i915_perf *perf, enum drm_i915_oa_format format)
{
	__set_bit(format, perf->format_mask);
}

 
static int read_properties_unlocked(struct i915_perf *perf,
				    u64 __user *uprops,
				    u32 n_props,
				    struct perf_open_properties *props)
{
	struct drm_i915_gem_context_param_sseu user_sseu;
	const struct i915_oa_format *f;
	u64 __user *uprop = uprops;
	bool config_instance = false;
	bool config_class = false;
	bool config_sseu = false;
	u8 class, instance;
	u32 i;
	int ret;

	memset(props, 0, sizeof(struct perf_open_properties));
	props->poll_oa_period = DEFAULT_POLL_PERIOD_NS;

	 
	if (!n_props || n_props >= DRM_I915_PERF_PROP_MAX) {
		drm_dbg(&perf->i915->drm,
			"Invalid number of i915 perf properties given\n");
		return -EINVAL;
	}

	 
	class = I915_ENGINE_CLASS_RENDER;
	instance = 0;

	for (i = 0; i < n_props; i++) {
		u64 oa_period, oa_freq_hz;
		u64 id, value;

		ret = get_user(id, uprop);
		if (ret)
			return ret;

		ret = get_user(value, uprop + 1);
		if (ret)
			return ret;

		if (id == 0 || id >= DRM_I915_PERF_PROP_MAX) {
			drm_dbg(&perf->i915->drm,
				"Unknown i915 perf property ID\n");
			return -EINVAL;
		}

		switch ((enum drm_i915_perf_property_id)id) {
		case DRM_I915_PERF_PROP_CTX_HANDLE:
			props->single_context = 1;
			props->ctx_handle = value;
			break;
		case DRM_I915_PERF_PROP_SAMPLE_OA:
			if (value)
				props->sample_flags |= SAMPLE_OA_REPORT;
			break;
		case DRM_I915_PERF_PROP_OA_METRICS_SET:
			if (value == 0) {
				drm_dbg(&perf->i915->drm,
					"Unknown OA metric set ID\n");
				return -EINVAL;
			}
			props->metrics_set = value;
			break;
		case DRM_I915_PERF_PROP_OA_FORMAT:
			if (value == 0 || value >= I915_OA_FORMAT_MAX) {
				drm_dbg(&perf->i915->drm,
					"Out-of-range OA report format %llu\n",
					  value);
				return -EINVAL;
			}
			if (!oa_format_valid(perf, value)) {
				drm_dbg(&perf->i915->drm,
					"Unsupported OA report format %llu\n",
					  value);
				return -EINVAL;
			}
			props->oa_format = value;
			break;
		case DRM_I915_PERF_PROP_OA_EXPONENT:
			if (value > OA_EXPONENT_MAX) {
				drm_dbg(&perf->i915->drm,
					"OA timer exponent too high (> %u)\n",
					 OA_EXPONENT_MAX);
				return -EINVAL;
			}

			 

			BUILD_BUG_ON(sizeof(oa_period) != 8);
			oa_period = oa_exponent_to_ns(perf, value);

			 
			if (oa_period <= NSEC_PER_SEC) {
				u64 tmp = NSEC_PER_SEC;
				do_div(tmp, oa_period);
				oa_freq_hz = tmp;
			} else
				oa_freq_hz = 0;

			if (oa_freq_hz > i915_oa_max_sample_rate && !perfmon_capable()) {
				drm_dbg(&perf->i915->drm,
					"OA exponent would exceed the max sampling frequency (sysctl dev.i915.oa_max_sample_rate) %uHz without CAP_PERFMON or CAP_SYS_ADMIN privileges\n",
					  i915_oa_max_sample_rate);
				return -EACCES;
			}

			props->oa_periodic = true;
			props->oa_period_exponent = value;
			break;
		case DRM_I915_PERF_PROP_HOLD_PREEMPTION:
			props->hold_preemption = !!value;
			break;
		case DRM_I915_PERF_PROP_GLOBAL_SSEU: {
			if (GRAPHICS_VER_FULL(perf->i915) >= IP_VER(12, 50)) {
				drm_dbg(&perf->i915->drm,
					"SSEU config not supported on gfx %x\n",
					GRAPHICS_VER_FULL(perf->i915));
				return -ENODEV;
			}

			if (copy_from_user(&user_sseu,
					   u64_to_user_ptr(value),
					   sizeof(user_sseu))) {
				drm_dbg(&perf->i915->drm,
					"Unable to copy global sseu parameter\n");
				return -EFAULT;
			}
			config_sseu = true;
			break;
		}
		case DRM_I915_PERF_PROP_POLL_OA_PERIOD:
			if (value < 100000  ) {
				drm_dbg(&perf->i915->drm,
					"OA availability timer too small (%lluns < 100us)\n",
					  value);
				return -EINVAL;
			}
			props->poll_oa_period = value;
			break;
		case DRM_I915_PERF_PROP_OA_ENGINE_CLASS:
			class = (u8)value;
			config_class = true;
			break;
		case DRM_I915_PERF_PROP_OA_ENGINE_INSTANCE:
			instance = (u8)value;
			config_instance = true;
			break;
		default:
			MISSING_CASE(id);
			return -EINVAL;
		}

		uprop += 2;
	}

	if ((config_class && !config_instance) ||
	    (config_instance && !config_class)) {
		drm_dbg(&perf->i915->drm,
			"OA engine-class and engine-instance parameters must be passed together\n");
		return -EINVAL;
	}

	props->engine = intel_engine_lookup_user(perf->i915, class, instance);
	if (!props->engine) {
		drm_dbg(&perf->i915->drm,
			"OA engine class and instance invalid %d:%d\n",
			class, instance);
		return -EINVAL;
	}

	if (!engine_supports_oa(props->engine)) {
		drm_dbg(&perf->i915->drm,
			"Engine not supported by OA %d:%d\n",
			class, instance);
		return -EINVAL;
	}

	 
	if (IS_MTL_MEDIA_STEP(props->engine->i915, STEP_A0, STEP_C0) &&
	    props->engine->oa_group->type == TYPE_OAM &&
	    intel_check_bios_c6_setup(&props->engine->gt->rc6)) {
		drm_dbg(&perf->i915->drm,
			"OAM requires media C6 to be disabled in BIOS\n");
		return -EINVAL;
	}

	i = array_index_nospec(props->oa_format, I915_OA_FORMAT_MAX);
	f = &perf->oa_formats[i];
	if (!engine_supports_oa_format(props->engine, f->type)) {
		drm_dbg(&perf->i915->drm,
			"Invalid OA format %d for class %d\n",
			f->type, props->engine->class);
		return -EINVAL;
	}

	if (config_sseu) {
		ret = get_sseu_config(&props->sseu, props->engine, &user_sseu);
		if (ret) {
			drm_dbg(&perf->i915->drm,
				"Invalid SSEU configuration\n");
			return ret;
		}
		props->has_sseu = true;
	}

	return 0;
}

 
int i915_perf_open_ioctl(struct drm_device *dev, void *data,
			 struct drm_file *file)
{
	struct i915_perf *perf = &to_i915(dev)->perf;
	struct drm_i915_perf_open_param *param = data;
	struct intel_gt *gt;
	struct perf_open_properties props;
	u32 known_open_flags;
	int ret;

	if (!perf->i915)
		return -ENOTSUPP;

	known_open_flags = I915_PERF_FLAG_FD_CLOEXEC |
			   I915_PERF_FLAG_FD_NONBLOCK |
			   I915_PERF_FLAG_DISABLED;
	if (param->flags & ~known_open_flags) {
		drm_dbg(&perf->i915->drm,
			"Unknown drm_i915_perf_open_param flag\n");
		return -EINVAL;
	}

	ret = read_properties_unlocked(perf,
				       u64_to_user_ptr(param->properties_ptr),
				       param->num_properties,
				       &props);
	if (ret)
		return ret;

	gt = props.engine->gt;

	mutex_lock(&gt->perf.lock);
	ret = i915_perf_open_ioctl_locked(perf, param, &props, file);
	mutex_unlock(&gt->perf.lock);

	return ret;
}

 
void i915_perf_register(struct drm_i915_private *i915)
{
	struct i915_perf *perf = &i915->perf;
	struct intel_gt *gt = to_gt(i915);

	if (!perf->i915)
		return;

	 
	mutex_lock(&gt->perf.lock);

	perf->metrics_kobj =
		kobject_create_and_add("metrics",
				       &i915->drm.primary->kdev->kobj);

	mutex_unlock(&gt->perf.lock);
}

 
void i915_perf_unregister(struct drm_i915_private *i915)
{
	struct i915_perf *perf = &i915->perf;

	if (!perf->metrics_kobj)
		return;

	kobject_put(perf->metrics_kobj);
	perf->metrics_kobj = NULL;
}

static bool gen8_is_valid_flex_addr(struct i915_perf *perf, u32 addr)
{
	static const i915_reg_t flex_eu_regs[] = {
		EU_PERF_CNTL0,
		EU_PERF_CNTL1,
		EU_PERF_CNTL2,
		EU_PERF_CNTL3,
		EU_PERF_CNTL4,
		EU_PERF_CNTL5,
		EU_PERF_CNTL6,
	};
	int i;

	for (i = 0; i < ARRAY_SIZE(flex_eu_regs); i++) {
		if (i915_mmio_reg_offset(flex_eu_regs[i]) == addr)
			return true;
	}
	return false;
}

static bool reg_in_range_table(u32 addr, const struct i915_range *table)
{
	while (table->start || table->end) {
		if (addr >= table->start && addr <= table->end)
			return true;

		table++;
	}

	return false;
}

#define REG_EQUAL(addr, mmio) \
	((addr) == i915_mmio_reg_offset(mmio))

static const struct i915_range gen7_oa_b_counters[] = {
	{ .start = 0x2710, .end = 0x272c },	 
	{ .start = 0x2740, .end = 0x275c },	 
	{ .start = 0x2770, .end = 0x27ac },	 
	{}
};

static const struct i915_range gen12_oa_b_counters[] = {
	{ .start = 0x2b2c, .end = 0x2b2c },	 
	{ .start = 0xd900, .end = 0xd91c },	 
	{ .start = 0xd920, .end = 0xd93c },	 
	{ .start = 0xd940, .end = 0xd97c },	 
	{ .start = 0xdc00, .end = 0xdc3c },	 
	{ .start = 0xdc40, .end = 0xdc40 },	 
	{ .start = 0xdc44, .end = 0xdc44 },	 
	{}
};

static const struct i915_range mtl_oam_b_counters[] = {
	{ .start = 0x393000, .end = 0x39301c },	 
	{ .start = 0x393020, .end = 0x39303c },	 
	{ .start = 0x393040, .end = 0x39307c },	 
	{ .start = 0x393200, .end = 0x39323C },	 
	{}
};

static const struct i915_range xehp_oa_b_counters[] = {
	{ .start = 0xdc48, .end = 0xdc48 },	 
	{ .start = 0xdd00, .end = 0xdd48 },	 
	{}
};

static const struct i915_range gen7_oa_mux_regs[] = {
	{ .start = 0x91b8, .end = 0x91cc },	 
	{ .start = 0x9800, .end = 0x9888 },	 
	{ .start = 0xe180, .end = 0xe180 },	 
	{}
};

static const struct i915_range hsw_oa_mux_regs[] = {
	{ .start = 0x09e80, .end = 0x09ea4 },  
	{ .start = 0x09ec0, .end = 0x09ec0 },  
	{ .start = 0x25100, .end = 0x2ff90 },
	{}
};

static const struct i915_range chv_oa_mux_regs[] = {
	{ .start = 0x182300, .end = 0x1823a4 },
	{}
};

static const struct i915_range gen8_oa_mux_regs[] = {
	{ .start = 0x0d00, .end = 0x0d2c },	 
	{ .start = 0x20cc, .end = 0x20cc },	 
	{}
};

static const struct i915_range gen11_oa_mux_regs[] = {
	{ .start = 0x91c8, .end = 0x91dc },	 
	{}
};

static const struct i915_range gen12_oa_mux_regs[] = {
	{ .start = 0x0d00, .end = 0x0d04 },      
	{ .start = 0x0d0c, .end = 0x0d2c },      
	{ .start = 0x9840, .end = 0x9840 },	 
	{ .start = 0x9884, .end = 0x9888 },	 
	{ .start = 0x20cc, .end = 0x20cc },	 
	{}
};

 
static const struct i915_range mtl_oa_mux_regs[] = {
	{ .start = 0x0d00, .end = 0x0d04 },	 
	{ .start = 0x0d0c, .end = 0x0d2c },	 
	{ .start = 0x9840, .end = 0x9840 },	 
	{ .start = 0x9884, .end = 0x9888 },	 
	{ .start = 0x38d100, .end = 0x38d114},	 
	{}
};

static bool gen7_is_valid_b_counter_addr(struct i915_perf *perf, u32 addr)
{
	return reg_in_range_table(addr, gen7_oa_b_counters);
}

static bool gen8_is_valid_mux_addr(struct i915_perf *perf, u32 addr)
{
	return reg_in_range_table(addr, gen7_oa_mux_regs) ||
		reg_in_range_table(addr, gen8_oa_mux_regs);
}

static bool gen11_is_valid_mux_addr(struct i915_perf *perf, u32 addr)
{
	return reg_in_range_table(addr, gen7_oa_mux_regs) ||
		reg_in_range_table(addr, gen8_oa_mux_regs) ||
		reg_in_range_table(addr, gen11_oa_mux_regs);
}

static bool hsw_is_valid_mux_addr(struct i915_perf *perf, u32 addr)
{
	return reg_in_range_table(addr, gen7_oa_mux_regs) ||
		reg_in_range_table(addr, hsw_oa_mux_regs);
}

static bool chv_is_valid_mux_addr(struct i915_perf *perf, u32 addr)
{
	return reg_in_range_table(addr, gen7_oa_mux_regs) ||
		reg_in_range_table(addr, chv_oa_mux_regs);
}

static bool gen12_is_valid_b_counter_addr(struct i915_perf *perf, u32 addr)
{
	return reg_in_range_table(addr, gen12_oa_b_counters);
}

static bool mtl_is_valid_oam_b_counter_addr(struct i915_perf *perf, u32 addr)
{
	if (HAS_OAM(perf->i915) &&
	    GRAPHICS_VER_FULL(perf->i915) >= IP_VER(12, 70))
		return reg_in_range_table(addr, mtl_oam_b_counters);

	return false;
}

static bool xehp_is_valid_b_counter_addr(struct i915_perf *perf, u32 addr)
{
	return reg_in_range_table(addr, xehp_oa_b_counters) ||
		reg_in_range_table(addr, gen12_oa_b_counters) ||
		mtl_is_valid_oam_b_counter_addr(perf, addr);
}

static bool gen12_is_valid_mux_addr(struct i915_perf *perf, u32 addr)
{
	if (IS_METEORLAKE(perf->i915))
		return reg_in_range_table(addr, mtl_oa_mux_regs);
	else
		return reg_in_range_table(addr, gen12_oa_mux_regs);
}

static u32 mask_reg_value(u32 reg, u32 val)
{
	 
	if (REG_EQUAL(reg, HALF_SLICE_CHICKEN2))
		val = val & ~_MASKED_BIT_ENABLE(GEN8_ST_PO_DISABLE);

	 
	if (REG_EQUAL(reg, WAIT_FOR_RC6_EXIT))
		val = val & ~_MASKED_BIT_ENABLE(HSW_WAIT_FOR_RC6_EXIT_ENABLE);

	return val;
}

static struct i915_oa_reg *alloc_oa_regs(struct i915_perf *perf,
					 bool (*is_valid)(struct i915_perf *perf, u32 addr),
					 u32 __user *regs,
					 u32 n_regs)
{
	struct i915_oa_reg *oa_regs;
	int err;
	u32 i;

	if (!n_regs)
		return NULL;

	 
	GEM_BUG_ON(!is_valid);
	if (!is_valid)
		return ERR_PTR(-EINVAL);

	oa_regs = kmalloc_array(n_regs, sizeof(*oa_regs), GFP_KERNEL);
	if (!oa_regs)
		return ERR_PTR(-ENOMEM);

	for (i = 0; i < n_regs; i++) {
		u32 addr, value;

		err = get_user(addr, regs);
		if (err)
			goto addr_err;

		if (!is_valid(perf, addr)) {
			drm_dbg(&perf->i915->drm,
				"Invalid oa_reg address: %X\n", addr);
			err = -EINVAL;
			goto addr_err;
		}

		err = get_user(value, regs + 1);
		if (err)
			goto addr_err;

		oa_regs[i].addr = _MMIO(addr);
		oa_regs[i].value = mask_reg_value(addr, value);

		regs += 2;
	}

	return oa_regs;

addr_err:
	kfree(oa_regs);
	return ERR_PTR(err);
}

static ssize_t show_dynamic_id(struct kobject *kobj,
			       struct kobj_attribute *attr,
			       char *buf)
{
	struct i915_oa_config *oa_config =
		container_of(attr, typeof(*oa_config), sysfs_metric_id);

	return sprintf(buf, "%d\n", oa_config->id);
}

static int create_dynamic_oa_sysfs_entry(struct i915_perf *perf,
					 struct i915_oa_config *oa_config)
{
	sysfs_attr_init(&oa_config->sysfs_metric_id.attr);
	oa_config->sysfs_metric_id.attr.name = "id";
	oa_config->sysfs_metric_id.attr.mode = S_IRUGO;
	oa_config->sysfs_metric_id.show = show_dynamic_id;
	oa_config->sysfs_metric_id.store = NULL;

	oa_config->attrs[0] = &oa_config->sysfs_metric_id.attr;
	oa_config->attrs[1] = NULL;

	oa_config->sysfs_metric.name = oa_config->uuid;
	oa_config->sysfs_metric.attrs = oa_config->attrs;

	return sysfs_create_group(perf->metrics_kobj,
				  &oa_config->sysfs_metric);
}

 
int i915_perf_add_config_ioctl(struct drm_device *dev, void *data,
			       struct drm_file *file)
{
	struct i915_perf *perf = &to_i915(dev)->perf;
	struct drm_i915_perf_oa_config *args = data;
	struct i915_oa_config *oa_config, *tmp;
	struct i915_oa_reg *regs;
	int err, id;

	if (!perf->i915)
		return -ENOTSUPP;

	if (!perf->metrics_kobj) {
		drm_dbg(&perf->i915->drm,
			"OA metrics weren't advertised via sysfs\n");
		return -EINVAL;
	}

	if (i915_perf_stream_paranoid && !perfmon_capable()) {
		drm_dbg(&perf->i915->drm,
			"Insufficient privileges to add i915 OA config\n");
		return -EACCES;
	}

	if ((!args->mux_regs_ptr || !args->n_mux_regs) &&
	    (!args->boolean_regs_ptr || !args->n_boolean_regs) &&
	    (!args->flex_regs_ptr || !args->n_flex_regs)) {
		drm_dbg(&perf->i915->drm,
			"No OA registers given\n");
		return -EINVAL;
	}

	oa_config = kzalloc(sizeof(*oa_config), GFP_KERNEL);
	if (!oa_config) {
		drm_dbg(&perf->i915->drm,
			"Failed to allocate memory for the OA config\n");
		return -ENOMEM;
	}

	oa_config->perf = perf;
	kref_init(&oa_config->ref);

	if (!uuid_is_valid(args->uuid)) {
		drm_dbg(&perf->i915->drm,
			"Invalid uuid format for OA config\n");
		err = -EINVAL;
		goto reg_err;
	}

	 
	memcpy(oa_config->uuid, args->uuid, sizeof(args->uuid));

	oa_config->mux_regs_len = args->n_mux_regs;
	regs = alloc_oa_regs(perf,
			     perf->ops.is_valid_mux_reg,
			     u64_to_user_ptr(args->mux_regs_ptr),
			     args->n_mux_regs);

	if (IS_ERR(regs)) {
		drm_dbg(&perf->i915->drm,
			"Failed to create OA config for mux_regs\n");
		err = PTR_ERR(regs);
		goto reg_err;
	}
	oa_config->mux_regs = regs;

	oa_config->b_counter_regs_len = args->n_boolean_regs;
	regs = alloc_oa_regs(perf,
			     perf->ops.is_valid_b_counter_reg,
			     u64_to_user_ptr(args->boolean_regs_ptr),
			     args->n_boolean_regs);

	if (IS_ERR(regs)) {
		drm_dbg(&perf->i915->drm,
			"Failed to create OA config for b_counter_regs\n");
		err = PTR_ERR(regs);
		goto reg_err;
	}
	oa_config->b_counter_regs = regs;

	if (GRAPHICS_VER(perf->i915) < 8) {
		if (args->n_flex_regs != 0) {
			err = -EINVAL;
			goto reg_err;
		}
	} else {
		oa_config->flex_regs_len = args->n_flex_regs;
		regs = alloc_oa_regs(perf,
				     perf->ops.is_valid_flex_reg,
				     u64_to_user_ptr(args->flex_regs_ptr),
				     args->n_flex_regs);

		if (IS_ERR(regs)) {
			drm_dbg(&perf->i915->drm,
				"Failed to create OA config for flex_regs\n");
			err = PTR_ERR(regs);
			goto reg_err;
		}
		oa_config->flex_regs = regs;
	}

	err = mutex_lock_interruptible(&perf->metrics_lock);
	if (err)
		goto reg_err;

	 
	idr_for_each_entry(&perf->metrics_idr, tmp, id) {
		if (!strcmp(tmp->uuid, oa_config->uuid)) {
			drm_dbg(&perf->i915->drm,
				"OA config already exists with this uuid\n");
			err = -EADDRINUSE;
			goto sysfs_err;
		}
	}

	err = create_dynamic_oa_sysfs_entry(perf, oa_config);
	if (err) {
		drm_dbg(&perf->i915->drm,
			"Failed to create sysfs entry for OA config\n");
		goto sysfs_err;
	}

	 
	oa_config->id = idr_alloc(&perf->metrics_idr,
				  oa_config, 2,
				  0, GFP_KERNEL);
	if (oa_config->id < 0) {
		drm_dbg(&perf->i915->drm,
			"Failed to create sysfs entry for OA config\n");
		err = oa_config->id;
		goto sysfs_err;
	}
	id = oa_config->id;

	drm_dbg(&perf->i915->drm,
		"Added config %s id=%i\n", oa_config->uuid, oa_config->id);
	mutex_unlock(&perf->metrics_lock);

	return id;

sysfs_err:
	mutex_unlock(&perf->metrics_lock);
reg_err:
	i915_oa_config_put(oa_config);
	drm_dbg(&perf->i915->drm,
		"Failed to add new OA config\n");
	return err;
}

 
int i915_perf_remove_config_ioctl(struct drm_device *dev, void *data,
				  struct drm_file *file)
{
	struct i915_perf *perf = &to_i915(dev)->perf;
	u64 *arg = data;
	struct i915_oa_config *oa_config;
	int ret;

	if (!perf->i915)
		return -ENOTSUPP;

	if (i915_perf_stream_paranoid && !perfmon_capable()) {
		drm_dbg(&perf->i915->drm,
			"Insufficient privileges to remove i915 OA config\n");
		return -EACCES;
	}

	ret = mutex_lock_interruptible(&perf->metrics_lock);
	if (ret)
		return ret;

	oa_config = idr_find(&perf->metrics_idr, *arg);
	if (!oa_config) {
		drm_dbg(&perf->i915->drm,
			"Failed to remove unknown OA config\n");
		ret = -ENOENT;
		goto err_unlock;
	}

	GEM_BUG_ON(*arg != oa_config->id);

	sysfs_remove_group(perf->metrics_kobj, &oa_config->sysfs_metric);

	idr_remove(&perf->metrics_idr, *arg);

	mutex_unlock(&perf->metrics_lock);

	drm_dbg(&perf->i915->drm,
		"Removed config %s id=%i\n", oa_config->uuid, oa_config->id);

	i915_oa_config_put(oa_config);

	return 0;

err_unlock:
	mutex_unlock(&perf->metrics_lock);
	return ret;
}

static struct ctl_table oa_table[] = {
	{
	 .procname = "perf_stream_paranoid",
	 .data = &i915_perf_stream_paranoid,
	 .maxlen = sizeof(i915_perf_stream_paranoid),
	 .mode = 0644,
	 .proc_handler = proc_dointvec_minmax,
	 .extra1 = SYSCTL_ZERO,
	 .extra2 = SYSCTL_ONE,
	 },
	{
	 .procname = "oa_max_sample_rate",
	 .data = &i915_oa_max_sample_rate,
	 .maxlen = sizeof(i915_oa_max_sample_rate),
	 .mode = 0644,
	 .proc_handler = proc_dointvec_minmax,
	 .extra1 = SYSCTL_ZERO,
	 .extra2 = &oa_sample_rate_hard_limit,
	 },
	{}
};

static u32 num_perf_groups_per_gt(struct intel_gt *gt)
{
	return 1;
}

static u32 __oam_engine_group(struct intel_engine_cs *engine)
{
	if (GRAPHICS_VER_FULL(engine->i915) >= IP_VER(12, 70)) {
		 
		drm_WARN_ON(&engine->i915->drm,
			    engine->gt->type != GT_MEDIA);

		return PERF_GROUP_OAM_SAMEDIA_0;
	}

	return PERF_GROUP_INVALID;
}

static u32 __oa_engine_group(struct intel_engine_cs *engine)
{
	switch (engine->class) {
	case RENDER_CLASS:
		return PERF_GROUP_OAG;

	case VIDEO_DECODE_CLASS:
	case VIDEO_ENHANCEMENT_CLASS:
		return __oam_engine_group(engine);

	default:
		return PERF_GROUP_INVALID;
	}
}

static struct i915_perf_regs __oam_regs(u32 base)
{
	return (struct i915_perf_regs) {
		base,
		GEN12_OAM_HEAD_POINTER(base),
		GEN12_OAM_TAIL_POINTER(base),
		GEN12_OAM_BUFFER(base),
		GEN12_OAM_CONTEXT_CONTROL(base),
		GEN12_OAM_CONTROL(base),
		GEN12_OAM_DEBUG(base),
		GEN12_OAM_STATUS(base),
		GEN12_OAM_CONTROL_COUNTER_FORMAT_SHIFT,
	};
}

static struct i915_perf_regs __oag_regs(void)
{
	return (struct i915_perf_regs) {
		0,
		GEN12_OAG_OAHEADPTR,
		GEN12_OAG_OATAILPTR,
		GEN12_OAG_OABUFFER,
		GEN12_OAG_OAGLBCTXCTRL,
		GEN12_OAG_OACONTROL,
		GEN12_OAG_OA_DEBUG,
		GEN12_OAG_OASTATUS,
		GEN12_OAG_OACONTROL_OA_COUNTER_FORMAT_SHIFT,
	};
}

static void oa_init_groups(struct intel_gt *gt)
{
	int i, num_groups = gt->perf.num_perf_groups;

	for (i = 0; i < num_groups; i++) {
		struct i915_perf_group *g = &gt->perf.group[i];

		 
		if (g->num_engines == 0)
			continue;

		if (i == PERF_GROUP_OAG && gt->type != GT_MEDIA) {
			g->regs = __oag_regs();
			g->type = TYPE_OAG;
		} else if (GRAPHICS_VER_FULL(gt->i915) >= IP_VER(12, 70)) {
			g->regs = __oam_regs(mtl_oa_base[i]);
			g->type = TYPE_OAM;
		}
	}
}

static int oa_init_gt(struct intel_gt *gt)
{
	u32 num_groups = num_perf_groups_per_gt(gt);
	struct intel_engine_cs *engine;
	struct i915_perf_group *g;
	intel_engine_mask_t tmp;

	g = kcalloc(num_groups, sizeof(*g), GFP_KERNEL);
	if (!g)
		return -ENOMEM;

	for_each_engine_masked(engine, gt, ALL_ENGINES, tmp) {
		u32 index = __oa_engine_group(engine);

		engine->oa_group = NULL;
		if (index < num_groups) {
			g[index].num_engines++;
			engine->oa_group = &g[index];
		}
	}

	gt->perf.num_perf_groups = num_groups;
	gt->perf.group = g;

	oa_init_groups(gt);

	return 0;
}

static int oa_init_engine_groups(struct i915_perf *perf)
{
	struct intel_gt *gt;
	int i, ret;

	for_each_gt(gt, perf->i915, i) {
		ret = oa_init_gt(gt);
		if (ret)
			return ret;
	}

	return 0;
}

static void oa_init_supported_formats(struct i915_perf *perf)
{
	struct drm_i915_private *i915 = perf->i915;
	enum intel_platform platform = INTEL_INFO(i915)->platform;

	switch (platform) {
	case INTEL_HASWELL:
		oa_format_add(perf, I915_OA_FORMAT_A13);
		oa_format_add(perf, I915_OA_FORMAT_A13);
		oa_format_add(perf, I915_OA_FORMAT_A29);
		oa_format_add(perf, I915_OA_FORMAT_A13_B8_C8);
		oa_format_add(perf, I915_OA_FORMAT_B4_C8);
		oa_format_add(perf, I915_OA_FORMAT_A45_B8_C8);
		oa_format_add(perf, I915_OA_FORMAT_B4_C8_A16);
		oa_format_add(perf, I915_OA_FORMAT_C4_B8);
		break;

	case INTEL_BROADWELL:
	case INTEL_CHERRYVIEW:
	case INTEL_SKYLAKE:
	case INTEL_BROXTON:
	case INTEL_KABYLAKE:
	case INTEL_GEMINILAKE:
	case INTEL_COFFEELAKE:
	case INTEL_COMETLAKE:
	case INTEL_ICELAKE:
	case INTEL_ELKHARTLAKE:
	case INTEL_JASPERLAKE:
	case INTEL_TIGERLAKE:
	case INTEL_ROCKETLAKE:
	case INTEL_DG1:
	case INTEL_ALDERLAKE_S:
	case INTEL_ALDERLAKE_P:
		oa_format_add(perf, I915_OA_FORMAT_A12);
		oa_format_add(perf, I915_OA_FORMAT_A12_B8_C8);
		oa_format_add(perf, I915_OA_FORMAT_A32u40_A4u32_B8_C8);
		oa_format_add(perf, I915_OA_FORMAT_C4_B8);
		break;

	case INTEL_DG2:
		oa_format_add(perf, I915_OAR_FORMAT_A32u40_A4u32_B8_C8);
		oa_format_add(perf, I915_OA_FORMAT_A24u40_A14u32_B8_C8);
		break;

	case INTEL_METEORLAKE:
		oa_format_add(perf, I915_OAR_FORMAT_A32u40_A4u32_B8_C8);
		oa_format_add(perf, I915_OA_FORMAT_A24u40_A14u32_B8_C8);
		oa_format_add(perf, I915_OAM_FORMAT_MPEC8u64_B8_C8);
		oa_format_add(perf, I915_OAM_FORMAT_MPEC8u32_B8_C8);
		break;

	default:
		MISSING_CASE(platform);
	}
}

static void i915_perf_init_info(struct drm_i915_private *i915)
{
	struct i915_perf *perf = &i915->perf;

	switch (GRAPHICS_VER(i915)) {
	case 8:
		perf->ctx_oactxctrl_offset = 0x120;
		perf->ctx_flexeu0_offset = 0x2ce;
		perf->gen8_valid_ctx_bit = BIT(25);
		break;
	case 9:
		perf->ctx_oactxctrl_offset = 0x128;
		perf->ctx_flexeu0_offset = 0x3de;
		perf->gen8_valid_ctx_bit = BIT(16);
		break;
	case 11:
		perf->ctx_oactxctrl_offset = 0x124;
		perf->ctx_flexeu0_offset = 0x78e;
		perf->gen8_valid_ctx_bit = BIT(16);
		break;
	case 12:
		perf->gen8_valid_ctx_bit = BIT(16);
		 
		break;
	default:
		MISSING_CASE(GRAPHICS_VER(i915));
	}
}

 
int i915_perf_init(struct drm_i915_private *i915)
{
	struct i915_perf *perf = &i915->perf;

	perf->oa_formats = oa_formats;
	if (IS_HASWELL(i915)) {
		perf->ops.is_valid_b_counter_reg = gen7_is_valid_b_counter_addr;
		perf->ops.is_valid_mux_reg = hsw_is_valid_mux_addr;
		perf->ops.is_valid_flex_reg = NULL;
		perf->ops.enable_metric_set = hsw_enable_metric_set;
		perf->ops.disable_metric_set = hsw_disable_metric_set;
		perf->ops.oa_enable = gen7_oa_enable;
		perf->ops.oa_disable = gen7_oa_disable;
		perf->ops.read = gen7_oa_read;
		perf->ops.oa_hw_tail_read = gen7_oa_hw_tail_read;
	} else if (HAS_LOGICAL_RING_CONTEXTS(i915)) {
		 
		perf->ops.read = gen8_oa_read;
		i915_perf_init_info(i915);

		if (IS_GRAPHICS_VER(i915, 8, 9)) {
			perf->ops.is_valid_b_counter_reg =
				gen7_is_valid_b_counter_addr;
			perf->ops.is_valid_mux_reg =
				gen8_is_valid_mux_addr;
			perf->ops.is_valid_flex_reg =
				gen8_is_valid_flex_addr;

			if (IS_CHERRYVIEW(i915)) {
				perf->ops.is_valid_mux_reg =
					chv_is_valid_mux_addr;
			}

			perf->ops.oa_enable = gen8_oa_enable;
			perf->ops.oa_disable = gen8_oa_disable;
			perf->ops.enable_metric_set = gen8_enable_metric_set;
			perf->ops.disable_metric_set = gen8_disable_metric_set;
			perf->ops.oa_hw_tail_read = gen8_oa_hw_tail_read;
		} else if (GRAPHICS_VER(i915) == 11) {
			perf->ops.is_valid_b_counter_reg =
				gen7_is_valid_b_counter_addr;
			perf->ops.is_valid_mux_reg =
				gen11_is_valid_mux_addr;
			perf->ops.is_valid_flex_reg =
				gen8_is_valid_flex_addr;

			perf->ops.oa_enable = gen8_oa_enable;
			perf->ops.oa_disable = gen8_oa_disable;
			perf->ops.enable_metric_set = gen8_enable_metric_set;
			perf->ops.disable_metric_set = gen11_disable_metric_set;
			perf->ops.oa_hw_tail_read = gen8_oa_hw_tail_read;
		} else if (GRAPHICS_VER(i915) == 12) {
			perf->ops.is_valid_b_counter_reg =
				HAS_OA_SLICE_CONTRIB_LIMITS(i915) ?
				xehp_is_valid_b_counter_addr :
				gen12_is_valid_b_counter_addr;
			perf->ops.is_valid_mux_reg =
				gen12_is_valid_mux_addr;
			perf->ops.is_valid_flex_reg =
				gen8_is_valid_flex_addr;

			perf->ops.oa_enable = gen12_oa_enable;
			perf->ops.oa_disable = gen12_oa_disable;
			perf->ops.enable_metric_set = gen12_enable_metric_set;
			perf->ops.disable_metric_set = gen12_disable_metric_set;
			perf->ops.oa_hw_tail_read = gen12_oa_hw_tail_read;
		}
	}

	if (perf->ops.enable_metric_set) {
		struct intel_gt *gt;
		int i, ret;

		for_each_gt(gt, i915, i)
			mutex_init(&gt->perf.lock);

		 
		oa_sample_rate_hard_limit = to_gt(i915)->clock_frequency / 2;

		mutex_init(&perf->metrics_lock);
		idr_init_base(&perf->metrics_idr, 1);

		 
		ratelimit_state_init(&perf->spurious_report_rs, 5 * HZ, 10);
		 
		ratelimit_set_flags(&perf->spurious_report_rs,
				    RATELIMIT_MSG_ON_RELEASE);

		ratelimit_state_init(&perf->tail_pointer_race,
				     5 * HZ, 10);
		ratelimit_set_flags(&perf->tail_pointer_race,
				    RATELIMIT_MSG_ON_RELEASE);

		atomic64_set(&perf->noa_programming_delay,
			     500 * 1000  );

		perf->i915 = i915;

		ret = oa_init_engine_groups(perf);
		if (ret) {
			drm_err(&i915->drm,
				"OA initialization failed %d\n", ret);
			return ret;
		}

		oa_init_supported_formats(perf);
	}

	return 0;
}

static int destroy_config(int id, void *p, void *data)
{
	i915_oa_config_put(p);
	return 0;
}

int i915_perf_sysctl_register(void)
{
	sysctl_header = register_sysctl("dev/i915", oa_table);
	return 0;
}

void i915_perf_sysctl_unregister(void)
{
	unregister_sysctl_table(sysctl_header);
}

 
void i915_perf_fini(struct drm_i915_private *i915)
{
	struct i915_perf *perf = &i915->perf;
	struct intel_gt *gt;
	int i;

	if (!perf->i915)
		return;

	for_each_gt(gt, perf->i915, i)
		kfree(gt->perf.group);

	idr_for_each(&perf->metrics_idr, destroy_config, perf);
	idr_destroy(&perf->metrics_idr);

	memset(&perf->ops, 0, sizeof(perf->ops));
	perf->i915 = NULL;
}

 
int i915_perf_ioctl_version(struct drm_i915_private *i915)
{
	 

	 
	if (IS_MTL_MEDIA_STEP(i915, STEP_A0, STEP_C0)) {
		struct intel_gt *gt;
		int i;

		for_each_gt(gt, i915, i) {
			if (gt->type == GT_MEDIA &&
			    intel_check_bios_c6_setup(&gt->rc6))
				return 6;
		}
	}

	return 7;
}

#if IS_ENABLED(CONFIG_DRM_I915_SELFTEST)
#include "selftests/i915_perf.c"
#endif
