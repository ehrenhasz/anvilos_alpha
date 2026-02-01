 
 

#ifndef _NET_BATMAN_ADV_FRAGMENTATION_H_
#define _NET_BATMAN_ADV_FRAGMENTATION_H_

#include "main.h"

#include <linux/compiler.h>
#include <linux/list.h>
#include <linux/skbuff.h>
#include <linux/stddef.h>
#include <linux/types.h>

void batadv_frag_purge_orig(struct batadv_orig_node *orig,
			    bool (*check_cb)(struct batadv_frag_table_entry *));
bool batadv_frag_skb_fwd(struct sk_buff *skb,
			 struct batadv_hard_iface *recv_if,
			 struct batadv_orig_node *orig_node_src);
bool batadv_frag_skb_buffer(struct sk_buff **skb,
			    struct batadv_orig_node *orig_node);
int batadv_frag_send_packet(struct sk_buff *skb,
			    struct batadv_orig_node *orig_node,
			    struct batadv_neigh_node *neigh_node);

 
static inline bool
batadv_frag_check_entry(struct batadv_frag_table_entry *frags_entry)
{
	if (!hlist_empty(&frags_entry->fragment_list) &&
	    batadv_has_timed_out(frags_entry->timestamp, BATADV_FRAG_TIMEOUT))
		return true;
	return false;
}

#endif  
