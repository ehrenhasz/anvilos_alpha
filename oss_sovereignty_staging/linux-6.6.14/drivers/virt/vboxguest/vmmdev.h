 
 

#ifndef __VBOX_VMMDEV_H__
#define __VBOX_VMMDEV_H__

#include <asm/bitsperlong.h>
#include <linux/sizes.h>
#include <linux/types.h>
#include <linux/vbox_vmmdev_types.h>

 
#define VMMDEV_PORT_OFF_REQUEST                             0

 
struct vmmdev_memory {
	 
	u32 size;
	 
	u32 version;

	union {
		struct {
			 
			u8 have_events;
			 
			u8 padding[3];
		} V1_04;

		struct {
			 
			u32 host_events;
			 
			u32 guest_event_mask;
		} V1_03;
	} V;

	 
};
VMMDEV_ASSERT_SIZE(vmmdev_memory, 8 + 8);

 
#define VMMDEV_MEMORY_VERSION   (1)

 
#define VMMDEV_EVENT_MOUSE_CAPABILITIES_CHANGED             BIT(0)
 
#define VMMDEV_EVENT_HGCM                                   BIT(1)
 
#define VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST                 BIT(2)
 
#define VMMDEV_EVENT_JUDGE_CREDENTIALS                      BIT(3)
 
#define VMMDEV_EVENT_RESTORED                               BIT(4)
 
#define VMMDEV_EVENT_SEAMLESS_MODE_CHANGE_REQUEST           BIT(5)
 
#define VMMDEV_EVENT_BALLOON_CHANGE_REQUEST                 BIT(6)
 
#define VMMDEV_EVENT_STATISTICS_INTERVAL_CHANGE_REQUEST     BIT(7)
 
#define VMMDEV_EVENT_VRDP                                   BIT(8)
 
#define VMMDEV_EVENT_MOUSE_POSITION_CHANGED                 BIT(9)
 
#define VMMDEV_EVENT_CPU_HOTPLUG                            BIT(10)
 
#define VMMDEV_EVENT_VALID_EVENT_MASK                       0x000007ffU

 
#define VMMDEV_VERSION                      0x00010004
#define VMMDEV_VERSION_MAJOR                (VMMDEV_VERSION >> 16)
#define VMMDEV_VERSION_MINOR                (VMMDEV_VERSION & 0xffff)

 
#define VMMDEV_MAX_VMMDEVREQ_SIZE           1048576

 
#define VMMDEV_REQUEST_HEADER_VERSION       0x10001

 
struct vmmdev_request_header {
	 
	u32 size;
	 
	u32 version;
	 
	enum vmmdev_request_type request_type;
	 
	s32 rc;
	 
	u32 reserved1;
	 
	u32 requestor;
};
VMMDEV_ASSERT_SIZE(vmmdev_request_header, 24);

 
struct vmmdev_mouse_status {
	 
	struct vmmdev_request_header header;
	 
	u32 mouse_features;
	 
	s32 pointer_pos_x;
	 
	s32 pointer_pos_y;
};
VMMDEV_ASSERT_SIZE(vmmdev_mouse_status, 24 + 12);

 
#define VMMDEV_MOUSE_GUEST_CAN_ABSOLUTE                     BIT(0)
 
#define VMMDEV_MOUSE_HOST_WANTS_ABSOLUTE                    BIT(1)
 
#define VMMDEV_MOUSE_GUEST_NEEDS_HOST_CURSOR                BIT(2)
 
#define VMMDEV_MOUSE_HOST_CANNOT_HWPOINTER                  BIT(3)
 
#define VMMDEV_MOUSE_NEW_PROTOCOL                           BIT(4)
 
#define VMMDEV_MOUSE_HOST_RECHECKS_NEEDS_HOST_CURSOR        BIT(5)
 
#define VMMDEV_MOUSE_HOST_HAS_ABS_DEV                       BIT(6)

 
#define VMMDEV_MOUSE_RANGE_MIN 0
 
#define VMMDEV_MOUSE_RANGE_MAX 0xFFFF

 
struct vmmdev_host_version {
	 
	struct vmmdev_request_header header;
	 
	u16 major;
	 
	u16 minor;
	 
	u32 build;
	 
	u32 revision;
	 
	u32 features;
};
VMMDEV_ASSERT_SIZE(vmmdev_host_version, 24 + 16);

 
#define VMMDEV_HVF_HGCM_PHYS_PAGE_LIST  BIT(0)

 
struct vmmdev_mask {
	 
	struct vmmdev_request_header header;
	 
	u32 or_mask;
	 
	u32 not_mask;
};
VMMDEV_ASSERT_SIZE(vmmdev_mask, 24 + 8);

 
#define VMMDEV_GUEST_SUPPORTS_SEAMLESS                      BIT(0)
 
#define VMMDEV_GUEST_SUPPORTS_GUEST_HOST_WINDOW_MAPPING     BIT(1)
 
#define VMMDEV_GUEST_SUPPORTS_GRAPHICS                      BIT(2)
 
#define VMMDEV_GUEST_CAPABILITIES_MASK                      0x00000007U

 
struct vmmdev_hypervisorinfo {
	 
	struct vmmdev_request_header header;
	 
	u32 hypervisor_start;
	 
	u32 hypervisor_size;
};
VMMDEV_ASSERT_SIZE(vmmdev_hypervisorinfo, 24 + 8);

 
struct vmmdev_events {
	 
	struct vmmdev_request_header header;
	 
	u32 events;
};
VMMDEV_ASSERT_SIZE(vmmdev_events, 24 + 4);

#define VMMDEV_OSTYPE_LINUX26		0x53000
#define VMMDEV_OSTYPE_X64		BIT(8)

 
struct vmmdev_guest_info {
	 
	struct vmmdev_request_header header;
	 
	u32 interface_version;
	 
	u32 os_type;
};
VMMDEV_ASSERT_SIZE(vmmdev_guest_info, 24 + 8);

#define VMMDEV_GUEST_INFO2_ADDITIONS_FEATURES_REQUESTOR_INFO	BIT(0)

 
struct vmmdev_guest_info2 {
	 
	struct vmmdev_request_header header;
	 
	u16 additions_major;
	 
	u16 additions_minor;
	 
	u32 additions_build;
	 
	u32 additions_revision;
	 
	u32 additions_features;
	 
	char name[128];
};
VMMDEV_ASSERT_SIZE(vmmdev_guest_info2, 24 + 144);

enum vmmdev_guest_facility_type {
	VBOXGUEST_FACILITY_TYPE_UNKNOWN          = 0,
	VBOXGUEST_FACILITY_TYPE_VBOXGUEST_DRIVER = 20,
	 
	VBOXGUEST_FACILITY_TYPE_AUTO_LOGON       = 90,
	VBOXGUEST_FACILITY_TYPE_VBOX_SERVICE     = 100,
	 
	VBOXGUEST_FACILITY_TYPE_VBOX_TRAY_CLIENT = 101,
	VBOXGUEST_FACILITY_TYPE_SEAMLESS         = 1000,
	VBOXGUEST_FACILITY_TYPE_GRAPHICS         = 1100,
	VBOXGUEST_FACILITY_TYPE_ALL              = 0x7ffffffe,
	 
	VBOXGUEST_FACILITY_TYPE_SIZEHACK         = 0x7fffffff
};

enum vmmdev_guest_facility_status {
	VBOXGUEST_FACILITY_STATUS_INACTIVE    = 0,
	VBOXGUEST_FACILITY_STATUS_PAUSED      = 1,
	VBOXGUEST_FACILITY_STATUS_PRE_INIT    = 20,
	VBOXGUEST_FACILITY_STATUS_INIT        = 30,
	VBOXGUEST_FACILITY_STATUS_ACTIVE      = 50,
	VBOXGUEST_FACILITY_STATUS_TERMINATING = 100,
	VBOXGUEST_FACILITY_STATUS_TERMINATED  = 101,
	VBOXGUEST_FACILITY_STATUS_FAILED      = 800,
	VBOXGUEST_FACILITY_STATUS_UNKNOWN     = 999,
	 
	VBOXGUEST_FACILITY_STATUS_SIZEHACK    = 0x7fffffff
};

 
struct vmmdev_guest_status {
	 
	struct vmmdev_request_header header;
	 
	enum vmmdev_guest_facility_type facility;
	 
	enum vmmdev_guest_facility_status status;
	 
	u32 flags;
};
VMMDEV_ASSERT_SIZE(vmmdev_guest_status, 24 + 12);

#define VMMDEV_MEMORY_BALLOON_CHUNK_SIZE             (1048576)
#define VMMDEV_MEMORY_BALLOON_CHUNK_PAGES            (1048576 / 4096)

 
struct vmmdev_memballoon_info {
	 
	struct vmmdev_request_header header;
	 
	u32 balloon_chunks;
	 
	u32 phys_mem_chunks;
	 
	u32 event_ack;
};
VMMDEV_ASSERT_SIZE(vmmdev_memballoon_info, 24 + 12);

 
struct vmmdev_memballoon_change {
	 
	struct vmmdev_request_header header;
	 
	u32 pages;
	 
	u32 inflate;
	 
	u64 phys_page[VMMDEV_MEMORY_BALLOON_CHUNK_PAGES];
};

 
struct vmmdev_write_core_dump {
	 
	struct vmmdev_request_header header;
	 
	u32 flags;
};
VMMDEV_ASSERT_SIZE(vmmdev_write_core_dump, 24 + 4);

 
struct vmmdev_heartbeat {
	 
	struct vmmdev_request_header header;
	 
	u64 interval_ns;
	 
	u8 enabled;
	 
	u8 padding[3];
} __packed;
VMMDEV_ASSERT_SIZE(vmmdev_heartbeat, 24 + 12);

#define VMMDEV_HGCM_REQ_DONE      BIT(0)
#define VMMDEV_HGCM_REQ_CANCELLED BIT(1)

 
struct vmmdev_hgcmreq_header {
	 
	struct vmmdev_request_header header;

	 
	u32 flags;

	 
	s32 result;
};
VMMDEV_ASSERT_SIZE(vmmdev_hgcmreq_header, 24 + 8);

 
struct vmmdev_hgcm_connect {
	 
	struct vmmdev_hgcmreq_header header;

	 
	struct vmmdev_hgcm_service_location loc;

	 
	u32 client_id;
};
VMMDEV_ASSERT_SIZE(vmmdev_hgcm_connect, 32 + 132 + 4);

 
struct vmmdev_hgcm_disconnect {
	 
	struct vmmdev_hgcmreq_header header;

	 
	u32 client_id;
};
VMMDEV_ASSERT_SIZE(vmmdev_hgcm_disconnect, 32 + 4);

#define VMMDEV_HGCM_MAX_PARMS 32

 
struct vmmdev_hgcm_call {
	 
	struct vmmdev_hgcmreq_header header;

	 
	u32 client_id;
	 
	u32 function;
	 
	u32 parm_count;
	 
};
VMMDEV_ASSERT_SIZE(vmmdev_hgcm_call, 32 + 12);

 
struct vmmdev_hgcm_cancel2 {
	 
	struct vmmdev_request_header header;
	 
	u32 phys_req_to_cancel;
};
VMMDEV_ASSERT_SIZE(vmmdev_hgcm_cancel2, 24 + 4);

#endif
