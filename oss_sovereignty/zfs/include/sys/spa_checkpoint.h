#ifndef _SYS_SPA_CHECKPOINT_H
#define	_SYS_SPA_CHECKPOINT_H
#include <sys/zthr.h>
typedef struct spa_checkpoint_info {
	uint64_t sci_timestamp;  
	uint64_t sci_dspace;     
} spa_checkpoint_info_t;
int spa_checkpoint(const char *);
int spa_checkpoint_discard(const char *);
boolean_t spa_checkpoint_discard_thread_check(void *, zthr_t *);
void spa_checkpoint_discard_thread(void *, zthr_t *);
int spa_checkpoint_get_stats(spa_t *, pool_checkpoint_stat_t *);
#endif  
