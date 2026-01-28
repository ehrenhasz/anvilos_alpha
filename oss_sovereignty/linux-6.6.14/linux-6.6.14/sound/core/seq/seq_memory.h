#ifndef __SND_SEQ_MEMORYMGR_H
#define __SND_SEQ_MEMORYMGR_H
#include <sound/seq_kernel.h>
#include <linux/poll.h>
struct snd_info_buffer;
union __snd_seq_event {
	struct snd_seq_event legacy;
#if IS_ENABLED(CONFIG_SND_SEQ_UMP)
	struct snd_seq_ump_event ump;
#endif
	struct {
		struct snd_seq_event event;
#if IS_ENABLED(CONFIG_SND_SEQ_UMP)
		u32 extra;
#endif
	} __packed raw;
};
struct snd_seq_event_cell {
	union {
		struct snd_seq_event event;
		union __snd_seq_event ump;
	};
	struct snd_seq_pool *pool;				 
	struct snd_seq_event_cell *next;	 
};
struct snd_seq_pool {
	struct snd_seq_event_cell *ptr;	 
	struct snd_seq_event_cell *free;	 
	int total_elements;	 
	atomic_t counter;	 
	int size;		 
	int room;		 
	int closing;
	int max_used;
	int event_alloc_nopool;
	int event_alloc_failures;
	int event_alloc_success;
	wait_queue_head_t output_sleep;
	spinlock_t lock;
};
void snd_seq_cell_free(struct snd_seq_event_cell *cell);
int snd_seq_event_dup(struct snd_seq_pool *pool, struct snd_seq_event *event,
		      struct snd_seq_event_cell **cellp, int nonblock,
		      struct file *file, struct mutex *mutexp);
static inline int snd_seq_unused_cells(struct snd_seq_pool *pool)
{
	return pool ? pool->total_elements - atomic_read(&pool->counter) : 0;
}
static inline int snd_seq_total_cells(struct snd_seq_pool *pool)
{
	return pool ? pool->total_elements : 0;
}
int snd_seq_pool_init(struct snd_seq_pool *pool);
void snd_seq_pool_mark_closing(struct snd_seq_pool *pool);
int snd_seq_pool_done(struct snd_seq_pool *pool);
struct snd_seq_pool *snd_seq_pool_new(int poolsize);
int snd_seq_pool_delete(struct snd_seq_pool **pool);
int snd_seq_pool_poll_wait(struct snd_seq_pool *pool, struct file *file, poll_table *wait);
void snd_seq_info_pool(struct snd_info_buffer *buffer,
		       struct snd_seq_pool *pool, char *space);
#endif
