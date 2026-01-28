
#ifndef __IEEE802154_CORE_H
#define __IEEE802154_CORE_H

#include <net/cfg802154.h>

struct cfg802154_registered_device {
	const struct cfg802154_ops *ops;
	struct list_head list;

	
	int wpan_phy_idx;

	
	int opencount;
	wait_queue_head_t dev_wait;

	
	int num_running_ifaces;

	
	struct list_head wpan_dev_list;
	int devlist_generation, wpan_dev_id;

	
	struct wpan_phy wpan_phy __aligned(NETDEV_ALIGN);
};

static inline struct cfg802154_registered_device *
wpan_phy_to_rdev(struct wpan_phy *wpan_phy)
{
	BUG_ON(!wpan_phy);
	return container_of(wpan_phy, struct cfg802154_registered_device,
			    wpan_phy);
}

extern struct list_head cfg802154_rdev_list;
extern int cfg802154_rdev_list_generation;

int cfg802154_switch_netns(struct cfg802154_registered_device *rdev,
			   struct net *net);

void cfg802154_dev_free(struct cfg802154_registered_device *rdev);
struct cfg802154_registered_device *
cfg802154_rdev_by_wpan_phy_idx(int wpan_phy_idx);
struct wpan_phy *wpan_phy_idx_to_wpan_phy(int wpan_phy_idx);

#endif 
