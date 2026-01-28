#ifndef __VBOXVIDEO_H__
#define __VBOXVIDEO_H__
#define VBOX_VIDEO_MAX_SCREENS 64
struct vbva_cmd_hdr {
	s16 x;
	s16 y;
	u16 w;
	u16 h;
} __packed;
#define VBVA_RING_BUFFER_SIZE        (4194304 - 1024)
#define VBVA_RING_BUFFER_THRESHOLD   (4096)
#define VBVA_MAX_RECORDS (64)
#define VBVA_F_MODE_ENABLED         0x00000001u
#define VBVA_F_MODE_VRDP            0x00000002u
#define VBVA_F_MODE_VRDP_RESET      0x00000004u
#define VBVA_F_MODE_VRDP_ORDER_MASK 0x00000008u
#define VBVA_F_STATE_PROCESSING     0x00010000u
#define VBVA_F_RECORD_PARTIAL       0x80000000u
struct vbva_record {
	u32 len_and_flags;
} __packed;
#define VBVA_ADAPTER_INFORMATION_SIZE 65536
#define VBVA_MIN_BUFFER_SIZE          65536
#define VBOX_VIDEO_DISABLE_ADAPTER_MEMORY        0xFFFFFFFF
#define VBOX_VIDEO_INTERPRET_ADAPTER_MEMORY      0x00000000
#define VBOX_VIDEO_INTERPRET_DISPLAY_MEMORY_BASE 0x00010000
struct vbva_host_flags {
	u32 host_events;
	u32 supported_orders;
} __packed;
struct vbva_buffer {
	struct vbva_host_flags host_flags;
	u32 data_offset;
	u32 free_offset;
	struct vbva_record records[VBVA_MAX_RECORDS];
	u32 record_first_index;
	u32 record_free_index;
	u32 partial_write_tresh;
	u32 data_len;
	u8 data[];
} __packed;
#define VBVA_MAX_RECORD_SIZE (128 * 1024 * 1024)
#define VBVA_QUERY_CONF32			 1
#define VBVA_SET_CONF32				 2
#define VBVA_INFO_VIEW				 3
#define VBVA_INFO_HEAP				 4
#define VBVA_FLUSH				 5
#define VBVA_INFO_SCREEN			 6
#define VBVA_ENABLE				 7
#define VBVA_MOUSE_POINTER_SHAPE		 8
#define VBVA_INFO_CAPS				12
#define VBVA_SCANLINE_CFG			13
#define VBVA_SCANLINE_INFO			14
#define VBVA_CMDVBVA_SUBMIT			16
#define VBVA_CMDVBVA_FLUSH			17
#define VBVA_CMDVBVA_CTL			18
#define VBVA_QUERY_MODE_HINTS			19
#define VBVA_REPORT_INPUT_MAPPING		20
#define VBVA_CURSOR_POSITION			21
#define VBVAHG_EVENT				1
#define VBVAHG_DISPLAY_CUSTOM			2
#define VBOX_VBVA_CONF32_MONITOR_COUNT		0
#define VBOX_VBVA_CONF32_HOST_HEAP_SIZE		1
#define VBOX_VBVA_CONF32_MODE_HINT_REPORTING	2
#define VBOX_VBVA_CONF32_GUEST_CURSOR_REPORTING	3
#define VBOX_VBVA_CONF32_CURSOR_CAPABILITIES	4
#define VBOX_VBVA_CONF32_SCREEN_FLAGS		5
#define VBOX_VBVA_CONF32_MAX_RECORD_SIZE	6
struct vbva_conf32 {
	u32 index;
	u32 value;
} __packed;
#define VBOX_VBVA_CURSOR_CAPABILITY_RESERVED0   BIT(0)
#define VBOX_VBVA_CURSOR_CAPABILITY_HARDWARE    BIT(1)
#define VBOX_VBVA_CURSOR_CAPABILITY_RESERVED2   BIT(2)
#define VBOX_VBVA_CURSOR_CAPABILITY_RESERVED3   BIT(3)
#define VBOX_VBVA_CURSOR_CAPABILITY_RESERVED4   BIT(4)
#define VBOX_VBVA_CURSOR_CAPABILITY_RESERVED5   BIT(5)
struct vbva_infoview {
	u32 view_index;
	u32 view_offset;
	u32 view_size;
	u32 max_screen_size;
} __packed;
struct vbva_flush {
	u32 reserved;
} __packed;
#define VBVA_SCREEN_F_NONE			0x0000
#define VBVA_SCREEN_F_ACTIVE			0x0001
#define VBVA_SCREEN_F_DISABLED			0x0002
#define VBVA_SCREEN_F_BLANK			0x0004
#define VBVA_SCREEN_F_BLANK2			0x0008
struct vbva_infoscreen {
	u32 view_index;
	s32 origin_x;
	s32 origin_y;
	u32 start_offset;
	u32 line_size;
	u32 width;
	u32 height;
	u16 bits_per_pixel;
	u16 flags;
} __packed;
#define VBVA_F_NONE				0x00000000
#define VBVA_F_ENABLE				0x00000001
#define VBVA_F_DISABLE				0x00000002
#define VBVA_F_EXTENDED				0x00000004
#define VBVA_F_ABSOFFSET			0x00000008
struct vbva_enable {
	u32 flags;
	u32 offset;
	s32 result;
} __packed;
struct vbva_enable_ex {
	struct vbva_enable base;
	u32 screen_id;
} __packed;
struct vbva_mouse_pointer_shape {
	s32 result;
	u32 flags;
	u32 hot_X;
	u32 hot_y;
	u32 width;
	u32 height;
	u8 data[4];
} __packed;
#define VBOX_MOUSE_POINTER_VISIBLE		0x0001
#define VBOX_MOUSE_POINTER_ALPHA		0x0002
#define VBOX_MOUSE_POINTER_SHAPE		0x0004
#define VBVACAPS_COMPLETEGCMD_BY_IOREAD		0x00000001
#define VBVACAPS_IRQ				0x00000002
#define VBVACAPS_VIDEO_MODE_HINTS		0x00000004
#define VBVACAPS_DISABLE_CURSOR_INTEGRATION	0x00000008
#define VBVACAPS_USE_VBVA_ONLY			0x00000010
struct vbva_caps {
	s32 rc;
	u32 caps;
} __packed;
struct vbva_query_mode_hints {
	u16 hints_queried_count;
	u16 hint_structure_guest_size;
	s32 rc;
} __packed;
struct vbva_modehint {
	u32 magic;
	u32 cx;
	u32 cy;
	u32 bpp;		 
	u32 display;
	u32 dx;			 
	u32 dy;			 
	u32 enabled;		 
} __packed;
#define VBVAMODEHINT_MAGIC 0x0801add9u
struct vbva_report_input_mapping {
	s32 x;	 
	s32 y;	 
	u32 cx;	 
	u32 cy;	 
} __packed;
struct vbva_cursor_position {
	u32 report_position;	 
	u32 x;			 
	u32 y;			 
} __packed;
#endif
