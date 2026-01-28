#ifndef __SDW_IRQ_H
#define __SDW_IRQ_H
#include <linux/soundwire/sdw.h>
#include <linux/fwnode.h>
#if IS_ENABLED(CONFIG_IRQ_DOMAIN)
int sdw_irq_create(struct sdw_bus *bus,
		   struct fwnode_handle *fwnode);
void sdw_irq_delete(struct sdw_bus *bus);
void sdw_irq_create_mapping(struct sdw_slave *slave);
void sdw_irq_dispose_mapping(struct sdw_slave *slave);
#else  
static inline int sdw_irq_create(struct sdw_bus *bus,
				 struct fwnode_handle *fwnode)
{
	return 0;
}
static inline void sdw_irq_delete(struct sdw_bus *bus)
{
}
static inline void sdw_irq_create_mapping(struct sdw_slave *slave)
{
}
static inline void sdw_irq_dispose_mapping(struct sdw_slave *slave)
{
}
#endif  
#endif  
