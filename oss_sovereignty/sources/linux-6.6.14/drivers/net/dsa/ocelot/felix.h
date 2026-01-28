

#ifndef _MSCC_FELIX_H
#define _MSCC_FELIX_H

#define ocelot_to_felix(o)		container_of((o), struct felix, ocelot)
#define FELIX_MAC_QUIRKS		OCELOT_QUIRK_PCS_PERFORMS_RATE_ADAPTATION

#define OCELOT_PORT_MODE_NONE		0
#define OCELOT_PORT_MODE_INTERNAL	BIT(0)
#define OCELOT_PORT_MODE_SGMII		BIT(1)
#define OCELOT_PORT_MODE_QSGMII		BIT(2)
#define OCELOT_PORT_MODE_2500BASEX	BIT(3)
#define OCELOT_PORT_MODE_USXGMII	BIT(4)
#define OCELOT_PORT_MODE_1000BASEX	BIT(5)

struct device_node;


struct felix_info {
	
	const struct resource		*resources;
	size_t				num_resources;
	
	const char *const		*resource_names;
	const struct reg_field		*regfields;
	const u32 *const		*map;
	const struct ocelot_ops		*ops;
	const u32			*port_modes;
	int				num_mact_rows;
	int				num_ports;
	int				num_tx_queues;
	struct vcap_props		*vcap;
	u16				vcap_pol_base;
	u16				vcap_pol_max;
	u16				vcap_pol_base2;
	u16				vcap_pol_max2;
	const struct ptp_clock_info	*ptp_caps;
	unsigned long			quirks;

	
	bool				quirk_no_xtr_irq;

	int	(*mdio_bus_alloc)(struct ocelot *ocelot);
	void	(*mdio_bus_free)(struct ocelot *ocelot);
	int	(*port_setup_tc)(struct dsa_switch *ds, int port,
				 enum tc_setup_type type, void *type_data);
	void	(*port_sched_speed_set)(struct ocelot *ocelot, int port,
					u32 speed);
	void	(*phylink_mac_config)(struct ocelot *ocelot, int port,
				      unsigned int mode,
				      const struct phylink_link_state *state);
	int	(*configure_serdes)(struct ocelot *ocelot, int port,
				    struct device_node *portnp);
};


struct felix_tag_proto_ops {
	int (*setup)(struct dsa_switch *ds);
	void (*teardown)(struct dsa_switch *ds);
	unsigned long (*get_host_fwd_mask)(struct dsa_switch *ds);
	int (*change_master)(struct dsa_switch *ds, int port,
			     struct net_device *master,
			     struct netlink_ext_ack *extack);
};

extern const struct dsa_switch_ops felix_switch_ops;


struct felix {
	struct dsa_switch		*ds;
	const struct felix_info		*info;
	struct ocelot			ocelot;
	struct mii_bus			*imdio;
	struct phylink_pcs		**pcs;
	resource_size_t			switch_base;
	enum dsa_tag_protocol		tag_proto;
	const struct felix_tag_proto_ops *tag_proto_ops;
	struct kthread_worker		*xmit_worker;
	unsigned long			host_flood_uc_mask;
	unsigned long			host_flood_mc_mask;
};

struct net_device *felix_port_to_netdev(struct ocelot *ocelot, int port);
int felix_netdev_to_port(struct net_device *dev);

#endif
