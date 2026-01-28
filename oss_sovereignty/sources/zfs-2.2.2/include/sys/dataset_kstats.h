



#ifndef _SYS_DATASET_KSTATS_H
#define	_SYS_DATASET_KSTATS_H

#include <sys/wmsum.h>
#include <sys/dmu.h>
#include <sys/kstat.h>
#include <sys/zil.h>

typedef struct dataset_sum_stats_t {
	wmsum_t dss_writes;
	wmsum_t dss_nwritten;
	wmsum_t dss_reads;
	wmsum_t dss_nread;
	wmsum_t dss_nunlinks;
	wmsum_t dss_nunlinked;
} dataset_sum_stats_t;

typedef struct dataset_kstat_values {
	kstat_named_t dkv_ds_name;
	kstat_named_t dkv_writes;
	kstat_named_t dkv_nwritten;
	kstat_named_t dkv_reads;
	kstat_named_t dkv_nread;
	
	kstat_named_t dkv_nunlinks;
	
	kstat_named_t dkv_nunlinked;
	
	zil_kstat_values_t dkv_zil_stats;
} dataset_kstat_values_t;

typedef struct dataset_kstats {
	dataset_sum_stats_t dk_sums;
	zil_sums_t dk_zil_sums;
	kstat_t *dk_kstats;
} dataset_kstats_t;

int dataset_kstats_create(dataset_kstats_t *, objset_t *);
void dataset_kstats_destroy(dataset_kstats_t *);

void dataset_kstats_update_write_kstats(dataset_kstats_t *, int64_t);
void dataset_kstats_update_read_kstats(dataset_kstats_t *, int64_t);

void dataset_kstats_update_nunlinks_kstat(dataset_kstats_t *, int64_t);
void dataset_kstats_update_nunlinked_kstat(dataset_kstats_t *, int64_t);

#endif 
