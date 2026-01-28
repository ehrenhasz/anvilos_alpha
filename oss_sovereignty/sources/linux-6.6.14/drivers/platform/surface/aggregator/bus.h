


#ifndef _SURFACE_AGGREGATOR_BUS_H
#define _SURFACE_AGGREGATOR_BUS_H

#include <linux/surface_aggregator/controller.h>

#ifdef CONFIG_SURFACE_AGGREGATOR_BUS

int ssam_bus_register(void);
void ssam_bus_unregister(void);

#else 

static inline int ssam_bus_register(void) { return 0; }
static inline void ssam_bus_unregister(void) {}

#endif 
#endif 
