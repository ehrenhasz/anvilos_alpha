 
#ifndef __PMU_H
#define __PMU_H

#include <linux/bitmap.h>
#include <linux/compiler.h>
#include <linux/perf_event.h>
#include <linux/list.h>
#include <stdbool.h>
#include <stdio.h>
#include "parse-events.h"
#include "pmu-events/pmu-events.h"

struct evsel_config_term;
struct perf_cpu_map;
struct print_callbacks;

enum {
	PERF_PMU_FORMAT_VALUE_CONFIG,
	PERF_PMU_FORMAT_VALUE_CONFIG1,
	PERF_PMU_FORMAT_VALUE_CONFIG2,
	PERF_PMU_FORMAT_VALUE_CONFIG3,
	PERF_PMU_FORMAT_VALUE_CONFIG_END,
};

#define PERF_PMU_FORMAT_BITS 64
#define MAX_PMU_NAME_LEN 128

struct perf_event_attr;

struct perf_pmu_caps {
	char *name;
	char *value;
	struct list_head list;
};

 
struct perf_pmu {
	 
	const char *name;
	 
	char *alias_name;
	 
	const char *id;
	 
	__u32 type;
	 
	bool selectable;
	 
	bool is_core;
	 
	bool is_uncore;
	 
	bool auxtrace;
	 
	bool formats_checked;
	 
	bool config_masks_present;
	 
	bool config_masks_computed;
	 
	int max_precise;
	 
	struct perf_event_attr *default_config;
	 
	struct perf_cpu_map *cpus;
	 
	struct list_head format;
	 
	struct list_head aliases;
	 
	const struct pmu_events_table *events_table;
	 
	uint32_t sysfs_aliases;
	 
	uint32_t loaded_json_aliases;
	 
	bool sysfs_aliases_loaded;
	 
	bool cpu_aliases_added;
	 
	bool caps_initialized;
	 
	u32 nr_caps;
	 
	struct list_head caps;
	 
	struct list_head list;

	 
	__u64 config_masks[PERF_PMU_FORMAT_VALUE_CONFIG_END];

	 
	struct {
		 
		bool exclude_guest;
	} missing_features;
};

 
extern struct perf_pmu perf_pmu__fake;

struct perf_pmu_info {
	const char *unit;
	double scale;
	bool per_pkg;
	bool snapshot;
};

struct pmu_event_info {
	const struct perf_pmu *pmu;
	const char *name;
	const char* alias;
	const char *scale_unit;
	const char *desc;
	const char *long_desc;
	const char *encoding_desc;
	const char *topic;
	const char *pmu_name;
	const char *str;
	bool deprecated;
};

typedef int (*pmu_event_callback)(void *state, struct pmu_event_info *info);

void pmu_add_sys_aliases(struct perf_pmu *pmu);
int perf_pmu__config(struct perf_pmu *pmu, struct perf_event_attr *attr,
		     struct list_head *head_terms,
		     struct parse_events_error *error);
int perf_pmu__config_terms(struct perf_pmu *pmu,
			   struct perf_event_attr *attr,
			   struct list_head *head_terms,
			   bool zero, struct parse_events_error *error);
__u64 perf_pmu__format_bits(struct perf_pmu *pmu, const char *name);
int perf_pmu__format_type(struct perf_pmu *pmu, const char *name);
int perf_pmu__check_alias(struct perf_pmu *pmu, struct list_head *head_terms,
			  struct perf_pmu_info *info, struct parse_events_error *err);
int perf_pmu__find_event(struct perf_pmu *pmu, const char *event, void *state, pmu_event_callback cb);

int perf_pmu__format_parse(struct perf_pmu *pmu, int dirfd, bool eager_load);
void perf_pmu_format__set_value(void *format, int config, unsigned long *bits);
bool perf_pmu__has_format(const struct perf_pmu *pmu, const char *name);

bool is_pmu_core(const char *name);
bool perf_pmu__supports_legacy_cache(const struct perf_pmu *pmu);
bool perf_pmu__auto_merge_stats(const struct perf_pmu *pmu);
bool perf_pmu__have_event(struct perf_pmu *pmu, const char *name);
size_t perf_pmu__num_events(struct perf_pmu *pmu);
int perf_pmu__for_each_event(struct perf_pmu *pmu, bool skip_duplicate_pmus,
			     void *state, pmu_event_callback cb);
bool pmu__name_match(const struct perf_pmu *pmu, const char *pmu_name);

 
bool perf_pmu__is_software(const struct perf_pmu *pmu);

FILE *perf_pmu__open_file(struct perf_pmu *pmu, const char *name);
FILE *perf_pmu__open_file_at(struct perf_pmu *pmu, int dirfd, const char *name);

int perf_pmu__scan_file(struct perf_pmu *pmu, const char *name, const char *fmt, ...) __scanf(3, 4);
int perf_pmu__scan_file_at(struct perf_pmu *pmu, int dirfd, const char *name,
			   const char *fmt, ...) __scanf(4, 5);

bool perf_pmu__file_exists(struct perf_pmu *pmu, const char *name);

int perf_pmu__test(void);

struct perf_event_attr *perf_pmu__get_default_config(struct perf_pmu *pmu);
void pmu_add_cpu_aliases_table(struct perf_pmu *pmu,
			       const struct pmu_events_table *table);

char *perf_pmu__getcpuid(struct perf_pmu *pmu);
const struct pmu_events_table *pmu_events_table__find(void);
const struct pmu_metrics_table *pmu_metrics_table__find(void);

int perf_pmu__convert_scale(const char *scale, char **end, double *sval);

int perf_pmu__caps_parse(struct perf_pmu *pmu);

void perf_pmu__warn_invalid_config(struct perf_pmu *pmu, __u64 config,
				   const char *name, int config_num,
				   const char *config_name);
void perf_pmu__warn_invalid_formats(struct perf_pmu *pmu);

int perf_pmu__match(const char *pattern, const char *name, const char *tok);

const char *pmu_find_real_name(const char *name);
const char *pmu_find_alias_name(const char *name);
double perf_pmu__cpu_slots_per_cycle(void);
int perf_pmu__event_source_devices_scnprintf(char *pathname, size_t size);
int perf_pmu__pathname_scnprintf(char *buf, size_t size,
				 const char *pmu_name, const char *filename);
int perf_pmu__event_source_devices_fd(void);
int perf_pmu__pathname_fd(int dirfd, const char *pmu_name, const char *filename, int flags);

struct perf_pmu *perf_pmu__lookup(struct list_head *pmus, int dirfd, const char *lookup_name);
struct perf_pmu *perf_pmu__create_placeholder_core_pmu(struct list_head *core_pmus);
void perf_pmu__delete(struct perf_pmu *pmu);
struct perf_pmu *pmu__find_core_pmu(void);

#endif  
