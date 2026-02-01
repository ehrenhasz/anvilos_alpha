 
 

#ifndef __HGSMI_CH_SETUP_H__
#define __HGSMI_CH_SETUP_H__

 
#define HGSMI_CC_HOST_FLAGS_LOCATION 0

struct hgsmi_buffer_location {
	u32 buf_location;
	u32 buf_len;
} __packed;

 

#define HGSMIHOSTFLAGS_COMMANDS_PENDING    0x01u
#define HGSMIHOSTFLAGS_IRQ                 0x02u
#define HGSMIHOSTFLAGS_VSYNC               0x10u
#define HGSMIHOSTFLAGS_HOTPLUG             0x20u
#define HGSMIHOSTFLAGS_CURSOR_CAPABILITIES 0x40u

struct hgsmi_host_flags {
	u32 host_flags;
	u32 reserved[3];
} __packed;

#endif
