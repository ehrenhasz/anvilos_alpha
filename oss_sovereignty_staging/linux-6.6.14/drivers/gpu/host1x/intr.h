 
 

#ifndef __HOST1X_INTR_H
#define __HOST1X_INTR_H

struct host1x;
struct host1x_syncpt_fence;

 
int host1x_intr_init(struct host1x *host);

 
void host1x_intr_deinit(struct host1x *host);

 
void host1x_intr_start(struct host1x *host);

 
void host1x_intr_stop(struct host1x *host);

void host1x_intr_handle_interrupt(struct host1x *host, unsigned int id);

void host1x_intr_add_fence_locked(struct host1x *host, struct host1x_syncpt_fence *fence);

bool host1x_intr_remove_fence(struct host1x *host, struct host1x_syncpt_fence *fence);

#endif
