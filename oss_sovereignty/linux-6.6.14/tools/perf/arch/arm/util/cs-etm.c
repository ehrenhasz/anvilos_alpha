
 

#include <api/fs/fs.h>
#include <linux/bits.h>
#include <linux/bitops.h>
#include <linux/compiler.h>
#include <linux/coresight-pmu.h>
#include <linux/kernel.h>
#include <linux/log2.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/zalloc.h>

#include "cs-etm.h"
#include "../../../util/debug.h"
#include "../../../util/record.h"
#include "../../../util/auxtrace.h"
#include "../../../util/cpumap.h"
#include "../../../util/event.h"
#include "../../../util/evlist.h"
#include "../../../util/evsel.h"
#include "../../../util/perf_api_probe.h"
#include "../../../util/evsel_config.h"
#include "../../../util/pmus.h"
#include "../../../util/cs-etm.h"
#include <internal/lib.h> 
#include "../../../util/session.h"

#include <errno.h>
#include <stdlib.h>
#include <sys/stat.h>

struct cs_etm_recording {
	struct auxtrace_record	itr;
	struct perf_pmu		*cs_etm_pmu;
	struct evlist		*evlist;
	bool			snapshot_mode;
	size_t			snapshot_size;
};

static const char *metadata_etmv3_ro[CS_ETM_PRIV_MAX] = {
	[CS_ETM_ETMCCER]	= "mgmt/etmccer",
	[CS_ETM_ETMIDR]		= "mgmt/etmidr",
};

static const char * const metadata_etmv4_ro[] = {
	[CS_ETMV4_TRCIDR0]		= "trcidr/trcidr0",
	[CS_ETMV4_TRCIDR1]		= "trcidr/trcidr1",
	[CS_ETMV4_TRCIDR2]		= "trcidr/trcidr2",
	[CS_ETMV4_TRCIDR8]		= "trcidr/trcidr8",
	[CS_ETMV4_TRCAUTHSTATUS]	= "mgmt/trcauthstatus",
	[CS_ETMV4_TS_SOURCE]		= "ts_source",
};

static const char * const metadata_ete_ro[] = {
	[CS_ETE_TRCIDR0]		= "trcidr/trcidr0",
	[CS_ETE_TRCIDR1]		= "trcidr/trcidr1",
	[CS_ETE_TRCIDR2]		= "trcidr/trcidr2",
	[CS_ETE_TRCIDR8]		= "trcidr/trcidr8",
	[CS_ETE_TRCAUTHSTATUS]		= "mgmt/trcauthstatus",
	[CS_ETE_TRCDEVARCH]		= "mgmt/trcdevarch",
	[CS_ETE_TS_SOURCE]		= "ts_source",
};

static bool cs_etm_is_etmv4(struct auxtrace_record *itr, int cpu);
static bool cs_etm_is_ete(struct auxtrace_record *itr, int cpu);

static int cs_etm_validate_context_id(struct auxtrace_record *itr,
				      struct evsel *evsel, int cpu)
{
	struct cs_etm_recording *ptr =
		container_of(itr, struct cs_etm_recording, itr);
	struct perf_pmu *cs_etm_pmu = ptr->cs_etm_pmu;
	char path[PATH_MAX];
	int err;
	u32 val;
	u64 contextid = evsel->core.attr.config &
		(perf_pmu__format_bits(cs_etm_pmu, "contextid") |
		 perf_pmu__format_bits(cs_etm_pmu, "contextid1") |
		 perf_pmu__format_bits(cs_etm_pmu, "contextid2"));

	if (!contextid)
		return 0;

	 
	if (!cs_etm_is_etmv4(itr, cpu)) {
		pr_err("%s: contextid not supported in ETMv3, disable with %s/contextid=0/\n",
		       CORESIGHT_ETM_PMU_NAME, CORESIGHT_ETM_PMU_NAME);
		return -EINVAL;
	}

	 
	snprintf(path, PATH_MAX, "cpu%d/%s",
		 cpu, metadata_etmv4_ro[CS_ETMV4_TRCIDR2]);
	err = perf_pmu__scan_file(cs_etm_pmu, path, "%x", &val);

	 
	if (err != 1) {
		pr_err("%s: can't read file %s\n", CORESIGHT_ETM_PMU_NAME,
		       path);
		return err;
	}

	if (contextid &
	    perf_pmu__format_bits(cs_etm_pmu, "contextid1")) {
		 
		if (BMVAL(val, 5, 9) != 0x4) {
			pr_err("%s: CONTEXTIDR_EL1 isn't supported, disable with %s/contextid1=0/\n",
			       CORESIGHT_ETM_PMU_NAME, CORESIGHT_ETM_PMU_NAME);
			return -EINVAL;
		}
	}

	if (contextid &
	    perf_pmu__format_bits(cs_etm_pmu, "contextid2")) {
		 
		if (!BMVAL(val, 29, 30) || BMVAL(val, 10, 14) < 4) {
			pr_err("%s: CONTEXTIDR_EL2 isn't supported, disable with %s/contextid2=0/\n",
			       CORESIGHT_ETM_PMU_NAME, CORESIGHT_ETM_PMU_NAME);
			return -EINVAL;
		}
	}

	return 0;
}

static int cs_etm_validate_timestamp(struct auxtrace_record *itr,
				     struct evsel *evsel, int cpu)
{
	struct cs_etm_recording *ptr =
		container_of(itr, struct cs_etm_recording, itr);
	struct perf_pmu *cs_etm_pmu = ptr->cs_etm_pmu;
	char path[PATH_MAX];
	int err;
	u32 val;

	if (!(evsel->core.attr.config &
	      perf_pmu__format_bits(cs_etm_pmu, "timestamp")))
		return 0;

	if (!cs_etm_is_etmv4(itr, cpu)) {
		pr_err("%s: timestamp not supported in ETMv3, disable with %s/timestamp=0/\n",
		       CORESIGHT_ETM_PMU_NAME, CORESIGHT_ETM_PMU_NAME);
		return -EINVAL;
	}

	 
	snprintf(path, PATH_MAX, "cpu%d/%s",
		 cpu, metadata_etmv4_ro[CS_ETMV4_TRCIDR0]);
	err = perf_pmu__scan_file(cs_etm_pmu, path, "%x", &val);

	 
	if (err != 1) {
		pr_err("%s: can't read file %s\n",
		       CORESIGHT_ETM_PMU_NAME, path);
		return err;
	}

	 
	val &= GENMASK(28, 24);
	if (!val) {
		return -EINVAL;
	}

	return 0;
}

 
static int cs_etm_validate_config(struct auxtrace_record *itr,
				  struct evsel *evsel)
{
	int i, err = -EINVAL;
	struct perf_cpu_map *event_cpus = evsel->evlist->core.user_requested_cpus;
	struct perf_cpu_map *online_cpus = perf_cpu_map__new(NULL);

	 
	for (i = 0; i < cpu__max_cpu().cpu; i++) {
		struct perf_cpu cpu = { .cpu = i, };

		if (!perf_cpu_map__has(event_cpus, cpu) ||
		    !perf_cpu_map__has(online_cpus, cpu))
			continue;

		err = cs_etm_validate_context_id(itr, evsel, i);
		if (err)
			goto out;
		err = cs_etm_validate_timestamp(itr, evsel, i);
		if (err)
			goto out;
	}

	err = 0;
out:
	perf_cpu_map__put(online_cpus);
	return err;
}

static int cs_etm_parse_snapshot_options(struct auxtrace_record *itr,
					 struct record_opts *opts,
					 const char *str)
{
	struct cs_etm_recording *ptr =
				container_of(itr, struct cs_etm_recording, itr);
	unsigned long long snapshot_size = 0;
	char *endptr;

	if (str) {
		snapshot_size = strtoull(str, &endptr, 0);
		if (*endptr || snapshot_size > SIZE_MAX)
			return -1;
	}

	opts->auxtrace_snapshot_mode = true;
	opts->auxtrace_snapshot_size = snapshot_size;
	ptr->snapshot_size = snapshot_size;

	return 0;
}

static int cs_etm_set_sink_attr(struct perf_pmu *pmu,
				struct evsel *evsel)
{
	char msg[BUFSIZ], path[PATH_MAX], *sink;
	struct evsel_config_term *term;
	int ret = -EINVAL;
	u32 hash;

	if (evsel->core.attr.config2 & GENMASK(31, 0))
		return 0;

	list_for_each_entry(term, &evsel->config_terms, list) {
		if (term->type != EVSEL__CONFIG_TERM_DRV_CFG)
			continue;

		sink = term->val.str;
		snprintf(path, PATH_MAX, "sinks/%s", sink);

		ret = perf_pmu__scan_file(pmu, path, "%x", &hash);
		if (ret != 1) {
			if (errno == ENOENT)
				pr_err("Couldn't find sink \"%s\" on event %s\n"
				       "Missing kernel or device support?\n\n"
				       "Hint: An appropriate sink will be picked automatically if one isn't specified.\n",
				       sink, evsel__name(evsel));
			else
				pr_err("Failed to set sink \"%s\" on event %s with %d (%s)\n",
				       sink, evsel__name(evsel), errno,
				       str_error_r(errno, msg, sizeof(msg)));
			return ret;
		}

		evsel->core.attr.config2 |= hash;
		return 0;
	}

	 
	return 0;
}

static int cs_etm_recording_options(struct auxtrace_record *itr,
				    struct evlist *evlist,
				    struct record_opts *opts)
{
	int ret;
	struct cs_etm_recording *ptr =
				container_of(itr, struct cs_etm_recording, itr);
	struct perf_pmu *cs_etm_pmu = ptr->cs_etm_pmu;
	struct evsel *evsel, *cs_etm_evsel = NULL;
	struct perf_cpu_map *cpus = evlist->core.user_requested_cpus;
	bool privileged = perf_event_paranoid_check(-1);
	int err = 0;

	evlist__for_each_entry(evlist, evsel) {
		if (evsel->core.attr.type == cs_etm_pmu->type) {
			if (cs_etm_evsel) {
				pr_err("There may be only one %s event\n",
				       CORESIGHT_ETM_PMU_NAME);
				return -EINVAL;
			}
			cs_etm_evsel = evsel;
		}
	}

	 
	if (!cs_etm_evsel)
		return 0;

	ptr->evlist = evlist;
	ptr->snapshot_mode = opts->auxtrace_snapshot_mode;

	if (!record_opts__no_switch_events(opts) &&
	    perf_can_record_switch_events())
		opts->record_switch_events = true;

	cs_etm_evsel->needs_auxtrace_mmap = true;
	opts->full_auxtrace = true;

	ret = cs_etm_set_sink_attr(cs_etm_pmu, cs_etm_evsel);
	if (ret)
		return ret;

	if (opts->use_clockid) {
		pr_err("Cannot use clockid (-k option) with %s\n",
		       CORESIGHT_ETM_PMU_NAME);
		return -EINVAL;
	}

	 
	if (opts->auxtrace_snapshot_mode) {
		 
		if (!opts->auxtrace_snapshot_size &&
		    !opts->auxtrace_mmap_pages) {
			if (privileged) {
				opts->auxtrace_mmap_pages = MiB(4) / page_size;
			} else {
				opts->auxtrace_mmap_pages =
							KiB(128) / page_size;
				if (opts->mmap_pages == UINT_MAX)
					opts->mmap_pages = KiB(256) / page_size;
			}
		} else if (!opts->auxtrace_mmap_pages && !privileged &&
						opts->mmap_pages == UINT_MAX) {
			opts->mmap_pages = KiB(256) / page_size;
		}

		 
		if (!opts->auxtrace_snapshot_size) {
			opts->auxtrace_snapshot_size =
				opts->auxtrace_mmap_pages * (size_t)page_size;
		}

		 
		if (!opts->auxtrace_mmap_pages) {
			size_t sz = opts->auxtrace_snapshot_size;

			sz = round_up(sz, page_size) / page_size;
			opts->auxtrace_mmap_pages = roundup_pow_of_two(sz);
		}

		 
		if (opts->auxtrace_snapshot_size >
				opts->auxtrace_mmap_pages * (size_t)page_size) {
			pr_err("Snapshot size %zu must not be greater than AUX area tracing mmap size %zu\n",
			       opts->auxtrace_snapshot_size,
			       opts->auxtrace_mmap_pages * (size_t)page_size);
			return -EINVAL;
		}

		 
		if (!opts->auxtrace_snapshot_size ||
		    !opts->auxtrace_mmap_pages) {
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

	if (opts->auxtrace_snapshot_mode)
		pr_debug2("%s snapshot size: %zu\n", CORESIGHT_ETM_PMU_NAME,
			  opts->auxtrace_snapshot_size);

	 
	evlist__to_front(evlist, cs_etm_evsel);

	 
	evsel__set_sample_bit(cs_etm_evsel, CPU);

	 
	if (!perf_cpu_map__empty(cpus)) {
		evsel__set_config_if_unset(cs_etm_pmu, cs_etm_evsel,
					   "timestamp", 1);
		evsel__set_config_if_unset(cs_etm_pmu, cs_etm_evsel,
					   "contextid", 1);
	}

	 
	err = parse_event(evlist, "dummy:u");
	if (err)
		goto out;
	evsel = evlist__last(evlist);
	evlist__set_tracking_event(evlist, evsel);
	evsel->core.attr.freq = 0;
	evsel->core.attr.sample_period = 1;

	 
	if (!perf_cpu_map__empty(cpus))
		evsel__set_sample_bit(evsel, TIME);

	err = cs_etm_validate_config(itr, cs_etm_evsel);
out:
	return err;
}

static u64 cs_etm_get_config(struct auxtrace_record *itr)
{
	u64 config = 0;
	struct cs_etm_recording *ptr =
			container_of(itr, struct cs_etm_recording, itr);
	struct perf_pmu *cs_etm_pmu = ptr->cs_etm_pmu;
	struct evlist *evlist = ptr->evlist;
	struct evsel *evsel;

	evlist__for_each_entry(evlist, evsel) {
		if (evsel->core.attr.type == cs_etm_pmu->type) {
			 
			config = evsel->core.attr.config;
			break;
		}
	}

	return config;
}

#ifndef BIT
#define BIT(N) (1UL << (N))
#endif

static u64 cs_etmv4_get_config(struct auxtrace_record *itr)
{
	u64 config = 0;
	u64 config_opts = 0;

	 
	config_opts = cs_etm_get_config(itr);
	if (config_opts & BIT(ETM_OPT_CYCACC))
		config |= BIT(ETM4_CFG_BIT_CYCACC);
	if (config_opts & BIT(ETM_OPT_CTXTID))
		config |= BIT(ETM4_CFG_BIT_CTXTID);
	if (config_opts & BIT(ETM_OPT_TS))
		config |= BIT(ETM4_CFG_BIT_TS);
	if (config_opts & BIT(ETM_OPT_RETSTK))
		config |= BIT(ETM4_CFG_BIT_RETSTK);
	if (config_opts & BIT(ETM_OPT_CTXTID2))
		config |= BIT(ETM4_CFG_BIT_VMID) |
			  BIT(ETM4_CFG_BIT_VMID_OPT);
	if (config_opts & BIT(ETM_OPT_BRANCH_BROADCAST))
		config |= BIT(ETM4_CFG_BIT_BB);

	return config;
}

static size_t
cs_etm_info_priv_size(struct auxtrace_record *itr __maybe_unused,
		      struct evlist *evlist __maybe_unused)
{
	int i;
	int etmv3 = 0, etmv4 = 0, ete = 0;
	struct perf_cpu_map *event_cpus = evlist->core.user_requested_cpus;
	struct perf_cpu_map *online_cpus = perf_cpu_map__new(NULL);

	 
	if (!perf_cpu_map__empty(event_cpus)) {
		for (i = 0; i < cpu__max_cpu().cpu; i++) {
			struct perf_cpu cpu = { .cpu = i, };

			if (!perf_cpu_map__has(event_cpus, cpu) ||
			    !perf_cpu_map__has(online_cpus, cpu))
				continue;

			if (cs_etm_is_ete(itr, i))
				ete++;
			else if (cs_etm_is_etmv4(itr, i))
				etmv4++;
			else
				etmv3++;
		}
	} else {
		 
		for (i = 0; i < cpu__max_cpu().cpu; i++) {
			struct perf_cpu cpu = { .cpu = i, };

			if (!perf_cpu_map__has(online_cpus, cpu))
				continue;

			if (cs_etm_is_ete(itr, i))
				ete++;
			else if (cs_etm_is_etmv4(itr, i))
				etmv4++;
			else
				etmv3++;
		}
	}

	perf_cpu_map__put(online_cpus);

	return (CS_ETM_HEADER_SIZE +
	       (ete   * CS_ETE_PRIV_SIZE) +
	       (etmv4 * CS_ETMV4_PRIV_SIZE) +
	       (etmv3 * CS_ETMV3_PRIV_SIZE));
}

static bool cs_etm_is_etmv4(struct auxtrace_record *itr, int cpu)
{
	bool ret = false;
	char path[PATH_MAX];
	int scan;
	unsigned int val;
	struct cs_etm_recording *ptr =
			container_of(itr, struct cs_etm_recording, itr);
	struct perf_pmu *cs_etm_pmu = ptr->cs_etm_pmu;

	 
	snprintf(path, PATH_MAX, "cpu%d/%s",
		 cpu, metadata_etmv4_ro[CS_ETMV4_TRCIDR0]);
	scan = perf_pmu__scan_file(cs_etm_pmu, path, "%x", &val);

	 
	if (scan == 1)
		ret = true;

	return ret;
}

static int cs_etm_get_ro(struct perf_pmu *pmu, int cpu, const char *path)
{
	char pmu_path[PATH_MAX];
	int scan;
	unsigned int val = 0;

	 
	snprintf(pmu_path, PATH_MAX, "cpu%d/%s", cpu, path);

	scan = perf_pmu__scan_file(pmu, pmu_path, "%x", &val);
	if (scan != 1)
		pr_err("%s: error reading: %s\n", __func__, pmu_path);

	return val;
}

static int cs_etm_get_ro_signed(struct perf_pmu *pmu, int cpu, const char *path)
{
	char pmu_path[PATH_MAX];
	int scan;
	int val = 0;

	 
	snprintf(pmu_path, PATH_MAX, "cpu%d/%s", cpu, path);

	scan = perf_pmu__scan_file(pmu, pmu_path, "%d", &val);
	if (scan != 1)
		pr_err("%s: error reading: %s\n", __func__, pmu_path);

	return val;
}

static bool cs_etm_pmu_path_exists(struct perf_pmu *pmu, int cpu, const char *path)
{
	char pmu_path[PATH_MAX];

	 
	snprintf(pmu_path, PATH_MAX, "cpu%d/%s", cpu, path);

	return perf_pmu__file_exists(pmu, pmu_path);
}

#define TRCDEVARCH_ARCHPART_SHIFT 0
#define TRCDEVARCH_ARCHPART_MASK  GENMASK(11, 0)
#define TRCDEVARCH_ARCHPART(x)    (((x) & TRCDEVARCH_ARCHPART_MASK) >> TRCDEVARCH_ARCHPART_SHIFT)

#define TRCDEVARCH_ARCHVER_SHIFT 12
#define TRCDEVARCH_ARCHVER_MASK  GENMASK(15, 12)
#define TRCDEVARCH_ARCHVER(x)    (((x) & TRCDEVARCH_ARCHVER_MASK) >> TRCDEVARCH_ARCHVER_SHIFT)

static bool cs_etm_is_ete(struct auxtrace_record *itr, int cpu)
{
	struct cs_etm_recording *ptr = container_of(itr, struct cs_etm_recording, itr);
	struct perf_pmu *cs_etm_pmu = ptr->cs_etm_pmu;
	int trcdevarch;

	if (!cs_etm_pmu_path_exists(cs_etm_pmu, cpu, metadata_ete_ro[CS_ETE_TRCDEVARCH]))
		return false;

	trcdevarch = cs_etm_get_ro(cs_etm_pmu, cpu, metadata_ete_ro[CS_ETE_TRCDEVARCH]);
	 
	return TRCDEVARCH_ARCHVER(trcdevarch) == 5 && TRCDEVARCH_ARCHPART(trcdevarch) == 0xA13;
}

static void cs_etm_save_etmv4_header(__u64 data[], struct auxtrace_record *itr, int cpu)
{
	struct cs_etm_recording *ptr = container_of(itr, struct cs_etm_recording, itr);
	struct perf_pmu *cs_etm_pmu = ptr->cs_etm_pmu;

	 
	data[CS_ETMV4_TRCCONFIGR] = cs_etmv4_get_config(itr);
	 
	data[CS_ETMV4_TRCTRACEIDR] =
		CORESIGHT_LEGACY_CPU_TRACE_ID(cpu) | CORESIGHT_TRACE_ID_UNUSED_FLAG;

	 
	data[CS_ETMV4_TRCIDR0] = cs_etm_get_ro(cs_etm_pmu, cpu,
					       metadata_etmv4_ro[CS_ETMV4_TRCIDR0]);
	data[CS_ETMV4_TRCIDR1] = cs_etm_get_ro(cs_etm_pmu, cpu,
					       metadata_etmv4_ro[CS_ETMV4_TRCIDR1]);
	data[CS_ETMV4_TRCIDR2] = cs_etm_get_ro(cs_etm_pmu, cpu,
					       metadata_etmv4_ro[CS_ETMV4_TRCIDR2]);
	data[CS_ETMV4_TRCIDR8] = cs_etm_get_ro(cs_etm_pmu, cpu,
					       metadata_etmv4_ro[CS_ETMV4_TRCIDR8]);
	data[CS_ETMV4_TRCAUTHSTATUS] = cs_etm_get_ro(cs_etm_pmu, cpu,
						     metadata_etmv4_ro[CS_ETMV4_TRCAUTHSTATUS]);

	 
	if (cs_etm_pmu_path_exists(cs_etm_pmu, cpu, metadata_etmv4_ro[CS_ETMV4_TS_SOURCE]))
		data[CS_ETMV4_TS_SOURCE] = (__u64) cs_etm_get_ro_signed(cs_etm_pmu, cpu,
				metadata_etmv4_ro[CS_ETMV4_TS_SOURCE]);
	else {
		pr_debug3("[%03d] pmu file 'ts_source' not found. Fallback to safe value (-1)\n",
			  cpu);
		data[CS_ETMV4_TS_SOURCE] = (__u64) -1;
	}
}

static void cs_etm_save_ete_header(__u64 data[], struct auxtrace_record *itr, int cpu)
{
	struct cs_etm_recording *ptr = container_of(itr, struct cs_etm_recording, itr);
	struct perf_pmu *cs_etm_pmu = ptr->cs_etm_pmu;

	 
	data[CS_ETE_TRCCONFIGR] = cs_etmv4_get_config(itr);
	 
	data[CS_ETE_TRCTRACEIDR] =
		CORESIGHT_LEGACY_CPU_TRACE_ID(cpu) | CORESIGHT_TRACE_ID_UNUSED_FLAG;

	 
	data[CS_ETE_TRCIDR0] = cs_etm_get_ro(cs_etm_pmu, cpu,
					     metadata_ete_ro[CS_ETE_TRCIDR0]);
	data[CS_ETE_TRCIDR1] = cs_etm_get_ro(cs_etm_pmu, cpu,
					     metadata_ete_ro[CS_ETE_TRCIDR1]);
	data[CS_ETE_TRCIDR2] = cs_etm_get_ro(cs_etm_pmu, cpu,
					     metadata_ete_ro[CS_ETE_TRCIDR2]);
	data[CS_ETE_TRCIDR8] = cs_etm_get_ro(cs_etm_pmu, cpu,
					     metadata_ete_ro[CS_ETE_TRCIDR8]);
	data[CS_ETE_TRCAUTHSTATUS] = cs_etm_get_ro(cs_etm_pmu, cpu,
						   metadata_ete_ro[CS_ETE_TRCAUTHSTATUS]);
	 
	data[CS_ETE_TRCDEVARCH] = cs_etm_get_ro(cs_etm_pmu, cpu,
						metadata_ete_ro[CS_ETE_TRCDEVARCH]);

	 
	if (cs_etm_pmu_path_exists(cs_etm_pmu, cpu, metadata_ete_ro[CS_ETE_TS_SOURCE]))
		data[CS_ETE_TS_SOURCE] = (__u64) cs_etm_get_ro_signed(cs_etm_pmu, cpu,
				metadata_ete_ro[CS_ETE_TS_SOURCE]);
	else {
		pr_debug3("[%03d] pmu file 'ts_source' not found. Fallback to safe value (-1)\n",
			  cpu);
		data[CS_ETE_TS_SOURCE] = (__u64) -1;
	}
}

static void cs_etm_get_metadata(int cpu, u32 *offset,
				struct auxtrace_record *itr,
				struct perf_record_auxtrace_info *info)
{
	u32 increment, nr_trc_params;
	u64 magic;
	struct cs_etm_recording *ptr =
			container_of(itr, struct cs_etm_recording, itr);
	struct perf_pmu *cs_etm_pmu = ptr->cs_etm_pmu;

	 
	if (cs_etm_is_ete(itr, cpu)) {
		magic = __perf_cs_ete_magic;
		cs_etm_save_ete_header(&info->priv[*offset], itr, cpu);

		 
		increment = CS_ETE_PRIV_MAX;
		nr_trc_params = CS_ETE_PRIV_MAX - CS_ETM_COMMON_BLK_MAX_V1;
	} else if (cs_etm_is_etmv4(itr, cpu)) {
		magic = __perf_cs_etmv4_magic;
		cs_etm_save_etmv4_header(&info->priv[*offset], itr, cpu);

		 
		increment = CS_ETMV4_PRIV_MAX;
		nr_trc_params = CS_ETMV4_PRIV_MAX - CS_ETMV4_TRCCONFIGR;
	} else {
		magic = __perf_cs_etmv3_magic;
		 
		info->priv[*offset + CS_ETM_ETMCR] = cs_etm_get_config(itr);
		 
		info->priv[*offset + CS_ETM_ETMTRACEIDR] =
			CORESIGHT_LEGACY_CPU_TRACE_ID(cpu) | CORESIGHT_TRACE_ID_UNUSED_FLAG;
		 
		info->priv[*offset + CS_ETM_ETMCCER] =
			cs_etm_get_ro(cs_etm_pmu, cpu,
				      metadata_etmv3_ro[CS_ETM_ETMCCER]);
		info->priv[*offset + CS_ETM_ETMIDR] =
			cs_etm_get_ro(cs_etm_pmu, cpu,
				      metadata_etmv3_ro[CS_ETM_ETMIDR]);

		 
		increment = CS_ETM_PRIV_MAX;
		nr_trc_params = CS_ETM_PRIV_MAX - CS_ETM_ETMCR;
	}

	 
	info->priv[*offset + CS_ETM_MAGIC] = magic;
	info->priv[*offset + CS_ETM_CPU] = cpu;
	info->priv[*offset + CS_ETM_NR_TRC_PARAMS] = nr_trc_params;
	 
	*offset += increment;
}

static int cs_etm_info_fill(struct auxtrace_record *itr,
			    struct perf_session *session,
			    struct perf_record_auxtrace_info *info,
			    size_t priv_size)
{
	int i;
	u32 offset;
	u64 nr_cpu, type;
	struct perf_cpu_map *cpu_map;
	struct perf_cpu_map *event_cpus = session->evlist->core.user_requested_cpus;
	struct perf_cpu_map *online_cpus = perf_cpu_map__new(NULL);
	struct cs_etm_recording *ptr =
			container_of(itr, struct cs_etm_recording, itr);
	struct perf_pmu *cs_etm_pmu = ptr->cs_etm_pmu;

	if (priv_size != cs_etm_info_priv_size(itr, session->evlist))
		return -EINVAL;

	if (!session->evlist->core.nr_mmaps)
		return -EINVAL;

	 
	if (perf_cpu_map__empty(event_cpus)) {
		cpu_map = online_cpus;
	} else {
		 
		for (i = 0; i < perf_cpu_map__nr(event_cpus); i++) {
			struct perf_cpu cpu = { .cpu = i, };

			if (perf_cpu_map__has(event_cpus, cpu) &&
			    !perf_cpu_map__has(online_cpus, cpu))
				return -EINVAL;
		}

		cpu_map = event_cpus;
	}

	nr_cpu = perf_cpu_map__nr(cpu_map);
	 
	type = cs_etm_pmu->type;

	 
	info->type = PERF_AUXTRACE_CS_ETM;
	info->priv[CS_HEADER_VERSION] = CS_HEADER_CURRENT_VERSION;
	info->priv[CS_PMU_TYPE_CPUS] = type << 32;
	info->priv[CS_PMU_TYPE_CPUS] |= nr_cpu;
	info->priv[CS_ETM_SNAPSHOT] = ptr->snapshot_mode;

	offset = CS_ETM_SNAPSHOT + 1;

	for (i = 0; i < cpu__max_cpu().cpu && offset < priv_size; i++) {
		struct perf_cpu cpu = { .cpu = i, };

		if (perf_cpu_map__has(cpu_map, cpu))
			cs_etm_get_metadata(i, &offset, itr, info);
	}

	perf_cpu_map__put(online_cpus);

	return 0;
}

static int cs_etm_snapshot_start(struct auxtrace_record *itr)
{
	struct cs_etm_recording *ptr =
			container_of(itr, struct cs_etm_recording, itr);
	struct evsel *evsel;

	evlist__for_each_entry(ptr->evlist, evsel) {
		if (evsel->core.attr.type == ptr->cs_etm_pmu->type)
			return evsel__disable(evsel);
	}
	return -EINVAL;
}

static int cs_etm_snapshot_finish(struct auxtrace_record *itr)
{
	struct cs_etm_recording *ptr =
			container_of(itr, struct cs_etm_recording, itr);
	struct evsel *evsel;

	evlist__for_each_entry(ptr->evlist, evsel) {
		if (evsel->core.attr.type == ptr->cs_etm_pmu->type)
			return evsel__enable(evsel);
	}
	return -EINVAL;
}

static u64 cs_etm_reference(struct auxtrace_record *itr __maybe_unused)
{
	return (((u64) rand() <<  0) & 0x00000000FFFFFFFFull) |
		(((u64) rand() << 32) & 0xFFFFFFFF00000000ull);
}

static void cs_etm_recording_free(struct auxtrace_record *itr)
{
	struct cs_etm_recording *ptr =
			container_of(itr, struct cs_etm_recording, itr);

	free(ptr);
}

struct auxtrace_record *cs_etm_record_init(int *err)
{
	struct perf_pmu *cs_etm_pmu;
	struct cs_etm_recording *ptr;

	cs_etm_pmu = perf_pmus__find(CORESIGHT_ETM_PMU_NAME);

	if (!cs_etm_pmu) {
		*err = -EINVAL;
		goto out;
	}

	ptr = zalloc(sizeof(struct cs_etm_recording));
	if (!ptr) {
		*err = -ENOMEM;
		goto out;
	}

	ptr->cs_etm_pmu			= cs_etm_pmu;
	ptr->itr.pmu			= cs_etm_pmu;
	ptr->itr.parse_snapshot_options	= cs_etm_parse_snapshot_options;
	ptr->itr.recording_options	= cs_etm_recording_options;
	ptr->itr.info_priv_size		= cs_etm_info_priv_size;
	ptr->itr.info_fill		= cs_etm_info_fill;
	ptr->itr.snapshot_start		= cs_etm_snapshot_start;
	ptr->itr.snapshot_finish	= cs_etm_snapshot_finish;
	ptr->itr.reference		= cs_etm_reference;
	ptr->itr.free			= cs_etm_recording_free;
	ptr->itr.read_finish		= auxtrace_record__read_finish;

	*err = 0;
	return &ptr->itr;
out:
	return NULL;
}

 
struct perf_event_attr *
cs_etm_get_default_config(struct perf_pmu *pmu __maybe_unused)
{
	struct perf_event_attr *attr;

	attr = zalloc(sizeof(struct perf_event_attr));
	if (!attr)
		return NULL;

	attr->sample_period = 1;

	return attr;
}
