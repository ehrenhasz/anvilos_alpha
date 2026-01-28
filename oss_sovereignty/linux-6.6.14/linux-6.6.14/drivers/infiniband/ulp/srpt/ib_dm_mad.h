#ifndef IB_DM_MAD_H
#define IB_DM_MAD_H
#include <linux/types.h>
#include <rdma/ib_mad.h>
enum {
	DM_MAD_STATUS_UNSUP_METHOD = 0x0008,
	DM_MAD_STATUS_UNSUP_METHOD_ATTR = 0x000c,
	DM_MAD_STATUS_INVALID_FIELD = 0x001c,
	DM_MAD_STATUS_NO_IOC = 0x0100,
	DM_ATTR_CLASS_PORT_INFO = 0x01,
	DM_ATTR_IOU_INFO = 0x10,
	DM_ATTR_IOC_PROFILE = 0x11,
	DM_ATTR_SVC_ENTRIES = 0x12
};
struct ib_dm_hdr {
	u8 reserved[28];
};
struct ib_dm_mad {
	struct ib_mad_hdr mad_hdr;
	struct ib_rmpp_hdr rmpp_hdr;
	struct ib_dm_hdr dm_hdr;
	u8 data[IB_MGMT_DEVICE_DATA];
};
struct ib_dm_iou_info {
	__be16 change_id;
	u8 max_controllers;
	u8 op_rom;
	u8 controller_list[128];
};
struct ib_dm_ioc_profile {
	__be64 guid;
	__be32 vendor_id;
	__be32 device_id;
	__be16 device_version;
	__be16 reserved1;
	__be32 subsys_vendor_id;
	__be32 subsys_device_id;
	__be16 io_class;
	__be16 io_subclass;
	__be16 protocol;
	__be16 protocol_version;
	__be16 service_conn;
	__be16 initiators_supported;
	__be16 send_queue_depth;
	u8 reserved2;
	u8 rdma_read_depth;
	__be32 send_size;
	__be32 rdma_size;
	u8 op_cap_mask;
	u8 svc_cap_mask;
	u8 num_svc_entries;
	u8 reserved3[9];
	u8 id_string[64];
};
struct ib_dm_svc_entry {
	u8 name[40];
	__be64 id;
};
struct ib_dm_svc_entries {
	struct ib_dm_svc_entry service_entries[4];
};
#endif
