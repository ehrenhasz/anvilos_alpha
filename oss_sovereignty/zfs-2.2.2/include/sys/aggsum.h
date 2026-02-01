 
 

#ifndef	_SYS_AGGSUM_H
#define	_SYS_AGGSUM_H

#include <sys/zfs_context.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct aggsum_bucket aggsum_bucket_t;

struct aggsum_bucket {
	kmutex_t asc_lock;
	int64_t asc_delta;
	uint64_t asc_borrowed;
} ____cacheline_aligned;

 
typedef struct aggsum {
	kmutex_t as_lock;
	int64_t as_lower_bound;
	uint64_t as_upper_bound;
	aggsum_bucket_t *as_buckets ____cacheline_aligned;
	uint_t as_numbuckets;
	uint_t as_bucketshift;
} aggsum_t;

void aggsum_init(aggsum_t *, uint64_t);
void aggsum_fini(aggsum_t *);
int64_t aggsum_lower_bound(aggsum_t *);
uint64_t aggsum_upper_bound(aggsum_t *);
int aggsum_compare(aggsum_t *, uint64_t);
uint64_t aggsum_value(aggsum_t *);
void aggsum_add(aggsum_t *, int64_t);

#ifdef	__cplusplus
}
#endif

#endif  
