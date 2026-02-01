
 

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/log2.h>
#include <linux/zalloc.h>
#include <time.h>

#include "../../../util/cpumap.h"
#include "../../../util/event.h"
#include "../../../util/evsel.h"
#include "../../../util/evsel_config.h"
#include "../../../util/evlist.h"
#include "../../../util/session.h"
#include <internal/lib.h> 
#include "../../../util/pmu.h"
#include "../../../util/debug.h"
#include "../../../util/auxtrace.h"
#include "../../../util/record.h"
#include "../../../util/arm-spe.h"
#include <tools/libc_compat.h> 

#define KiB(x) ((x) * 1024)
#define MiB(x) ((x) * 1024 * 1024)

struct arm_spe_recording {
	struct auxtrace_record		itr;
	struct perf_pmu			*arm_spe_pmu;
	struct evlist		*evlist;
	int			wrapped_cnt;
	bool			*wrapped;
};

static size_t
arm_spe_info_priv_size(struct auxtrace_record *itr __maybe_unused,
		       struct evlist *evlist __maybe_unused)
{
	return ARM_SPE_AUXTRACE_PRIV_SIZE;
}

static int arm_spe_info_fill(struct auxtrace_record *itr,
			     struct perf_session *session,
			     struct perf_record_auxtrace_info *auxtrace_info,
			     size_t priv_size)
{
	struct arm_spe_recording *sper =
			container_of(itr, struct arm_spe_recording, itr);
	struct perf_pmu *arm_spe_pmu = sper->arm_spe_pmu;

	if (priv_size != ARM_SPE_AUXTRACE_PRIV_SIZE)
		return -EINVAL;

	if (!session->evlist->core.nr_mmaps)
		return -EINVAL;

	auxtrace_info->type = PERF_AUXTRACE_ARM_SPE;
	auxtrace_info->priv[ARM_SPE_PMU_TYPE] = arm_spe_pmu->type;

	return 0;
}

static void
arm_spe_snapshot_resolve_auxtrace_defaults(struct record_opts *opts,
					   bool privileged)
{
	 

	 
	if (!opts->auxtrace_snapshot_size && !opts->auxtrace_mmap_pages) {
		if (privileged) {
			opts->auxtrace_mmap_pages = MiB(4) / page_size;
		} else {
			opts->auxtrace_mmap_pages = KiB(128) / page_size;
			if (opts->mmap_pages == UINT_MAX)
				opts->mmap_pages = KiB(256) / page_size;
		}
	} else if (!opts->auxtrace_mmap_pages && !privileged && opts->mmap_pages == UINT_MAX) {
		opts->mmap_pages = KiB(256) / page_size;
	}

	 
	if (!opts->auxtrace_snapshot_size)
		opts->auxtrace_snapshot_size = opts->auxtrace_mmap_pages * (size_t)page_size;

	 
	if (!opts->auxtrace_mmap_pages) {
		size_t sz = opts->auxtrace_snapshot_size;

		sz = round_up(sz, page_size) / page_size;
		opts->auxtrace_mmap_pages = roundup_pow_of_two(sz);
	}
}

static int arm_spe_recording_options(struct auxtrace_record *itr,
				     struct evlist *evlist,
				     struct record_opts *opts)
{
	struct arm_spe_recording *sper =
			container_of(itr, struct arm_spe_recording, itr);
	struct perf_pmu *arm_spe_pmu = sper->arm_spe_pmu;
	struct evsel *evsel, *arm_spe_evsel = NULL;
	struct perf_cpu_map *cpus = evlist->core.user_requested_cpus;
	bool privileged = perf_event_paranoid_check(-1);
	struct evsel *tracking_evsel;
	int err;
	u64 bit;

	sper->evlist = evlist;

	evlist__for_each_entry(evlist, evsel) {
		if (evsel->core.attr.type == arm_spe_pmu->type) {
			if (arm_spe_evsel) {
				pr_err("There may be only one " ARM_SPE_PMU_NAME "x event\n");
				return -EINVAL;
			}
			evsel->core.attr.freq = 0;
			evsel->core.attr.sample_period = arm_spe_pmu->default_config->sample_period;
			evsel->needs_auxtrace_mmap = true;
			arm_spe_evsel = evsel;
			opts->full_auxtrace = true;
		}
	}

	if (!opts->full_auxtrace)
		return 0;

	 
	if (opts->auxtrace_snapshot_mode) {
		 
		if (!opts->auxtrace_snapshot_size || !opts->auxtrace_mmap_pages)
			arm_spe_snapshot_resolve_auxtrace_defaults(opts, privileged);

		 
		if (opts->auxtrace_snapshot_size > opts->auxtrace_mmap_pages * (size_t)page_size) {
			pr_err("Snapshot size %zu must not be greater than AUX area tracing mmap size %zu\n",
			       opts->auxtrace_snapshot_size,
			       opts->auxtrace_mmap_pages * (size_t)page_size);
			return -EINVAL;
		}

		 
		if (!opts->auxtrace_snapshot_size || !opts->auxtrace_mmap_pages) {
			pr_err("Failed to calculate default snapshot size and/or AUX area tracing mmap pages\n");
			return -EINVAL;
		}
	}

	 
	if (!opts->auxtrace_mmap_pages) {
		if (privileged) {
			opts->auxtrace_mmap_pages = MiB(4) / page_size;
		} else {
			opts->auxtrace_mmap_pages = KiB(128) / page_size;
			if (opts->mmap_pages == UINT_MAX)
				opts->mmap_pages = KiB(256) / page_size;
		}
	}

	 
	if (opts->auxtrace_mmap_pages) {
		size_t sz = opts->auxtrace_mmap_pages * (size_t)page_size;
		size_t min_sz = KiB(8);

		if (sz < min_sz || !is_power_of_2(sz)) {
			pr_err("Invalid mmap size for ARM SPE: must be at least %zuKiB and a power of 2\n",
			       min_sz / 1024);
			return -EINVAL;
		}
	}

	if (opts->auxtrace_snapshot_mode)
		pr_debug2("%sx snapshot size: %zu\n", ARM_SPE_PMU_NAME,
			  opts->auxtrace_snapshot_size);

	 
	evlist__to_front(evlist, arm_spe_evsel);

	 
	if (!perf_cpu_map__empty(cpus)) {
		evsel__set_sample_bit(arm_spe_evsel, CPU);
		evsel__set_config_if_unset(arm_spe_pmu, arm_spe_evsel,
					   "ts_enable", 1);
	}

	 
	evsel__set_sample_bit(arm_spe_evsel, DATA_SRC);

	 
	bit = perf_pmu__format_bits(arm_spe_pmu, "pa_enable");
	if (arm_spe_evsel->core.attr.config & bit)
		evsel__set_sample_bit(arm_spe_evsel, PHYS_ADDR);

	 
	err = parse_event(evlist, "dummy:u");
	if (err)
		return err;

	tracking_evsel = evlist__last(evlist);
	evlist__set_tracking_event(evlist, tracking_evsel);

	tracking_evsel->core.attr.freq = 0;
	tracking_evsel->core.attr.sample_period = 1;

	 
	if (!perf_cpu_map__empty(cpus)) {
		evsel__set_sample_bit(tracking_evsel, TIME);
		evsel__set_sample_bit(tracking_evsel, CPU);

		 
		if (!record_opts__no_switch_events(opts))
			tracking_evsel->core.attr.context_switch = 1;
	}

	return 0;
}

static int arm_spe_parse_snapshot_options(struct auxtrace_record *itr __maybe_unused,
					 struct record_opts *opts,
					 const char *str)
{
	unsigned long long snapshot_size = 0;
	char *endptr;

	if (str) {
		snapshot_size = strtoull(str, &endptr, 0);
		if (*endptr || snapshot_size > SIZE_MAX)
			return -1;
	}

	opts->auxtrace_snapshot_mode = true;
	opts->auxtrace_snapshot_size = snapshot_size;

	return 0;
}

static int arm_spe_snapshot_start(struct auxtrace_record *itr)
{
	struct arm_spe_recording *ptr =
			container_of(itr, struct arm_spe_recording, itr);
	struct evsel *evsel;

	evlist__for_each_entry(ptr->evlist, evsel) {
		if (evsel->core.attr.type == ptr->arm_spe_pmu->type)
			return evsel__disable(evsel);
	}
	return -EINVAL;
}

static int arm_spe_snapshot_finish(struct auxtrace_record *itr)
{
	struct arm_spe_recording *ptr =
			container_of(itr, struct arm_spe_recording, itr);
	struct evsel *evsel;

	evlist__for_each_entry(ptr->evlist, evsel) {
		if (evsel->core.attr.type == ptr->arm_spe_pmu->type)
			return evsel__enable(evsel);
	}
	return -EINVAL;
}

static int arm_spe_alloc_wrapped_array(struct arm_spe_recording *ptr, int idx)
{
	bool *wrapped;
	int cnt = ptr->wrapped_cnt, new_cnt, i;

	 
	if (idx < cnt)
		return 0;

	 
	new_cnt = idx + 1;

	 
	wrapped = reallocarray(ptr->wrapped, new_cnt, sizeof(bool));
	if (!wrapped)
		return -ENOMEM;

	 
	for (i = cnt; i < new_cnt; i++)
		wrapped[i] = false;

	ptr->wrapped_cnt = new_cnt;
	ptr->wrapped = wrapped;

	return 0;
}

static bool arm_spe_buffer_has_wrapped(unsigned char *buffer,
				      size_t buffer_size, u64 head)
{
	u64 i, watermark;
	u64 *buf = (u64 *)buffer;
	size_t buf_size = buffer_size;

	 
	if (head >= buffer_size)
		return true;

	 
	watermark = buf_size - 512;

	 

	 
	if (head > watermark)
		watermark = head;

	 
	watermark /= sizeof(u64);
	buf_size /= sizeof(u64);

	 
	for (i = watermark; i < buf_size; i++)
		if (buf[i])
			return true;

	return false;
}

static int arm_spe_find_snapshot(struct auxtrace_record *itr, int idx,
				  struct auxtrace_mmap *mm, unsigned char *data,
				  u64 *head, u64 *old)
{
	int err;
	bool wrapped;
	struct arm_spe_recording *ptr =
			container_of(itr, struct arm_spe_recording, itr);

	 
	if (idx >= ptr->wrapped_cnt) {
		err = arm_spe_alloc_wrapped_array(ptr, idx);
		if (err)
			return err;
	}

	 
	wrapped = ptr->wrapped[idx];
	if (!wrapped && arm_spe_buffer_has_wrapped(data, mm->len, *head)) {
		wrapped = true;
		ptr->wrapped[idx] = true;
	}

	pr_debug3("%s: mmap index %d old head %zu new head %zu size %zu\n",
		  __func__, idx, (size_t)*old, (size_t)*head, mm->len);

	 
	if (!wrapped)
		return 0;

	 
	if (*head >= mm->len) {
		*old = *head - mm->len;
	} else {
		*head += mm->len;
		*old = *head - mm->len;
	}

	return 0;
}

static u64 arm_spe_reference(struct auxtrace_record *itr __maybe_unused)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC_RAW, &ts);

	return ts.tv_sec ^ ts.tv_nsec;
}

static void arm_spe_recording_free(struct auxtrace_record *itr)
{
	struct arm_spe_recording *sper =
			container_of(itr, struct arm_spe_recording, itr);

	zfree(&sper->wrapped);
	free(sper);
}

struct auxtrace_record *arm_spe_recording_init(int *err,
					       struct perf_pmu *arm_spe_pmu)
{
	struct arm_spe_recording *sper;

	if (!arm_spe_pmu) {
		*err = -ENODEV;
		return NULL;
	}

	sper = zalloc(sizeof(struct arm_spe_recording));
	if (!sper) {
		*err = -ENOMEM;
		return NULL;
	}

	sper->arm_spe_pmu = arm_spe_pmu;
	sper->itr.pmu = arm_spe_pmu;
	sper->itr.snapshot_start = arm_spe_snapshot_start;
	sper->itr.snapshot_finish = arm_spe_snapshot_finish;
	sper->itr.find_snapshot = arm_spe_find_snapshot;
	sper->itr.parse_snapshot_options = arm_spe_parse_snapshot_options;
	sper->itr.recording_options = arm_spe_recording_options;
	sper->itr.info_priv_size = arm_spe_info_priv_size;
	sper->itr.info_fill = arm_spe_info_fill;
	sper->itr.free = arm_spe_recording_free;
	sper->itr.reference = arm_spe_reference;
	sper->itr.read_finish = auxtrace_record__read_finish;
	sper->itr.alignment = 0;

	*err = 0;
	return &sper->itr;
}

struct perf_event_attr
*arm_spe_pmu_default_config(struct perf_pmu *arm_spe_pmu)
{
	struct perf_event_attr *attr;

	attr = zalloc(sizeof(struct perf_event_attr));
	if (!attr) {
		pr_err("arm_spe default config cannot allocate a perf_event_attr\n");
		return NULL;
	}

	 
	if (perf_pmu__scan_file(arm_spe_pmu, "caps/min_interval", "%llu",
				  &attr->sample_period) != 1) {
		pr_debug("arm_spe driver doesn't advertise a min. interval. Using 4096\n");
		attr->sample_period = 4096;
	}

	arm_spe_pmu->selectable = true;
	arm_spe_pmu->is_uncore = false;

	return attr;
}
