 
 

 

#ifndef R8180_WX_H
#define R8180_WX_H

extern const struct iw_handler_def r8192_wx_handlers_def;
 
struct iw_statistics *r8192_get_wireless_stats(struct net_device *dev);

#endif
