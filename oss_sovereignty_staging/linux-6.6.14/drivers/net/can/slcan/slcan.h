 

#ifndef _SLCAN_H
#define _SLCAN_H

bool slcan_err_rst_on_open(struct net_device *ndev);
int slcan_enable_err_rst_on_open(struct net_device *ndev, bool on);

extern const struct ethtool_ops slcan_ethtool_ops;

#endif  
