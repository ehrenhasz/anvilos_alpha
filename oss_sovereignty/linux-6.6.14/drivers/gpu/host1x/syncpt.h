 
 

#ifndef __HOST1X_SYNCPT_H
#define __HOST1X_SYNCPT_H

#include <linux/atomic.h>
#include <linux/host1x.h>
#include <linux/kernel.h>
#include <linux/kref.h>
#include <linux/sched.h>

#include "fence.h"
#include "intr.h"

struct host1x;

 
#define HOST1X_SYNCPT_RESERVED			0

struct host1x_syncpt_base {
	unsigned int id;
	bool requested;
};

struct host1x_syncpt {
	struct kref ref;

	unsigned int id;
	atomic_t min_val;
	atomic_t max_val;
	u32 base_val;
	const char *name;
	bool client_managed;
	struct host1x *host;
	struct host1x_syncpt_base *base;

	 
	struct host1x_fence_list fences;

	 
	bool locked;
};

 
int host1x_syncpt_init(struct host1x *host);

 
void host1x_syncpt_deinit(struct host1x *host);

 
unsigned int host1x_syncpt_nb_pts(struct host1x *host);

 
unsigned int host1x_syncpt_nb_bases(struct host1x *host);

 
unsigned int host1x_syncpt_nb_mlocks(struct host1x *host);

 
static inline bool host1x_syncpt_check_max(struct host1x_syncpt *sp, u32 real)
{
	u32 max;
	if (sp->client_managed)
		return true;
	max = host1x_syncpt_read_max(sp);
	return (s32)(max - real) >= 0;
}

 
static inline bool host1x_syncpt_client_managed(struct host1x_syncpt *sp)
{
	return sp->client_managed;
}

 
static inline bool host1x_syncpt_idle(struct host1x_syncpt *sp)
{
	int min, max;
	smp_rmb();
	min = atomic_read(&sp->min_val);
	max = atomic_read(&sp->max_val);
	return (min == max);
}

 
u32 host1x_syncpt_load(struct host1x_syncpt *sp);

 
bool host1x_syncpt_is_expired(struct host1x_syncpt *sp, u32 thresh);

 
void host1x_syncpt_save(struct host1x *host);

 
void host1x_syncpt_restore(struct host1x *host);

 
u32 host1x_syncpt_load_wait_base(struct host1x_syncpt *sp);

 
u32 host1x_syncpt_incr_max(struct host1x_syncpt *sp, u32 incrs);

 
static inline int host1x_syncpt_is_valid(struct host1x_syncpt *sp)
{
	return sp->id < host1x_syncpt_nb_pts(sp->host);
}

static inline void host1x_syncpt_set_locked(struct host1x_syncpt *sp)
{
	sp->locked = true;
}

#endif
