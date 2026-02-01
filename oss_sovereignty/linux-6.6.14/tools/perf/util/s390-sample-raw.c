
 

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include <sys/stat.h>
#include <linux/compiler.h>
#include <asm/byteorder.h>

#include "debug.h"
#include "session.h"
#include "evlist.h"
#include "color.h"
#include "sample-raw.h"
#include "s390-cpumcf-kernel.h"
#include "util/pmu.h"
#include "util/sample.h"

static size_t ctrset_size(struct cf_ctrset_entry *set)
{
	return sizeof(*set) + set->ctr * sizeof(u64);
}

static bool ctrset_valid(struct cf_ctrset_entry *set)
{
	return set->def == S390_CPUMCF_DIAG_DEF;
}

 
static bool s390_cpumcfdg_testctr(struct perf_sample *sample)
{
	size_t len = sample->raw_size, offset = 0;
	unsigned char *buf = sample->raw_data;
	struct cf_trailer_entry *te;
	struct cf_ctrset_entry *cep, ce;

	if (!len)
		return false;
	while (offset < len) {
		cep = (struct cf_ctrset_entry *)(buf + offset);
		ce.def = be16_to_cpu(cep->def);
		ce.set = be16_to_cpu(cep->set);
		ce.ctr = be16_to_cpu(cep->ctr);
		ce.res1 = be16_to_cpu(cep->res1);

		if (!ctrset_valid(&ce) || offset + ctrset_size(&ce) > len) {
			 
			if (len - offset - sizeof(*te) == 4)
				break;
			pr_err("Invalid counter set entry at %zd\n", offset);
			return false;
		}
		offset += ctrset_size(&ce);
	}
	return true;
}

 
static void s390_cpumcfdg_dumptrail(const char *color, size_t offset,
				    struct cf_trailer_entry *tep)
{
	struct cf_trailer_entry  te;

	te.flags = be64_to_cpu(tep->flags);
	te.cfvn = be16_to_cpu(tep->cfvn);
	te.csvn = be16_to_cpu(tep->csvn);
	te.cpu_speed = be32_to_cpu(tep->cpu_speed);
	te.timestamp = be64_to_cpu(tep->timestamp);
	te.progusage1 = be64_to_cpu(tep->progusage1);
	te.progusage2 = be64_to_cpu(tep->progusage2);
	te.progusage3 = be64_to_cpu(tep->progusage3);
	te.tod_base = be64_to_cpu(tep->tod_base);
	te.mach_type = be16_to_cpu(tep->mach_type);
	te.res1 = be16_to_cpu(tep->res1);
	te.res2 = be32_to_cpu(tep->res2);

	color_fprintf(stdout, color, "    [%#08zx] Trailer:%c%c%c%c%c"
		      " Cfvn:%d Csvn:%d Speed:%d TOD:%#llx\n",
		      offset, te.clock_base ? 'T' : ' ',
		      te.speed ? 'S' : ' ', te.mtda ? 'M' : ' ',
		      te.caca ? 'C' : ' ', te.lcda ? 'L' : ' ',
		      te.cfvn, te.csvn, te.cpu_speed, te.timestamp);
	color_fprintf(stdout, color, "\t\t1:%lx 2:%lx 3:%lx TOD-Base:%#llx"
		      " Type:%x\n\n",
		      te.progusage1, te.progusage2, te.progusage3,
		      te.tod_base, te.mach_type);
}

 
static int get_counterset_start(int setnr)
{
	switch (setnr) {
	case CPUMF_CTR_SET_BASIC:		 
		return 0;
	case CPUMF_CTR_SET_USER:		 
		return 32;
	case CPUMF_CTR_SET_CRYPTO:		 
		return 64;
	case CPUMF_CTR_SET_EXT:			 
		return 128;
	case CPUMF_CTR_SET_MT_DIAG:		 
		return 448;
	default:
		return -1;
	}
}

struct get_counter_name_data {
	int wanted;
	char *result;
};

static int get_counter_name_callback(void *vdata, struct pmu_event_info *info)
{
	struct get_counter_name_data *data = vdata;
	int rc, event_nr;
	const char *event_str;

	if (info->str == NULL)
		return 0;

	event_str = strstr(info->str, "event=");
	if (!event_str)
		return 0;

	rc = sscanf(event_str, "event=%x", &event_nr);
	if (rc == 1 && event_nr == data->wanted) {
		data->result = strdup(info->name);
		return 1;  
	}
	return 0;
}

 
static char *get_counter_name(int set, int nr, struct perf_pmu *pmu)
{
	struct get_counter_name_data data = {
		.wanted = get_counterset_start(set) + nr,
		.result = NULL,
	};

	if (!pmu)
		return NULL;

	perf_pmu__for_each_event(pmu,   true,
				 &data, get_counter_name_callback);
	return data.result;
}

static void s390_cpumcfdg_dump(struct perf_pmu *pmu, struct perf_sample *sample)
{
	size_t i, len = sample->raw_size, offset = 0;
	unsigned char *buf = sample->raw_data;
	const char *color = PERF_COLOR_BLUE;
	struct cf_ctrset_entry *cep, ce;
	u64 *p;

	while (offset < len) {
		cep = (struct cf_ctrset_entry *)(buf + offset);

		ce.def = be16_to_cpu(cep->def);
		ce.set = be16_to_cpu(cep->set);
		ce.ctr = be16_to_cpu(cep->ctr);
		ce.res1 = be16_to_cpu(cep->res1);

		if (!ctrset_valid(&ce)) {	 
			s390_cpumcfdg_dumptrail(color, offset,
						(struct cf_trailer_entry *)cep);
			return;
		}

		color_fprintf(stdout, color, "    [%#08zx] Counterset:%d"
			      " Counters:%d\n", offset, ce.set, ce.ctr);
		for (i = 0, p = (u64 *)(cep + 1); i < ce.ctr; ++i, ++p) {
			char *ev_name = get_counter_name(ce.set, i, pmu);

			color_fprintf(stdout, color,
				      "\tCounter:%03d %s Value:%#018lx\n", i,
				      ev_name ?: "<unknown>", be64_to_cpu(*p));
			free(ev_name);
		}
		offset += ctrset_size(&ce);
	}
}

 
void evlist__s390_sample_raw(struct evlist *evlist, union perf_event *event, struct perf_sample *sample)
{
	struct evsel *evsel;

	if (event->header.type != PERF_RECORD_SAMPLE)
		return;

	evsel = evlist__event2evsel(evlist, event);
	if (evsel == NULL ||
	    evsel->core.attr.config != PERF_EVENT_CPUM_CF_DIAG)
		return;

	 
	if (!s390_cpumcfdg_testctr(sample)) {
		pr_err("Invalid counter set data encountered\n");
		return;
	}
	s390_cpumcfdg_dump(evsel->pmu, sample);
}
