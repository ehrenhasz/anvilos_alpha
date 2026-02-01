 
 

#ifndef RTL8225H
#define RTL8225H

#define RTL819X_TOTAL_RF_PATH 2  
void phy_set_rf8256_bandwidth(struct net_device *dev,
			      enum ht_channel_width bandwidth);
void phy_rf8256_config(struct net_device *dev);
void phy_set_rf8256_cck_tx_power(struct net_device *dev, u8 powerlevel);
void phy_set_rf8256_ofdm_tx_power(struct net_device *dev, u8 powerlevel);

#endif
