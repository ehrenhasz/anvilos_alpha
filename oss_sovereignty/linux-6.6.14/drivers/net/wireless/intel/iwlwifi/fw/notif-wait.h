 
 
#ifndef __iwl_notif_wait_h__
#define __iwl_notif_wait_h__

#include <linux/wait.h>

#include "iwl-trans.h"

struct iwl_notif_wait_data {
	struct list_head notif_waits;
	spinlock_t notif_wait_lock;
	wait_queue_head_t notif_waitq;
};

#define MAX_NOTIF_CMDS	5

 
struct iwl_notification_wait {
	struct list_head list;

	bool (*fn)(struct iwl_notif_wait_data *notif_data,
		   struct iwl_rx_packet *pkt, void *data);
	void *fn_data;

	u16 cmds[MAX_NOTIF_CMDS];
	u8 n_cmds;
	bool triggered, aborted;
};


 
void iwl_notification_wait_init(struct iwl_notif_wait_data *notif_data);
bool iwl_notification_wait(struct iwl_notif_wait_data *notif_data,
			   struct iwl_rx_packet *pkt);
void iwl_abort_notification_waits(struct iwl_notif_wait_data *notif_data);

static inline void
iwl_notification_notify(struct iwl_notif_wait_data *notif_data)
{
	wake_up_all(&notif_data->notif_waitq);
}

static inline void
iwl_notification_wait_notify(struct iwl_notif_wait_data *notif_data,
			     struct iwl_rx_packet *pkt)
{
	if (iwl_notification_wait(notif_data, pkt))
		iwl_notification_notify(notif_data);
}

 
void __acquires(wait_entry)
iwl_init_notification_wait(struct iwl_notif_wait_data *notif_data,
			   struct iwl_notification_wait *wait_entry,
			   const u16 *cmds, int n_cmds,
			   bool (*fn)(struct iwl_notif_wait_data *notif_data,
				      struct iwl_rx_packet *pkt, void *data),
			   void *fn_data);

int __must_check __releases(wait_entry)
iwl_wait_notification(struct iwl_notif_wait_data *notif_data,
		      struct iwl_notification_wait *wait_entry,
		      unsigned long timeout);

void __releases(wait_entry)
iwl_remove_notification(struct iwl_notif_wait_data *notif_data,
			struct iwl_notification_wait *wait_entry);

#endif  
