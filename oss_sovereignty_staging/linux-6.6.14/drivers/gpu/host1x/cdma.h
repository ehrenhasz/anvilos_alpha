 
 

#ifndef __HOST1X_CDMA_H
#define __HOST1X_CDMA_H

#include <linux/sched.h>
#include <linux/completion.h>
#include <linux/list.h>
#include <linux/workqueue.h>

struct host1x_syncpt;
struct host1x_userctx_timeout;
struct host1x_job;

 

struct push_buffer {
	void *mapped;			 
	dma_addr_t dma;			 
	dma_addr_t phys;		 
	u32 fence;			 
	u32 pos;			 
	u32 size;
	u32 alloc_size;
};

struct buffer_timeout {
	struct delayed_work wq;		 
	bool initialized;		 
	struct host1x_syncpt *syncpt;	 
	u32 syncpt_val;			 
	ktime_t start_ktime;		 
	 
	struct host1x_client *client;
};

enum cdma_event {
	CDMA_EVENT_NONE,		 
	CDMA_EVENT_SYNC_QUEUE_EMPTY,	 
	CDMA_EVENT_PUSH_BUFFER_SPACE	 
};

struct host1x_cdma {
	struct mutex lock;		 
	struct completion complete;	 
	enum cdma_event event;		 
	unsigned int slots_used;	 
	unsigned int slots_free;	 
	unsigned int first_get;		 
	unsigned int last_pos;		 
	struct push_buffer push_buffer;	 
	struct list_head sync_queue;	 
	struct buffer_timeout timeout;	 
	bool running;
	bool torndown;
	struct work_struct update_work;
};

#define cdma_to_channel(cdma) container_of(cdma, struct host1x_channel, cdma)
#define cdma_to_host1x(cdma) dev_get_drvdata(cdma_to_channel(cdma)->dev->parent)
#define pb_to_cdma(pb) container_of(pb, struct host1x_cdma, push_buffer)

int host1x_cdma_init(struct host1x_cdma *cdma);
int host1x_cdma_deinit(struct host1x_cdma *cdma);
int host1x_cdma_begin(struct host1x_cdma *cdma, struct host1x_job *job);
void host1x_cdma_push(struct host1x_cdma *cdma, u32 op1, u32 op2);
void host1x_cdma_push_wide(struct host1x_cdma *cdma, u32 op1, u32 op2,
			   u32 op3, u32 op4);
void host1x_cdma_end(struct host1x_cdma *cdma, struct host1x_job *job);
void host1x_cdma_update(struct host1x_cdma *cdma);
void host1x_cdma_peek(struct host1x_cdma *cdma, u32 dmaget, int slot,
		      u32 *out);
unsigned int host1x_cdma_wait_locked(struct host1x_cdma *cdma,
				     enum cdma_event event);
void host1x_cdma_update_sync_queue(struct host1x_cdma *cdma,
				   struct device *dev);
#endif
