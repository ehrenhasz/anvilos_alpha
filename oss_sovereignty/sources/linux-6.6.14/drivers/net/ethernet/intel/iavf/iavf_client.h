


#ifndef _IAVF_CLIENT_H_
#define _IAVF_CLIENT_H_

#define IAVF_CLIENT_STR_LENGTH 10


#define IAVF_CLIENT_VERSION_MAJOR 0
#define IAVF_CLIENT_VERSION_MINOR 01
#define IAVF_CLIENT_VERSION_BUILD 00
#define IAVF_CLIENT_VERSION_STR     \
	__stringify(IAVF_CLIENT_VERSION_MAJOR) "." \
	__stringify(IAVF_CLIENT_VERSION_MINOR) "." \
	__stringify(IAVF_CLIENT_VERSION_BUILD)

struct iavf_client_version {
	u8 major;
	u8 minor;
	u8 build;
	u8 rsvd;
};

enum iavf_client_state {
	__IAVF_CLIENT_NULL,
	__IAVF_CLIENT_REGISTERED
};

enum iavf_client_instance_state {
	__IAVF_CLIENT_INSTANCE_NONE,
	__IAVF_CLIENT_INSTANCE_OPENED,
};

struct iavf_ops;
struct iavf_client;


#define IAVF_QUEUE_TYPE_PE_AEQ	0x80
#define IAVF_QUEUE_INVALID_IDX	0xFFFF

struct iavf_qv_info {
	u32 v_idx; 
	u16 ceq_idx;
	u16 aeq_idx;
	u8 itr_idx;
};

struct iavf_qvlist_info {
	u32 num_vectors;
	struct iavf_qv_info qv_info[];
};

#define IAVF_CLIENT_MSIX_ALL 0xFFFFFFFF




struct iavf_prio_qos_params {
	u16 qs_handle; 
	u8 tc; 
	u8 reserved;
};

#define IAVF_CLIENT_MAX_USER_PRIORITY	8

struct iavf_qos_params {
	struct iavf_prio_qos_params prio_qos[IAVF_CLIENT_MAX_USER_PRIORITY];
};

struct iavf_params {
	struct iavf_qos_params qos;
	u16 mtu;
	u16 link_up; 
};


struct iavf_info {
	struct iavf_client_version version;
	u8 lanmac[6];
	struct net_device *netdev;
	struct pci_dev *pcidev;
	u8 __iomem *hw_addr;
	u8 fid;	
#define IAVF_CLIENT_FTYPE_PF 0
#define IAVF_CLIENT_FTYPE_VF 1
	u8 ftype; 
	void *vf; 

	
	struct iavf_params params;
	struct iavf_ops *ops;

	u16 msix_count;	 
	
	struct msix_entry *msix_entries;
	u16 itr_index; 
};

struct iavf_ops {
	
	int (*setup_qvlist)(struct iavf_info *ldev, struct iavf_client *client,
			    struct iavf_qvlist_info *qv_info);

	u32 (*virtchnl_send)(struct iavf_info *ldev, struct iavf_client *client,
			     u8 *msg, u16 len);

	
	void (*request_reset)(struct iavf_info *ldev,
			      struct iavf_client *client);
};

struct iavf_client_ops {
	
	int (*open)(struct iavf_info *ldev, struct iavf_client *client);

	
	void (*close)(struct iavf_info *ldev, struct iavf_client *client,
		      bool reset);

	
	void (*l2_param_change)(struct iavf_info *ldev,
				struct iavf_client *client,
				struct iavf_params *params);

	
	int (*virtchnl_receive)(struct iavf_info *ldev,
				struct iavf_client *client,
				u8 *msg, u16 len);
};


struct iavf_client_instance {
	struct list_head list;
	struct iavf_info lan_info;
	struct iavf_client *client;
	unsigned long  state;
};

struct iavf_client {
	struct list_head list;		
	char name[IAVF_CLIENT_STR_LENGTH];
	struct iavf_client_version version;
	unsigned long state;		
	atomic_t ref_cnt;  
	u32 flags;
#define IAVF_CLIENT_FLAGS_LAUNCH_ON_PROBE	BIT(0)
#define IAVF_TX_FLAGS_NOTIFY_OTHER_EVENTS	BIT(2)
	u8 type;
#define IAVF_CLIENT_RDMA 0
	struct iavf_client_ops *ops;	
};


int iavf_register_client(struct iavf_client *client);
int iavf_unregister_client(struct iavf_client *client);
#endif 
