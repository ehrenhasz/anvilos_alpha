
#ifndef METRICGROUP_H
#define METRICGROUP_H 1

#include <linux/list.h>
#include <linux/rbtree.h>
#include <stdbool.h>
#include "pmu-events/pmu-events.h"

struct evlist;
struct evsel;
struct option;
struct print_callbacks;
struct rblist;
struct cgroup;

 
struct metric_event {
	struct rb_node nd;
	struct evsel *evsel;
	bool is_default;  
	struct list_head head;  
};

 
struct metric_ref {
	const char *metric_name;
	const char *metric_expr;
};

 
struct metric_expr {
	struct list_head nd;
	 
	const char *metric_expr;
	 
	const char *metric_name;
	const char *metric_threshold;
	 
	const char *metric_unit;
	 
	const char *default_metricgroup_name;
	 
	struct evsel **metric_events;
	 
	struct metric_ref *metric_refs;
	 
	int runtime;
};

struct metric_event *metricgroup__lookup(struct rblist *metric_events,
					 struct evsel *evsel,
					 bool create);
int metricgroup__parse_groups(struct evlist *perf_evlist,
			      const char *pmu,
			      const char *str,
			      bool metric_no_group,
			      bool metric_no_merge,
			      bool metric_no_threshold,
			      const char *user_requested_cpu_list,
			      bool system_wide,
			      struct rblist *metric_events);
int metricgroup__parse_groups_test(struct evlist *evlist,
				   const struct pmu_metrics_table *table,
				   const char *str,
				   struct rblist *metric_events);

void metricgroup__print(const struct print_callbacks *print_cb, void *print_state);
bool metricgroup__has_metric(const char *pmu, const char *metric);
unsigned int metricgroups__topdown_max_level(void);
int arch_get_runtimeparam(const struct pmu_metric *pm);
void metricgroup__rblist_exit(struct rblist *metric_events);

int metricgroup__copy_metric_events(struct evlist *evlist, struct cgroup *cgrp,
				    struct rblist *new_metric_events,
				    struct rblist *old_metric_events);
#endif
