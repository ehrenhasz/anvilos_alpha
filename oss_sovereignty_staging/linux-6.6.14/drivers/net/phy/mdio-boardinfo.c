
 

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/export.h>
#include <linux/mutex.h>
#include <linux/list.h>

#include "mdio-boardinfo.h"

static LIST_HEAD(mdio_board_list);
static DEFINE_MUTEX(mdio_board_lock);

 
void mdiobus_setup_mdiodev_from_board_info(struct mii_bus *bus,
					   int (*cb)
					   (struct mii_bus *bus,
					    struct mdio_board_info *bi))
{
	struct mdio_board_entry *be;
	struct mdio_board_entry *tmp;
	struct mdio_board_info *bi;
	int ret;

	mutex_lock(&mdio_board_lock);
	list_for_each_entry_safe(be, tmp, &mdio_board_list, list) {
		bi = &be->board_info;

		if (strcmp(bus->id, bi->bus_id))
			continue;

		mutex_unlock(&mdio_board_lock);
		ret = cb(bus, bi);
		mutex_lock(&mdio_board_lock);
		if (ret)
			continue;

	}
	mutex_unlock(&mdio_board_lock);
}
EXPORT_SYMBOL(mdiobus_setup_mdiodev_from_board_info);

 
int mdiobus_register_board_info(const struct mdio_board_info *info,
				unsigned int n)
{
	struct mdio_board_entry *be;
	unsigned int i;

	be = kcalloc(n, sizeof(*be), GFP_KERNEL);
	if (!be)
		return -ENOMEM;

	for (i = 0; i < n; i++, be++, info++) {
		memcpy(&be->board_info, info, sizeof(*info));
		mutex_lock(&mdio_board_lock);
		list_add_tail(&be->list, &mdio_board_list);
		mutex_unlock(&mdio_board_lock);
	}

	return 0;
}
EXPORT_SYMBOL(mdiobus_register_board_info);
