 
 

#ifndef KFD_EVENTS_H_INCLUDED
#define KFD_EVENTS_H_INCLUDED

#include <linux/kernel.h>
#include <linux/hashtable.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/wait.h>
#include "kfd_priv.h"
#include <uapi/linux/kfd_ioctl.h>

 
#define KFD_FIRST_NONSIGNAL_EVENT_ID ((INT_MAX >> 1) + 1)
#define KFD_LAST_NONSIGNAL_EVENT_ID INT_MAX

 
#define UNSIGNALED_EVENT_SLOT ((uint64_t)-1)

struct kfd_event_waiter;
struct signal_page;

struct kfd_event {
	u32 event_id;
	u64 event_age;

	bool signaled;
	bool auto_reset;

	int type;

	spinlock_t lock;
	wait_queue_head_t wq;  

	 
	uint64_t __user *user_signal_address;

	 
	union {
		struct kfd_hsa_memory_exception_data memory_exception_data;
		struct kfd_hsa_hw_exception_data hw_exception_data;
	};

	struct rcu_head rcu;  
};

#define KFD_EVENT_TIMEOUT_IMMEDIATE 0
#define KFD_EVENT_TIMEOUT_INFINITE 0xFFFFFFFFu

 
#define KFD_EVENT_TYPE_SIGNAL 0
#define KFD_EVENT_TYPE_HW_EXCEPTION 3
#define KFD_EVENT_TYPE_DEBUG 5
#define KFD_EVENT_TYPE_MEMORY 8

extern void kfd_signal_event_interrupt(u32 pasid, uint32_t partial_id,
				       uint32_t valid_id_bits);

#endif
