


#ifndef __IBM_NEWEMAC_PHY_H
#define __IBM_NEWEMAC_PHY_H

struct mii_phy;


struct mii_phy_ops {
	int (*init) (struct mii_phy * phy);
	int (*suspend) (struct mii_phy * phy, int wol_options);
	int (*setup_aneg) (struct mii_phy * phy, u32 advertise);
	int (*setup_forced) (struct mii_phy * phy, int speed, int fd);
	int (*poll_link) (struct mii_phy * phy);
	int (*read_link) (struct mii_phy * phy);
};


struct mii_phy_def {
	u32 phy_id;		
	u32 phy_id_mask;	
	u32 features;		
	int magic_aneg;		
	const char *name;
	const struct mii_phy_ops *ops;
};


struct mii_phy {
	struct mii_phy_def *def;
	u32 advertising;	
	u32 features;		
	int address;		
	int mode;		
	int gpcs_address;	

	
	int autoneg;

	
	int speed;
	int duplex;
	int pause;
	int asym_pause;

	
	struct net_device *dev;
	int (*mdio_read) (struct net_device * dev, int addr, int reg);
	void (*mdio_write) (struct net_device * dev, int addr, int reg,
			    int val);
};


int emac_mii_phy_probe(struct mii_phy *phy, int address);
int emac_mii_reset_phy(struct mii_phy *phy);
int emac_mii_reset_gpcs(struct mii_phy *phy);

#endif 
