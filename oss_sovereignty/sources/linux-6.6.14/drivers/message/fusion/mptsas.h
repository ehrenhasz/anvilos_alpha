



#ifndef MPTSAS_H_INCLUDED
#define MPTSAS_H_INCLUDED


struct mptsas_target_reset_event {
	struct list_head 	list;
	EVENT_DATA_SAS_DEVICE_STATUS_CHANGE sas_event_data;
	u8	target_reset_issued;
	unsigned long	 time_count;
};

enum mptsas_hotplug_action {
	MPTSAS_ADD_DEVICE,
	MPTSAS_DEL_DEVICE,
	MPTSAS_ADD_RAID,
	MPTSAS_DEL_RAID,
	MPTSAS_ADD_PHYSDISK,
	MPTSAS_ADD_PHYSDISK_REPROBE,
	MPTSAS_DEL_PHYSDISK,
	MPTSAS_DEL_PHYSDISK_REPROBE,
	MPTSAS_ADD_INACTIVE_VOLUME,
	MPTSAS_IGNORE_EVENT,
};

struct mptsas_mapping{
	u8			id;
	u8			channel;
};

struct mptsas_device_info {
	struct list_head 	list;
	struct mptsas_mapping	os;	
	struct mptsas_mapping	fw;	
	u64			sas_address;
	u32			device_info; 
	u16			slot;		
	u64			enclosure_logical_id; 
	u8			is_logical_volume; 
	
	u8			is_hidden_raid_component;
	
	u8			volume_id;
	
	u8			is_cached;
};

struct mptsas_hotplug_event {
	MPT_ADAPTER		*ioc;
	enum mptsas_hotplug_action event_type;
	u64			sas_address;
	u8			channel;
	u8			id;
	u32			device_info;
	u16			handle;
	u8			phy_id;
	u8			phys_disk_num;		
	struct scsi_device	*sdev;
};

struct fw_event_work {
	struct list_head 	list;
	struct delayed_work	 work;
	int			users;
	MPT_ADAPTER	*ioc;
	u32			event;
	u8			retries;
	char			event_data[] __aligned(4);
};

struct mptsas_discovery_event {
	struct work_struct	work;
	MPT_ADAPTER		*ioc;
};



struct mptsas_devinfo {
	u16	handle;		
	u16	handle_parent;	
	u16	handle_enclosure; 
	u16	slot;		
	u8	phy_id;		
	u8	port_id;	
	u8	id;		
	u32	phys_disk_num;	
	u8	channel;	
	u64	sas_address;    
	u32	device_info;	
	u16	flags;		
};


struct mptsas_portinfo_details{
	u16	num_phys;	
	u64	phy_bitmask; 	
	struct sas_rphy *rphy;	
	struct sas_port *port;	
	struct scsi_target *starget;
	struct mptsas_portinfo *port_info;
};

struct mptsas_phyinfo {
	u16	handle;			
	u8	phy_id; 		
	u8	port_id; 		
	u8	negotiated_link_rate;	
	u8	hw_link_rate; 		
	u8	programmed_link_rate;	
	u8	sas_port_add_phy;	
	struct mptsas_devinfo identify;	
	struct mptsas_devinfo attached;	
	struct sas_phy *phy;		
	struct mptsas_portinfo *portinfo;
	struct mptsas_portinfo_details * port_details;
};

struct mptsas_portinfo {
	struct list_head list;
	u16		num_phys;	
	struct mptsas_phyinfo *phy_info;
};

struct mptsas_enclosure {
	u64	enclosure_logical_id;	
	u16	enclosure_handle;	
	u16	flags;			
	u16	num_slot;		
	u16	start_slot;		
	u8	start_id;		
	u8	start_channel;		
	u8	sep_id;			
	u8	sep_channel;		
};


#endif
