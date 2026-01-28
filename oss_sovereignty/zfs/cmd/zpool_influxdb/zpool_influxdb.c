#include <string.h>
#include <getopt.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <libzfs.h>
#define	POOL_MEASUREMENT	"zpool_stats"
#define	SCAN_MEASUREMENT	"zpool_scan_stats"
#define	VDEV_MEASUREMENT	"zpool_vdev_stats"
#define	POOL_LATENCY_MEASUREMENT	"zpool_latency"
#define	POOL_QUEUE_MEASUREMENT	"zpool_vdev_queue"
#define	MIN_LAT_INDEX	10   
#define	POOL_IO_SIZE_MEASUREMENT	"zpool_io_size"
#define	MIN_SIZE_INDEX	9   
int execd_mode = 0;
int no_histograms = 0;
int sum_histogram_buckets = 0;
char metric_data_type = 'u';
uint64_t metric_value_mask = UINT64_MAX;
uint64_t timestamp = 0;
int complained_about_sync = 0;
const char *tags = "";
typedef int (*stat_printer_f)(nvlist_t *, const char *, const char *);
static char *
escape_string(const char *s)
{
	const char *c;
	char *d;
	char *t = (char *)malloc(ZFS_MAX_DATASET_NAME_LEN * 2);
	if (t == NULL) {
		fprintf(stderr, "error: cannot allocate memory\n");
		exit(1);
	}
	for (c = s, d = t; *c != '\0'; c++, d++) {
		switch (*c) {
		case ' ':
		case ',':
		case '=':
		case '\\':
			*d++ = '\\';
			zfs_fallthrough;
		default:
			*d = *c;
		}
	}
	*d = '\0';
	return (t);
}
static void
print_kv(const char *key, uint64_t value)
{
	printf("%s=%llu%c", key,
	    (u_longlong_t)value & metric_value_mask, metric_data_type);
}
static int
print_scan_status(nvlist_t *nvroot, const char *pool_name)
{
	uint_t c;
	int64_t elapsed;
	uint64_t examined, pass_exam, paused_time, paused_ts, rate;
	uint64_t remaining_time;
	pool_scan_stat_t *ps = NULL;
	double pct_done;
	const char *const state[DSS_NUM_STATES] = {
	    "none", "scanning", "finished", "canceled"};
	const char *func;
	(void) nvlist_lookup_uint64_array(nvroot,
	    ZPOOL_CONFIG_SCAN_STATS,
	    (uint64_t **)&ps, &c);
	if (ps == NULL)
		return (0);
	if (ps->pss_state >= DSS_NUM_STATES ||
	    ps->pss_func >= POOL_SCAN_FUNCS) {
		if (complained_about_sync % 1000 == 0) {
			fprintf(stderr, "error: cannot decode scan stats: "
			    "ZFS is out of sync with compiled zpool_influxdb");
			complained_about_sync++;
		}
		return (1);
	}
	switch (ps->pss_func) {
	case POOL_SCAN_NONE:
		func = "none_requested";
		break;
	case POOL_SCAN_SCRUB:
		func = "scrub";
		break;
	case POOL_SCAN_RESILVER:
		func = "resilver";
		break;
#ifdef POOL_SCAN_REBUILD
	case POOL_SCAN_REBUILD:
		func = "rebuild";
		break;
#endif
	default:
		func = "scan";
	}
	examined = ps->pss_examined ? ps->pss_examined : 1;
	pct_done = 0.0;
	if (ps->pss_to_examine > 0)
		pct_done = 100.0 * examined / ps->pss_to_examine;
#ifdef EZFS_SCRUB_PAUSED
	paused_ts = ps->pss_pass_scrub_pause;
	paused_time = ps->pss_pass_scrub_spent_paused;
#else
	paused_ts = 0;
	paused_time = 0;
#endif
	if (ps->pss_state == DSS_SCANNING) {
		elapsed = (int64_t)time(NULL) - (int64_t)ps->pss_pass_start -
		    (int64_t)paused_time;
		elapsed = (elapsed > 0) ? elapsed : 1;
		pass_exam = ps->pss_pass_exam ? ps->pss_pass_exam : 1;
		rate = pass_exam / elapsed;
		rate = (rate > 0) ? rate : 1;
		remaining_time = ps->pss_to_examine - examined / rate;
	} else {
		elapsed =
		    (int64_t)ps->pss_end_time - (int64_t)ps->pss_pass_start -
		    (int64_t)paused_time;
		elapsed = (elapsed > 0) ? elapsed : 1;
		pass_exam = ps->pss_pass_exam ? ps->pss_pass_exam : 1;
		rate = pass_exam / elapsed;
		remaining_time = 0;
	}
	rate = rate ? rate : 1;
	printf("%s%s,function=%s,name=%s,state=%s ",
	    SCAN_MEASUREMENT, tags, func, pool_name, state[ps->pss_state]);
	print_kv("end_ts", ps->pss_end_time);
	print_kv(",errors", ps->pss_errors);
	print_kv(",examined", examined);
	print_kv(",skipped", ps->pss_skipped);
	print_kv(",issued", ps->pss_issued);
	print_kv(",pass_examined", pass_exam);
	print_kv(",pass_issued", ps->pss_pass_issued);
	print_kv(",paused_ts", paused_ts);
	print_kv(",paused_t", paused_time);
	printf(",pct_done=%.2f", pct_done);
	print_kv(",processed", ps->pss_processed);
	print_kv(",rate", rate);
	print_kv(",remaining_t", remaining_time);
	print_kv(",start_ts", ps->pss_start_time);
	print_kv(",to_examine", ps->pss_to_examine);
	printf(" %llu\n", (u_longlong_t)timestamp);
	return (0);
}
static char *
get_vdev_name(nvlist_t *nvroot, const char *parent_name)
{
	static char vdev_name[256];
	uint64_t vdev_id = 0;
	const char *vdev_type = "unknown";
	(void) nvlist_lookup_string(nvroot, ZPOOL_CONFIG_TYPE, &vdev_type);
	if (nvlist_lookup_uint64(
	    nvroot, ZPOOL_CONFIG_ID, &vdev_id) != 0)
		vdev_id = UINT64_MAX;
	if (parent_name == NULL) {
		(void) snprintf(vdev_name, sizeof (vdev_name), "%s",
		    vdev_type);
	} else {
		(void) snprintf(vdev_name, sizeof (vdev_name),
		    "%.220s/%s-%llu",
		    parent_name, vdev_type, (u_longlong_t)vdev_id);
	}
	return (vdev_name);
}
static char *
get_vdev_desc(nvlist_t *nvroot, const char *parent_name)
{
	static char vdev_desc[2 * MAXPATHLEN];
	char vdev_value[MAXPATHLEN];
	char *s, *t;
	const char *vdev_type = "unknown";
	uint64_t vdev_id = UINT64_MAX;
	const char *vdev_path = NULL;
	(void) nvlist_lookup_string(nvroot, ZPOOL_CONFIG_TYPE, &vdev_type);
	(void) nvlist_lookup_uint64(nvroot, ZPOOL_CONFIG_ID, &vdev_id);
	(void) nvlist_lookup_string(nvroot, ZPOOL_CONFIG_PATH, &vdev_path);
	if (parent_name == NULL) {
		s = escape_string(vdev_type);
		(void) snprintf(vdev_value, sizeof (vdev_value), "vdev=%s", s);
		free(s);
	} else {
		s = escape_string((char *)parent_name);
		t = escape_string(vdev_type);
		(void) snprintf(vdev_value, sizeof (vdev_value),
		    "vdev=%s/%s-%llu", s, t, (u_longlong_t)vdev_id);
		free(s);
		free(t);
	}
	if (vdev_path == NULL) {
		(void) snprintf(vdev_desc, sizeof (vdev_desc), "%s",
		    vdev_value);
	} else {
		s = escape_string(vdev_path);
		(void) snprintf(vdev_desc, sizeof (vdev_desc), "path=%s,%s",
		    s, vdev_value);
		free(s);
	}
	return (vdev_desc);
}
static int
print_summary_stats(nvlist_t *nvroot, const char *pool_name,
    const char *parent_name)
{
	uint_t c;
	vdev_stat_t *vs;
	char *vdev_desc = NULL;
	vdev_desc = get_vdev_desc(nvroot, parent_name);
	if (nvlist_lookup_uint64_array(nvroot, ZPOOL_CONFIG_VDEV_STATS,
	    (uint64_t **)&vs, &c) != 0) {
		return (1);
	}
	printf("%s%s,name=%s,state=%s,%s ", POOL_MEASUREMENT, tags,
	    pool_name, zpool_state_to_name((vdev_state_t)vs->vs_state,
	    (vdev_aux_t)vs->vs_aux), vdev_desc);
	print_kv("alloc", vs->vs_alloc);
	print_kv(",free", vs->vs_space - vs->vs_alloc);
	print_kv(",size", vs->vs_space);
	print_kv(",read_bytes", vs->vs_bytes[ZIO_TYPE_READ]);
	print_kv(",read_errors", vs->vs_read_errors);
	print_kv(",read_ops", vs->vs_ops[ZIO_TYPE_READ]);
	print_kv(",write_bytes", vs->vs_bytes[ZIO_TYPE_WRITE]);
	print_kv(",write_errors", vs->vs_write_errors);
	print_kv(",write_ops", vs->vs_ops[ZIO_TYPE_WRITE]);
	print_kv(",checksum_errors", vs->vs_checksum_errors);
	print_kv(",fragmentation", vs->vs_fragmentation);
	printf(" %llu\n", (u_longlong_t)timestamp);
	return (0);
}
static int
print_vdev_latency_stats(nvlist_t *nvroot, const char *pool_name,
    const char *parent_name)
{
	uint_t c, end = 0;
	nvlist_t *nv_ex;
	char *vdev_desc = NULL;
	struct lat_lookup {
	    const char *name;
	    const char *short_name;
	    uint64_t sum;
	    uint64_t *array;
	};
	struct lat_lookup lat_type[] = {
	    {ZPOOL_CONFIG_VDEV_TOT_R_LAT_HISTO,   "total_read", 0},
	    {ZPOOL_CONFIG_VDEV_TOT_W_LAT_HISTO,   "total_write", 0},
	    {ZPOOL_CONFIG_VDEV_DISK_R_LAT_HISTO,  "disk_read", 0},
	    {ZPOOL_CONFIG_VDEV_DISK_W_LAT_HISTO,  "disk_write", 0},
	    {ZPOOL_CONFIG_VDEV_SYNC_R_LAT_HISTO,  "sync_read", 0},
	    {ZPOOL_CONFIG_VDEV_SYNC_W_LAT_HISTO,  "sync_write", 0},
	    {ZPOOL_CONFIG_VDEV_ASYNC_R_LAT_HISTO, "async_read", 0},
	    {ZPOOL_CONFIG_VDEV_ASYNC_W_LAT_HISTO, "async_write", 0},
	    {ZPOOL_CONFIG_VDEV_SCRUB_LAT_HISTO,   "scrub", 0},
#ifdef ZPOOL_CONFIG_VDEV_TRIM_LAT_HISTO
	    {ZPOOL_CONFIG_VDEV_TRIM_LAT_HISTO,    "trim", 0},
#endif
	    {ZPOOL_CONFIG_VDEV_REBUILD_LAT_HISTO,    "rebuild", 0},
	    {NULL,	NULL}
	};
	if (nvlist_lookup_nvlist(nvroot,
	    ZPOOL_CONFIG_VDEV_STATS_EX, &nv_ex) != 0) {
		return (6);
	}
	vdev_desc = get_vdev_desc(nvroot, parent_name);
	for (int i = 0; lat_type[i].name; i++) {
		if (nvlist_lookup_uint64_array(nv_ex,
		    lat_type[i].name, &lat_type[i].array, &c) != 0) {
			fprintf(stderr, "error: can't get %s\n",
			    lat_type[i].name);
			return (3);
		}
		end = c - 1;
	}
	for (int bucket = 0; bucket <= end; bucket++) {
		if (bucket < MIN_LAT_INDEX) {
			for (int i = 0; lat_type[i].name; i++) {
				lat_type[i].sum += lat_type[i].array[bucket];
			}
			continue;
		}
		if (bucket < end) {
			printf("%s%s,le=%0.6f,name=%s,%s ",
			    POOL_LATENCY_MEASUREMENT, tags,
			    (float)(1ULL << bucket) * 1e-9,
			    pool_name, vdev_desc);
		} else {
			printf("%s%s,le=+Inf,name=%s,%s ",
			    POOL_LATENCY_MEASUREMENT, tags, pool_name,
			    vdev_desc);
		}
		for (int i = 0; lat_type[i].name; i++) {
			if (bucket <= MIN_LAT_INDEX || sum_histogram_buckets) {
				lat_type[i].sum += lat_type[i].array[bucket];
			} else {
				lat_type[i].sum = lat_type[i].array[bucket];
			}
			print_kv(lat_type[i].short_name, lat_type[i].sum);
			if (lat_type[i + 1].name != NULL) {
				printf(",");
			}
		}
		printf(" %llu\n", (u_longlong_t)timestamp);
	}
	return (0);
}
static int
print_vdev_size_stats(nvlist_t *nvroot, const char *pool_name,
    const char *parent_name)
{
	uint_t c, end = 0;
	nvlist_t *nv_ex;
	char *vdev_desc = NULL;
	struct size_lookup {
	    const char *name;
	    const char *short_name;
	    uint64_t sum;
	    uint64_t *array;
	};
	struct size_lookup size_type[] = {
	    {ZPOOL_CONFIG_VDEV_SYNC_IND_R_HISTO,   "sync_read_ind"},
	    {ZPOOL_CONFIG_VDEV_SYNC_IND_W_HISTO,   "sync_write_ind"},
	    {ZPOOL_CONFIG_VDEV_ASYNC_IND_R_HISTO,  "async_read_ind"},
	    {ZPOOL_CONFIG_VDEV_ASYNC_IND_W_HISTO,  "async_write_ind"},
	    {ZPOOL_CONFIG_VDEV_IND_SCRUB_HISTO,    "scrub_read_ind"},
	    {ZPOOL_CONFIG_VDEV_SYNC_AGG_R_HISTO,   "sync_read_agg"},
	    {ZPOOL_CONFIG_VDEV_SYNC_AGG_W_HISTO,   "sync_write_agg"},
	    {ZPOOL_CONFIG_VDEV_ASYNC_AGG_R_HISTO,  "async_read_agg"},
	    {ZPOOL_CONFIG_VDEV_ASYNC_AGG_W_HISTO,  "async_write_agg"},
	    {ZPOOL_CONFIG_VDEV_AGG_SCRUB_HISTO,    "scrub_read_agg"},
#ifdef ZPOOL_CONFIG_VDEV_IND_TRIM_HISTO
	    {ZPOOL_CONFIG_VDEV_IND_TRIM_HISTO,    "trim_write_ind"},
	    {ZPOOL_CONFIG_VDEV_AGG_TRIM_HISTO,    "trim_write_agg"},
#endif
	    {ZPOOL_CONFIG_VDEV_IND_REBUILD_HISTO,    "rebuild_write_ind"},
	    {ZPOOL_CONFIG_VDEV_AGG_REBUILD_HISTO,    "rebuild_write_agg"},
	    {NULL,	NULL}
	};
	if (nvlist_lookup_nvlist(nvroot,
	    ZPOOL_CONFIG_VDEV_STATS_EX, &nv_ex) != 0) {
		return (6);
	}
	vdev_desc = get_vdev_desc(nvroot, parent_name);
	for (int i = 0; size_type[i].name; i++) {
		if (nvlist_lookup_uint64_array(nv_ex, size_type[i].name,
		    &size_type[i].array, &c) != 0) {
			fprintf(stderr, "error: can't get %s\n",
			    size_type[i].name);
			return (3);
		}
		end = c - 1;
	}
	for (int bucket = 0; bucket <= end; bucket++) {
		if (bucket < MIN_SIZE_INDEX) {
			for (int i = 0; size_type[i].name; i++) {
				size_type[i].sum += size_type[i].array[bucket];
			}
			continue;
		}
		if (bucket < end) {
			printf("%s%s,le=%llu,name=%s,%s ",
			    POOL_IO_SIZE_MEASUREMENT, tags, 1ULL << bucket,
			    pool_name, vdev_desc);
		} else {
			printf("%s%s,le=+Inf,name=%s,%s ",
			    POOL_IO_SIZE_MEASUREMENT, tags, pool_name,
			    vdev_desc);
		}
		for (int i = 0; size_type[i].name; i++) {
			if (bucket <= MIN_SIZE_INDEX || sum_histogram_buckets) {
				size_type[i].sum += size_type[i].array[bucket];
			} else {
				size_type[i].sum = size_type[i].array[bucket];
			}
			print_kv(size_type[i].short_name, size_type[i].sum);
			if (size_type[i + 1].name != NULL) {
				printf(",");
			}
		}
		printf(" %llu\n", (u_longlong_t)timestamp);
	}
	return (0);
}
static int
print_queue_stats(nvlist_t *nvroot, const char *pool_name,
    const char *parent_name)
{
	nvlist_t *nv_ex;
	uint64_t value;
	struct queue_lookup {
	    const char *name;
	    const char *short_name;
	};
	struct queue_lookup queue_type[] = {
	    {ZPOOL_CONFIG_VDEV_SYNC_R_ACTIVE_QUEUE,	"sync_r_active"},
	    {ZPOOL_CONFIG_VDEV_SYNC_W_ACTIVE_QUEUE,	"sync_w_active"},
	    {ZPOOL_CONFIG_VDEV_ASYNC_R_ACTIVE_QUEUE,	"async_r_active"},
	    {ZPOOL_CONFIG_VDEV_ASYNC_W_ACTIVE_QUEUE,	"async_w_active"},
	    {ZPOOL_CONFIG_VDEV_SCRUB_ACTIVE_QUEUE,	"async_scrub_active"},
	    {ZPOOL_CONFIG_VDEV_REBUILD_ACTIVE_QUEUE,	"rebuild_active"},
	    {ZPOOL_CONFIG_VDEV_SYNC_R_PEND_QUEUE,	"sync_r_pend"},
	    {ZPOOL_CONFIG_VDEV_SYNC_W_PEND_QUEUE,	"sync_w_pend"},
	    {ZPOOL_CONFIG_VDEV_ASYNC_R_PEND_QUEUE,	"async_r_pend"},
	    {ZPOOL_CONFIG_VDEV_ASYNC_W_PEND_QUEUE,	"async_w_pend"},
	    {ZPOOL_CONFIG_VDEV_SCRUB_PEND_QUEUE,	"async_scrub_pend"},
	    {ZPOOL_CONFIG_VDEV_REBUILD_PEND_QUEUE,	"rebuild_pend"},
	    {NULL,	NULL}
	};
	if (nvlist_lookup_nvlist(nvroot,
	    ZPOOL_CONFIG_VDEV_STATS_EX, &nv_ex) != 0) {
		return (6);
	}
	printf("%s%s,name=%s,%s ", POOL_QUEUE_MEASUREMENT, tags, pool_name,
	    get_vdev_desc(nvroot, parent_name));
	for (int i = 0; queue_type[i].name; i++) {
		if (nvlist_lookup_uint64(nv_ex,
		    queue_type[i].name, &value) != 0) {
			fprintf(stderr, "error: can't get %s\n",
			    queue_type[i].name);
			return (3);
		}
		print_kv(queue_type[i].short_name, value);
		if (queue_type[i + 1].name != NULL) {
			printf(",");
		}
	}
	printf(" %llu\n", (u_longlong_t)timestamp);
	return (0);
}
static int
print_top_level_vdev_stats(nvlist_t *nvroot, const char *pool_name)
{
	nvlist_t *nv_ex;
	uint64_t value;
	struct queue_lookup {
	    const char *name;
	    const char *short_name;
	};
	struct queue_lookup queue_type[] = {
	    {ZPOOL_CONFIG_VDEV_SYNC_R_ACTIVE_QUEUE, "sync_r_active_queue"},
	    {ZPOOL_CONFIG_VDEV_SYNC_W_ACTIVE_QUEUE, "sync_w_active_queue"},
	    {ZPOOL_CONFIG_VDEV_ASYNC_R_ACTIVE_QUEUE, "async_r_active_queue"},
	    {ZPOOL_CONFIG_VDEV_ASYNC_W_ACTIVE_QUEUE, "async_w_active_queue"},
	    {ZPOOL_CONFIG_VDEV_SCRUB_ACTIVE_QUEUE, "async_scrub_active_queue"},
	    {ZPOOL_CONFIG_VDEV_REBUILD_ACTIVE_QUEUE, "rebuild_active_queue"},
	    {ZPOOL_CONFIG_VDEV_SYNC_R_PEND_QUEUE, "sync_r_pend_queue"},
	    {ZPOOL_CONFIG_VDEV_SYNC_W_PEND_QUEUE, "sync_w_pend_queue"},
	    {ZPOOL_CONFIG_VDEV_ASYNC_R_PEND_QUEUE, "async_r_pend_queue"},
	    {ZPOOL_CONFIG_VDEV_ASYNC_W_PEND_QUEUE, "async_w_pend_queue"},
	    {ZPOOL_CONFIG_VDEV_SCRUB_PEND_QUEUE, "async_scrub_pend_queue"},
	    {ZPOOL_CONFIG_VDEV_REBUILD_PEND_QUEUE, "rebuild_pend_queue"},
	    {NULL, NULL}
	};
	if (nvlist_lookup_nvlist(nvroot,
	    ZPOOL_CONFIG_VDEV_STATS_EX, &nv_ex) != 0) {
		return (6);
	}
	printf("%s%s,name=%s,vdev=root ", VDEV_MEASUREMENT, tags,
	    pool_name);
	for (int i = 0; queue_type[i].name; i++) {
		if (nvlist_lookup_uint64(nv_ex,
		    queue_type[i].name, &value) != 0) {
			fprintf(stderr, "error: can't get %s\n",
			    queue_type[i].name);
			return (3);
		}
		if (i > 0)
			printf(",");
		print_kv(queue_type[i].short_name, value);
	}
	printf(" %llu\n", (u_longlong_t)timestamp);
	return (0);
}
static int
print_recursive_stats(stat_printer_f func, nvlist_t *nvroot,
    const char *pool_name, const char *parent_name, int descend)
{
	uint_t c, children;
	nvlist_t **child;
	char vdev_name[256];
	int err;
	err = func(nvroot, pool_name, parent_name);
	if (err)
		return (err);
	if (descend && nvlist_lookup_nvlist_array(nvroot, ZPOOL_CONFIG_CHILDREN,
	    &child, &children) == 0) {
		(void) strlcpy(vdev_name, get_vdev_name(nvroot, parent_name),
		    sizeof (vdev_name));
		for (c = 0; c < children; c++) {
			err = print_recursive_stats(func, child[c], pool_name,
			    vdev_name, descend);
			if (err)
				return (err);
		}
	}
	return (0);
}
static int
print_stats(zpool_handle_t *zhp, void *data)
{
	uint_t c;
	int err;
	boolean_t missing;
	nvlist_t *config, *nvroot;
	vdev_stat_t *vs;
	struct timespec tv;
	char *pool_name;
	if (data &&
	    strncmp(data, zpool_get_name(zhp), ZFS_MAX_DATASET_NAME_LEN) != 0) {
		zpool_close(zhp);
		return (0);
	}
	if (zpool_refresh_stats(zhp, &missing) != 0) {
		zpool_close(zhp);
		return (1);
	}
	config = zpool_get_config(zhp, NULL);
	if (clock_gettime(CLOCK_REALTIME, &tv) != 0)
		timestamp = (uint64_t)time(NULL) * 1000000000;
	else
		timestamp =
		    ((uint64_t)tv.tv_sec * 1000000000) + (uint64_t)tv.tv_nsec;
	if (nvlist_lookup_nvlist(
	    config, ZPOOL_CONFIG_VDEV_TREE, &nvroot) != 0) {
	zpool_close(zhp);
		return (2);
	}
	if (nvlist_lookup_uint64_array(nvroot, ZPOOL_CONFIG_VDEV_STATS,
	    (uint64_t **)&vs, &c) != 0) {
	zpool_close(zhp);
		return (3);
	}
	pool_name = escape_string(zpool_get_name(zhp));
	err = print_recursive_stats(print_summary_stats, nvroot,
	    pool_name, NULL, 1);
	if (err == 0)
	err = print_top_level_vdev_stats(nvroot, pool_name);
	if (no_histograms == 0) {
	if (err == 0)
		err = print_recursive_stats(print_vdev_latency_stats, nvroot,
		    pool_name, NULL, 1);
	if (err == 0)
		err = print_recursive_stats(print_vdev_size_stats, nvroot,
		    pool_name, NULL, 1);
	if (err == 0)
		err = print_recursive_stats(print_queue_stats, nvroot,
		    pool_name, NULL, 0);
	}
	if (err == 0)
		err = print_scan_status(nvroot, pool_name);
	free(pool_name);
	zpool_close(zhp);
	return (err);
}
static void
usage(char *name)
{
	fprintf(stderr, "usage: %s [--execd][--no-histograms]"
	    "[--sum-histogram-buckets] [--signed-int] [poolname]\n", name);
	exit(EXIT_FAILURE);
}
int
main(int argc, char *argv[])
{
	int opt;
	int ret = 8;
	char *line = NULL, *ttags = NULL;
	size_t len, tagslen = 0;
	struct option long_options[] = {
	    {"execd", no_argument, NULL, 'e'},
	    {"help", no_argument, NULL, 'h'},
	    {"no-histograms", no_argument, NULL, 'n'},
	    {"signed-int", no_argument, NULL, 'i'},
	    {"sum-histogram-buckets", no_argument, NULL, 's'},
	    {"tags", required_argument, NULL, 't'},
	    {0, 0, 0, 0}
	};
	while ((opt = getopt_long(
	    argc, argv, "ehinst:", long_options, NULL)) != -1) {
		switch (opt) {
		case 'e':
			execd_mode = 1;
			break;
		case 'i':
			metric_data_type = 'i';
			metric_value_mask = INT64_MAX;
			break;
		case 'n':
			no_histograms = 1;
			break;
		case 's':
			sum_histogram_buckets = 1;
			break;
		case 't':
			free(ttags);
			tagslen = strlen(optarg) + 2;
			ttags = calloc(1, tagslen);
			if (ttags == NULL) {
				fprintf(stderr,
				    "error: cannot allocate memory "
				    "for tags\n");
				exit(1);
			}
			(void) snprintf(ttags, tagslen, ",%s", optarg);
			tags = ttags;
			break;
		default:
			usage(argv[0]);
		}
	}
	libzfs_handle_t *g_zfs;
	if ((g_zfs = libzfs_init()) == NULL) {
		fprintf(stderr,
		    "error: cannot initialize libzfs. "
		    "Is the zfs module loaded or zrepl running?\n");
		exit(EXIT_FAILURE);
	}
	if (execd_mode == 0) {
		ret = zpool_iter(g_zfs, print_stats, argv[optind]);
		return (ret);
	}
	while (getline(&line, &len, stdin) != -1) {
		ret = zpool_iter(g_zfs, print_stats, argv[optind]);
		fflush(stdout);
	}
	return (ret);
}
