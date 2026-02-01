
 

#include <linux/debugfs.h>
#include <linux/string_helpers.h>

#include "gt/intel_gt.h"
#include "i915_drv.h"
#include "i915_irq.h"
#include "i915_memcpy.h"
#include "intel_guc_capture.h"
#include "intel_guc_log.h"
#include "intel_guc_print.h"

#if defined(CONFIG_DRM_I915_DEBUG_GUC)
#define GUC_LOG_DEFAULT_CRASH_BUFFER_SIZE	SZ_2M
#define GUC_LOG_DEFAULT_DEBUG_BUFFER_SIZE	SZ_16M
#define GUC_LOG_DEFAULT_CAPTURE_BUFFER_SIZE	SZ_1M
#elif defined(CONFIG_DRM_I915_DEBUG_GEM)
#define GUC_LOG_DEFAULT_CRASH_BUFFER_SIZE	SZ_1M
#define GUC_LOG_DEFAULT_DEBUG_BUFFER_SIZE	SZ_2M
#define GUC_LOG_DEFAULT_CAPTURE_BUFFER_SIZE	SZ_1M
#else
#define GUC_LOG_DEFAULT_CRASH_BUFFER_SIZE	SZ_8K
#define GUC_LOG_DEFAULT_DEBUG_BUFFER_SIZE	SZ_64K
#define GUC_LOG_DEFAULT_CAPTURE_BUFFER_SIZE	SZ_1M
#endif

static void guc_log_copy_debuglogs_for_relay(struct intel_guc_log *log);

struct guc_log_section {
	u32 max;
	u32 flag;
	u32 default_val;
	const char *name;
};

static void _guc_log_init_sizes(struct intel_guc_log *log)
{
	struct intel_guc *guc = log_to_guc(log);
	static const struct guc_log_section sections[GUC_LOG_SECTIONS_LIMIT] = {
		{
			GUC_LOG_CRASH_MASK >> GUC_LOG_CRASH_SHIFT,
			GUC_LOG_LOG_ALLOC_UNITS,
			GUC_LOG_DEFAULT_CRASH_BUFFER_SIZE,
			"crash dump"
		},
		{
			GUC_LOG_DEBUG_MASK >> GUC_LOG_DEBUG_SHIFT,
			GUC_LOG_LOG_ALLOC_UNITS,
			GUC_LOG_DEFAULT_DEBUG_BUFFER_SIZE,
			"debug",
		},
		{
			GUC_LOG_CAPTURE_MASK >> GUC_LOG_CAPTURE_SHIFT,
			GUC_LOG_CAPTURE_ALLOC_UNITS,
			GUC_LOG_DEFAULT_CAPTURE_BUFFER_SIZE,
			"capture",
		}
	};
	int i;

	for (i = 0; i < GUC_LOG_SECTIONS_LIMIT; i++)
		log->sizes[i].bytes = sections[i].default_val;

	 
	if (log->sizes[GUC_LOG_SECTIONS_DEBUG].bytes >= SZ_1M &&
	    GUC_LOG_DEFAULT_CRASH_BUFFER_SIZE < SZ_1M)
		log->sizes[GUC_LOG_SECTIONS_CRASH].bytes = SZ_1M;

	 
	for (i = 0; i < GUC_LOG_SECTIONS_LIMIT; i++) {
		 
		if ((log->sizes[i].bytes % SZ_1M) == 0) {
			log->sizes[i].units = SZ_1M;
			log->sizes[i].flag = sections[i].flag;
		} else {
			log->sizes[i].units = SZ_4K;
			log->sizes[i].flag = 0;
		}

		if (!IS_ALIGNED(log->sizes[i].bytes, log->sizes[i].units))
			guc_err(guc, "Mis-aligned log %s size: 0x%X vs 0x%X!\n",
				sections[i].name, log->sizes[i].bytes, log->sizes[i].units);
		log->sizes[i].count = log->sizes[i].bytes / log->sizes[i].units;

		if (!log->sizes[i].count) {
			guc_err(guc, "Zero log %s size!\n", sections[i].name);
		} else {
			 
			log->sizes[i].count--;
		}

		 
		if (log->sizes[i].count > sections[i].max) {
			guc_err(guc, "log %s size too large: %d vs %d!\n",
				sections[i].name, log->sizes[i].count + 1, sections[i].max + 1);
			log->sizes[i].count = sections[i].max;
		}
	}

	if (log->sizes[GUC_LOG_SECTIONS_CRASH].units != log->sizes[GUC_LOG_SECTIONS_DEBUG].units) {
		guc_err(guc, "Unit mismatch for crash and debug sections: %d vs %d!\n",
			log->sizes[GUC_LOG_SECTIONS_CRASH].units,
			log->sizes[GUC_LOG_SECTIONS_DEBUG].units);
		log->sizes[GUC_LOG_SECTIONS_CRASH].units = log->sizes[GUC_LOG_SECTIONS_DEBUG].units;
		log->sizes[GUC_LOG_SECTIONS_CRASH].count = 0;
	}

	log->sizes_initialised = true;
}

static void guc_log_init_sizes(struct intel_guc_log *log)
{
	if (log->sizes_initialised)
		return;

	_guc_log_init_sizes(log);
}

static u32 intel_guc_log_section_size_crash(struct intel_guc_log *log)
{
	guc_log_init_sizes(log);

	return log->sizes[GUC_LOG_SECTIONS_CRASH].bytes;
}

static u32 intel_guc_log_section_size_debug(struct intel_guc_log *log)
{
	guc_log_init_sizes(log);

	return log->sizes[GUC_LOG_SECTIONS_DEBUG].bytes;
}

u32 intel_guc_log_section_size_capture(struct intel_guc_log *log)
{
	guc_log_init_sizes(log);

	return log->sizes[GUC_LOG_SECTIONS_CAPTURE].bytes;
}

static u32 intel_guc_log_size(struct intel_guc_log *log)
{
	 
	return PAGE_SIZE +
		intel_guc_log_section_size_crash(log) +
		intel_guc_log_section_size_debug(log) +
		intel_guc_log_section_size_capture(log);
}

 

static int guc_action_flush_log_complete(struct intel_guc *guc)
{
	u32 action[] = {
		INTEL_GUC_ACTION_LOG_BUFFER_FILE_FLUSH_COMPLETE,
		GUC_DEBUG_LOG_BUFFER
	};

	return intel_guc_send_nb(guc, action, ARRAY_SIZE(action), 0);
}

static int guc_action_flush_log(struct intel_guc *guc)
{
	u32 action[] = {
		INTEL_GUC_ACTION_FORCE_LOG_BUFFER_FLUSH,
		0
	};

	return intel_guc_send(guc, action, ARRAY_SIZE(action));
}

static int guc_action_control_log(struct intel_guc *guc, bool enable,
				  bool default_logging, u32 verbosity)
{
	u32 action[] = {
		INTEL_GUC_ACTION_UK_LOG_ENABLE_LOGGING,
		(enable ? GUC_LOG_CONTROL_LOGGING_ENABLED : 0) |
		(verbosity << GUC_LOG_CONTROL_VERBOSITY_SHIFT) |
		(default_logging ? GUC_LOG_CONTROL_DEFAULT_LOGGING : 0)
	};

	GEM_BUG_ON(verbosity > GUC_LOG_VERBOSITY_MAX);

	return intel_guc_send(guc, action, ARRAY_SIZE(action));
}

 
static int subbuf_start_callback(struct rchan_buf *buf,
				 void *subbuf,
				 void *prev_subbuf,
				 size_t prev_padding)
{
	 
	if (relay_buf_full(buf))
		return 0;

	return 1;
}

 
static struct dentry *create_buf_file_callback(const char *filename,
					       struct dentry *parent,
					       umode_t mode,
					       struct rchan_buf *buf,
					       int *is_global)
{
	struct dentry *buf_file;

	 
	*is_global = 1;

	if (!parent)
		return NULL;

	buf_file = debugfs_create_file(filename, mode,
				       parent, buf, &relay_file_operations);
	if (IS_ERR(buf_file))
		return NULL;

	return buf_file;
}

 
static int remove_buf_file_callback(struct dentry *dentry)
{
	debugfs_remove(dentry);
	return 0;
}

 
static const struct rchan_callbacks relay_callbacks = {
	.subbuf_start = subbuf_start_callback,
	.create_buf_file = create_buf_file_callback,
	.remove_buf_file = remove_buf_file_callback,
};

static void guc_move_to_next_buf(struct intel_guc_log *log)
{
	 
	smp_wmb();

	 
	relay_reserve(log->relay.channel, log->vma->obj->base.size -
					  intel_guc_log_section_size_capture(log));

	 
	relay_flush(log->relay.channel);
}

static void *guc_get_write_buffer(struct intel_guc_log *log)
{
	 
	return relay_reserve(log->relay.channel, 0);
}

bool intel_guc_check_log_buf_overflow(struct intel_guc_log *log,
				      enum guc_log_buffer_type type,
				      unsigned int full_cnt)
{
	unsigned int prev_full_cnt = log->stats[type].sampled_overflow;
	bool overflow = false;

	if (full_cnt != prev_full_cnt) {
		overflow = true;

		log->stats[type].overflow = full_cnt;
		log->stats[type].sampled_overflow += full_cnt - prev_full_cnt;

		if (full_cnt < prev_full_cnt) {
			 
			log->stats[type].sampled_overflow += 16;
		}

		guc_notice_ratelimited(log_to_guc(log), "log buffer overflow\n");
	}

	return overflow;
}

unsigned int intel_guc_get_log_buffer_size(struct intel_guc_log *log,
					   enum guc_log_buffer_type type)
{
	switch (type) {
	case GUC_DEBUG_LOG_BUFFER:
		return intel_guc_log_section_size_debug(log);
	case GUC_CRASH_DUMP_LOG_BUFFER:
		return intel_guc_log_section_size_crash(log);
	case GUC_CAPTURE_LOG_BUFFER:
		return intel_guc_log_section_size_capture(log);
	default:
		MISSING_CASE(type);
	}

	return 0;
}

size_t intel_guc_get_log_buffer_offset(struct intel_guc_log *log,
				       enum guc_log_buffer_type type)
{
	enum guc_log_buffer_type i;
	size_t offset = PAGE_SIZE; 

	for (i = GUC_DEBUG_LOG_BUFFER; i < GUC_MAX_LOG_BUFFER; ++i) {
		if (i == type)
			break;
		offset += intel_guc_get_log_buffer_size(log, i);
	}

	return offset;
}

static void _guc_log_copy_debuglogs_for_relay(struct intel_guc_log *log)
{
	struct intel_guc *guc = log_to_guc(log);
	unsigned int buffer_size, read_offset, write_offset, bytes_to_copy, full_cnt;
	struct guc_log_buffer_state *log_buf_state, *log_buf_snapshot_state;
	struct guc_log_buffer_state log_buf_state_local;
	enum guc_log_buffer_type type;
	void *src_data, *dst_data;
	bool new_overflow;

	mutex_lock(&log->relay.lock);

	if (guc_WARN_ON(guc, !intel_guc_log_relay_created(log)))
		goto out_unlock;

	 
	src_data = log->buf_addr;
	log_buf_state = src_data;

	 
	log_buf_snapshot_state = dst_data = guc_get_write_buffer(log);

	if (unlikely(!log_buf_snapshot_state)) {
		 
		guc_err_ratelimited(guc, "no sub-buffer to copy general logs\n");
		log->relay.full_count++;

		goto out_unlock;
	}

	 
	src_data += PAGE_SIZE;
	dst_data += PAGE_SIZE;

	 
	for (type = GUC_DEBUG_LOG_BUFFER; type <= GUC_CRASH_DUMP_LOG_BUFFER; type++) {
		 
		memcpy(&log_buf_state_local, log_buf_state,
		       sizeof(struct guc_log_buffer_state));
		buffer_size = intel_guc_get_log_buffer_size(log, type);
		read_offset = log_buf_state_local.read_ptr;
		write_offset = log_buf_state_local.sampled_write_ptr;
		full_cnt = log_buf_state_local.buffer_full_cnt;

		 
		log->stats[type].flush += log_buf_state_local.flush_to_file;
		new_overflow = intel_guc_check_log_buf_overflow(log, type, full_cnt);

		 
		log_buf_state->read_ptr = write_offset;
		log_buf_state->flush_to_file = 0;
		log_buf_state++;

		 
		memcpy(log_buf_snapshot_state, &log_buf_state_local,
		       sizeof(struct guc_log_buffer_state));

		 
		log_buf_snapshot_state->write_ptr = write_offset;
		log_buf_snapshot_state++;

		 
		if (unlikely(new_overflow)) {
			 
			read_offset = 0;
			write_offset = buffer_size;
		} else if (unlikely((read_offset > buffer_size) ||
				    (write_offset > buffer_size))) {
			guc_err(guc, "invalid log buffer state\n");
			 
			read_offset = 0;
			write_offset = buffer_size;
		}

		 
		if (read_offset > write_offset) {
			i915_memcpy_from_wc(dst_data, src_data, write_offset);
			bytes_to_copy = buffer_size - read_offset;
		} else {
			bytes_to_copy = write_offset - read_offset;
		}
		i915_memcpy_from_wc(dst_data + read_offset,
				    src_data + read_offset, bytes_to_copy);

		src_data += buffer_size;
		dst_data += buffer_size;
	}

	guc_move_to_next_buf(log);

out_unlock:
	mutex_unlock(&log->relay.lock);
}

static void copy_debug_logs_work(struct work_struct *work)
{
	struct intel_guc_log *log =
		container_of(work, struct intel_guc_log, relay.flush_work);

	guc_log_copy_debuglogs_for_relay(log);
}

static int guc_log_relay_map(struct intel_guc_log *log)
{
	lockdep_assert_held(&log->relay.lock);

	if (!log->vma || !log->buf_addr)
		return -ENODEV;

	 
	i915_gem_object_get(log->vma->obj);
	log->relay.buf_in_use = true;

	return 0;
}

static void guc_log_relay_unmap(struct intel_guc_log *log)
{
	lockdep_assert_held(&log->relay.lock);

	i915_gem_object_put(log->vma->obj);
	log->relay.buf_in_use = false;
}

void intel_guc_log_init_early(struct intel_guc_log *log)
{
	mutex_init(&log->relay.lock);
	INIT_WORK(&log->relay.flush_work, copy_debug_logs_work);
	log->relay.started = false;
}

static int guc_log_relay_create(struct intel_guc_log *log)
{
	struct intel_guc *guc = log_to_guc(log);
	struct drm_i915_private *i915 = guc_to_gt(guc)->i915;
	struct rchan *guc_log_relay_chan;
	size_t n_subbufs, subbuf_size;
	int ret;

	lockdep_assert_held(&log->relay.lock);
	GEM_BUG_ON(!log->vma);

	  
	subbuf_size = log->vma->size - intel_guc_log_section_size_capture(log);

	 
	n_subbufs = 8;

	if (!guc->dbgfs_node)
		return -ENOENT;

	guc_log_relay_chan = relay_open("guc_log",
					guc->dbgfs_node,
					subbuf_size, n_subbufs,
					&relay_callbacks, i915);
	if (!guc_log_relay_chan) {
		guc_err(guc, "Couldn't create relay channel for logging\n");

		ret = -ENOMEM;
		return ret;
	}

	GEM_BUG_ON(guc_log_relay_chan->subbuf_size < subbuf_size);
	log->relay.channel = guc_log_relay_chan;

	return 0;
}

static void guc_log_relay_destroy(struct intel_guc_log *log)
{
	lockdep_assert_held(&log->relay.lock);

	relay_close(log->relay.channel);
	log->relay.channel = NULL;
}

static void guc_log_copy_debuglogs_for_relay(struct intel_guc_log *log)
{
	struct intel_guc *guc = log_to_guc(log);
	struct drm_i915_private *i915 = guc_to_gt(guc)->i915;
	intel_wakeref_t wakeref;

	_guc_log_copy_debuglogs_for_relay(log);

	 
	with_intel_runtime_pm(&i915->runtime_pm, wakeref)
		guc_action_flush_log_complete(guc);
}

static u32 __get_default_log_level(struct intel_guc_log *log)
{
	struct intel_guc *guc = log_to_guc(log);
	struct drm_i915_private *i915 = guc_to_gt(guc)->i915;

	 
	if (i915->params.guc_log_level < 0) {
		return (IS_ENABLED(CONFIG_DRM_I915_DEBUG) ||
			IS_ENABLED(CONFIG_DRM_I915_DEBUG_GEM)) ?
			GUC_LOG_LEVEL_MAX : GUC_LOG_LEVEL_NON_VERBOSE;
	}

	if (i915->params.guc_log_level > GUC_LOG_LEVEL_MAX) {
		guc_warn(guc, "Log verbosity param out of range: %d > %d!\n",
			 i915->params.guc_log_level, GUC_LOG_LEVEL_MAX);
		return (IS_ENABLED(CONFIG_DRM_I915_DEBUG) ||
			IS_ENABLED(CONFIG_DRM_I915_DEBUG_GEM)) ?
			GUC_LOG_LEVEL_MAX : GUC_LOG_LEVEL_DISABLED;
	}

	GEM_BUG_ON(i915->params.guc_log_level < GUC_LOG_LEVEL_DISABLED);
	GEM_BUG_ON(i915->params.guc_log_level > GUC_LOG_LEVEL_MAX);
	return i915->params.guc_log_level;
}

int intel_guc_log_create(struct intel_guc_log *log)
{
	struct intel_guc *guc = log_to_guc(log);
	struct i915_vma *vma;
	void *vaddr;
	u32 guc_log_size;
	int ret;

	GEM_BUG_ON(log->vma);

	guc_log_size = intel_guc_log_size(log);

	vma = intel_guc_allocate_vma(guc, guc_log_size);
	if (IS_ERR(vma)) {
		ret = PTR_ERR(vma);
		goto err;
	}

	log->vma = vma;
	 
	vaddr = i915_gem_object_pin_map_unlocked(log->vma->obj, I915_MAP_WC);
	if (IS_ERR(vaddr)) {
		ret = PTR_ERR(vaddr);
		i915_vma_unpin_and_release(&log->vma, 0);
		goto err;
	}
	log->buf_addr = vaddr;

	log->level = __get_default_log_level(log);
	guc_dbg(guc, "guc_log_level=%d (%s, verbose:%s, verbosity:%d)\n",
		log->level, str_enabled_disabled(log->level),
		str_yes_no(GUC_LOG_LEVEL_IS_VERBOSE(log->level)),
		GUC_LOG_LEVEL_TO_VERBOSITY(log->level));

	return 0;

err:
	guc_err(guc, "Failed to allocate or map log buffer %pe\n", ERR_PTR(ret));
	return ret;
}

void intel_guc_log_destroy(struct intel_guc_log *log)
{
	log->buf_addr = NULL;
	i915_vma_unpin_and_release(&log->vma, I915_VMA_RELEASE_MAP);
}

int intel_guc_log_set_level(struct intel_guc_log *log, u32 level)
{
	struct intel_guc *guc = log_to_guc(log);
	struct drm_i915_private *i915 = guc_to_gt(guc)->i915;
	intel_wakeref_t wakeref;
	int ret = 0;

	BUILD_BUG_ON(GUC_LOG_VERBOSITY_MIN != 0);
	GEM_BUG_ON(!log->vma);

	 
	if (level < GUC_LOG_LEVEL_DISABLED || level > GUC_LOG_LEVEL_MAX)
		return -EINVAL;

	mutex_lock(&i915->drm.struct_mutex);

	if (log->level == level)
		goto out_unlock;

	with_intel_runtime_pm(&i915->runtime_pm, wakeref)
		ret = guc_action_control_log(guc,
					     GUC_LOG_LEVEL_IS_VERBOSE(level),
					     GUC_LOG_LEVEL_IS_ENABLED(level),
					     GUC_LOG_LEVEL_TO_VERBOSITY(level));
	if (ret) {
		guc_dbg(guc, "guc_log_control action failed %pe\n", ERR_PTR(ret));
		goto out_unlock;
	}

	log->level = level;

out_unlock:
	mutex_unlock(&i915->drm.struct_mutex);

	return ret;
}

bool intel_guc_log_relay_created(const struct intel_guc_log *log)
{
	return log->buf_addr;
}

int intel_guc_log_relay_open(struct intel_guc_log *log)
{
	int ret;

	if (!log->vma)
		return -ENODEV;

	mutex_lock(&log->relay.lock);

	if (intel_guc_log_relay_created(log)) {
		ret = -EEXIST;
		goto out_unlock;
	}

	 
	if (!i915_has_memcpy_from_wc()) {
		ret = -ENXIO;
		goto out_unlock;
	}

	ret = guc_log_relay_create(log);
	if (ret)
		goto out_unlock;

	ret = guc_log_relay_map(log);
	if (ret)
		goto out_relay;

	mutex_unlock(&log->relay.lock);

	return 0;

out_relay:
	guc_log_relay_destroy(log);
out_unlock:
	mutex_unlock(&log->relay.lock);

	return ret;
}

int intel_guc_log_relay_start(struct intel_guc_log *log)
{
	if (log->relay.started)
		return -EEXIST;

	 
	queue_work(system_highpri_wq, &log->relay.flush_work);

	log->relay.started = true;

	return 0;
}

void intel_guc_log_relay_flush(struct intel_guc_log *log)
{
	struct intel_guc *guc = log_to_guc(log);
	intel_wakeref_t wakeref;

	if (!log->relay.started)
		return;

	 
	flush_work(&log->relay.flush_work);

	with_intel_runtime_pm(guc_to_gt(guc)->uncore->rpm, wakeref)
		guc_action_flush_log(guc);

	 
	guc_log_copy_debuglogs_for_relay(log);
}

 
static void guc_log_relay_stop(struct intel_guc_log *log)
{
	struct intel_guc *guc = log_to_guc(log);
	struct drm_i915_private *i915 = guc_to_gt(guc)->i915;

	if (!log->relay.started)
		return;

	intel_synchronize_irq(i915);

	flush_work(&log->relay.flush_work);

	log->relay.started = false;
}

void intel_guc_log_relay_close(struct intel_guc_log *log)
{
	guc_log_relay_stop(log);

	mutex_lock(&log->relay.lock);
	GEM_BUG_ON(!intel_guc_log_relay_created(log));
	guc_log_relay_unmap(log);
	guc_log_relay_destroy(log);
	mutex_unlock(&log->relay.lock);
}

void intel_guc_log_handle_flush_event(struct intel_guc_log *log)
{
	if (log->relay.started)
		queue_work(system_highpri_wq, &log->relay.flush_work);
}

static const char *
stringify_guc_log_type(enum guc_log_buffer_type type)
{
	switch (type) {
	case GUC_DEBUG_LOG_BUFFER:
		return "DEBUG";
	case GUC_CRASH_DUMP_LOG_BUFFER:
		return "CRASH";
	case GUC_CAPTURE_LOG_BUFFER:
		return "CAPTURE";
	default:
		MISSING_CASE(type);
	}

	return "";
}

 
void intel_guc_log_info(struct intel_guc_log *log, struct drm_printer *p)
{
	enum guc_log_buffer_type type;

	if (!intel_guc_log_relay_created(log)) {
		drm_puts(p, "GuC log relay not created\n");
		return;
	}

	drm_puts(p, "GuC logging stats:\n");

	drm_printf(p, "\tRelay full count: %u\n", log->relay.full_count);

	for (type = GUC_DEBUG_LOG_BUFFER; type < GUC_MAX_LOG_BUFFER; type++) {
		drm_printf(p, "\t%s:\tflush count %10u, overflow count %10u\n",
			   stringify_guc_log_type(type),
			   log->stats[type].flush,
			   log->stats[type].sampled_overflow);
	}
}

 
int intel_guc_log_dump(struct intel_guc_log *log, struct drm_printer *p,
		       bool dump_load_err)
{
	struct intel_guc *guc = log_to_guc(log);
	struct intel_uc *uc = container_of(guc, struct intel_uc, guc);
	struct drm_i915_gem_object *obj = NULL;
	void *map;
	u32 *page;
	int i, j;

	if (!intel_guc_is_supported(guc))
		return -ENODEV;

	if (dump_load_err)
		obj = uc->load_err_log;
	else if (guc->log.vma)
		obj = guc->log.vma->obj;

	if (!obj)
		return 0;

	page = (u32 *)__get_free_page(GFP_KERNEL);
	if (!page)
		return -ENOMEM;

	intel_guc_dump_time_info(guc, p);

	map = i915_gem_object_pin_map_unlocked(obj, I915_MAP_WC);
	if (IS_ERR(map)) {
		guc_dbg(guc, "Failed to pin log object: %pe\n", map);
		drm_puts(p, "(log data unaccessible)\n");
		free_page((unsigned long)page);
		return PTR_ERR(map);
	}

	for (i = 0; i < obj->base.size; i += PAGE_SIZE) {
		if (!i915_memcpy_from_wc(page, map + i, PAGE_SIZE))
			memcpy(page, map + i, PAGE_SIZE);

		for (j = 0; j < PAGE_SIZE / sizeof(u32); j += 4)
			drm_printf(p, "0x%08x 0x%08x 0x%08x 0x%08x\n",
				   *(page + j + 0), *(page + j + 1),
				   *(page + j + 2), *(page + j + 3));
	}

	drm_puts(p, "\n");

	i915_gem_object_unpin_map(obj);
	free_page((unsigned long)page);

	return 0;
}
