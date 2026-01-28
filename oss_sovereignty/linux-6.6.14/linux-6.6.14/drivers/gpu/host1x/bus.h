#ifndef HOST1X_BUS_H
#define HOST1X_BUS_H
struct bus_type;
struct host1x;
extern struct bus_type host1x_bus_type;
int host1x_register(struct host1x *host1x);
int host1x_unregister(struct host1x *host1x);
#endif
