 
 

#ifndef _ALPS_H
#define _ALPS_H

#include <linux/input/mt.h>

#define ALPS_PROTO_V1		0x100
#define ALPS_PROTO_V2		0x200
#define ALPS_PROTO_V3		0x300
#define ALPS_PROTO_V3_RUSHMORE	0x310
#define ALPS_PROTO_V4		0x400
#define ALPS_PROTO_V5		0x500
#define ALPS_PROTO_V6		0x600
#define ALPS_PROTO_V7		0x700	 
#define ALPS_PROTO_V8		0x800	 
#define ALPS_PROTO_V9		0x900	 

#define MAX_TOUCHES	4

#define DOLPHIN_COUNT_PER_ELECTRODE	64
#define DOLPHIN_PROFILE_XOFFSET		8	 
#define DOLPHIN_PROFILE_YOFFSET		1	 

 
enum SS4_PACKET_ID {
	SS4_PACKET_ID_IDLE = 0,
	SS4_PACKET_ID_ONE,
	SS4_PACKET_ID_TWO,
	SS4_PACKET_ID_MULTI,
	SS4_PACKET_ID_STICK,
};

#define SS4_COUNT_PER_ELECTRODE		256
#define SS4_NUMSENSOR_XOFFSET		7
#define SS4_NUMSENSOR_YOFFSET		7
#define SS4_MIN_PITCH_MM		50

#define SS4_MASK_NORMAL_BUTTONS		0x07

#define SS4PLUS_COUNT_PER_ELECTRODE	128
#define SS4PLUS_NUMSENSOR_XOFFSET	16
#define SS4PLUS_NUMSENSOR_YOFFSET	5
#define SS4PLUS_MIN_PITCH_MM		37

#define IS_SS4PLUS_DEV(_b)	(((_b[0]) == 0x73) &&	\
				 ((_b[1]) == 0x03) &&	\
				 ((_b[2]) == 0x28)		\
				)

#define SS4_IS_IDLE_V2(_b)	(((_b[0]) == 0x18) &&		\
				 ((_b[1]) == 0x10) &&		\
				 ((_b[2]) == 0x00) &&		\
				 ((_b[3] & 0x88) == 0x08) &&	\
				 ((_b[4]) == 0x10) &&		\
				 ((_b[5]) == 0x00)		\
				)

#define SS4_1F_X_V2(_b)		(((_b[0]) & 0x0007) |		\
				 ((_b[1] << 3) & 0x0078) |	\
				 ((_b[1] << 2) & 0x0380) |	\
				 ((_b[2] << 5) & 0x1C00)	\
				)

#define SS4_1F_Y_V2(_b)		(((_b[2]) & 0x000F) |		\
				 ((_b[3] >> 2) & 0x0030) |	\
				 ((_b[4] << 6) & 0x03C0) |	\
				 ((_b[4] << 5) & 0x0C00)	\
				)

#define SS4_1F_Z_V2(_b)		(((_b[5]) & 0x0F) |		\
				 ((_b[5] >> 1) & 0x70) |	\
				 ((_b[4]) & 0x80)		\
				)

#define SS4_1F_LFB_V2(_b)	(((_b[2] >> 4) & 0x01) == 0x01)

#define SS4_MF_LF_V2(_b, _i)	((_b[1 + (_i) * 3] & 0x0004) == 0x0004)

#define SS4_BTN_V2(_b)		((_b[0] >> 5) & SS4_MASK_NORMAL_BUTTONS)

#define SS4_STD_MF_X_V2(_b, _i)	(((_b[0 + (_i) * 3] << 5) & 0x00E0) |	\
				 ((_b[1 + _i * 3]  << 5) & 0x1F00)	\
				)

#define SS4_PLUS_STD_MF_X_V2(_b, _i) (((_b[0 + (_i) * 3] << 4) & 0x0070) | \
				 ((_b[1 + (_i) * 3]  << 4) & 0x0F80)	\
				)

#define SS4_STD_MF_Y_V2(_b, _i)	(((_b[1 + (_i) * 3] << 3) & 0x0010) |	\
				 ((_b[2 + (_i) * 3] << 5) & 0x01E0) |	\
				 ((_b[2 + (_i) * 3] << 4) & 0x0E00)	\
				)

#define SS4_BTL_MF_X_V2(_b, _i)	(SS4_STD_MF_X_V2(_b, _i) |		\
				 ((_b[0 + (_i) * 3] >> 3) & 0x0010)	\
				)

#define SS4_PLUS_BTL_MF_X_V2(_b, _i) (SS4_PLUS_STD_MF_X_V2(_b, _i) |	\
				 ((_b[0 + (_i) * 3] >> 4) & 0x0008)	\
				)

#define SS4_BTL_MF_Y_V2(_b, _i)	(SS4_STD_MF_Y_V2(_b, _i) | \
				 ((_b[0 + (_i) * 3] >> 3) & 0x0008)	\
				)

#define SS4_MF_Z_V2(_b, _i)	(((_b[1 + (_i) * 3]) & 0x0001) |	\
				 ((_b[1 + (_i) * 3] >> 1) & 0x0002)	\
				)

#define SS4_IS_MF_CONTINUE(_b)	((_b[2] & 0x10) == 0x10)
#define SS4_IS_5F_DETECTED(_b)	((_b[2] & 0x10) == 0x10)

#define SS4_TS_X_V2(_b)		(s8)(				\
				 ((_b[0] & 0x01) << 7) |	\
				 (_b[1] & 0x7F)		\
				)

#define SS4_TS_Y_V2(_b)		-(s8)(				\
				 ((_b[3] & 0x01) << 7) |	\
				 (_b[2] & 0x7F)		\
				)

#define SS4_TS_Z_V2(_b)		(s8)(_b[4] & 0x7F)


#define SS4_MFPACKET_NO_AX		8160	 
#define SS4_MFPACKET_NO_AY		4080	 
#define SS4_MFPACKET_NO_AX_BL		8176	 
#define SS4_MFPACKET_NO_AY_BL		4088	 
#define SS4_PLUS_MFPACKET_NO_AX		4080	 
#define SS4_PLUS_MFPACKET_NO_AX_BL	4088	 

 
enum V7_PACKET_ID {
	 V7_PACKET_ID_IDLE,
	 V7_PACKET_ID_TWO,
	 V7_PACKET_ID_MULTI,
	 V7_PACKET_ID_NEW,
	 V7_PACKET_ID_UNKNOWN,
};

 
struct alps_protocol_info {
	u16 version;
	u8 byte0, mask0;
	unsigned int flags;
};

 
struct alps_model_info {
	u8 signature[3];
	struct alps_protocol_info protocol_info;
};

 
struct alps_nibble_commands {
	int command;
	unsigned char data;
};

struct alps_bitmap_point {
	int start_bit;
	int num_bits;
};

 
struct alps_fields {
	unsigned int x_map;
	unsigned int y_map;
	unsigned int fingers;

	int pressure;
	struct input_mt_pos st;
	struct input_mt_pos mt[MAX_TOUCHES];

	unsigned int first_mp:1;
	unsigned int is_mp:1;

	unsigned int left:1;
	unsigned int right:1;
	unsigned int middle:1;

	unsigned int ts_left:1;
	unsigned int ts_right:1;
	unsigned int ts_middle:1;
};

 
struct alps_data {
	struct psmouse *psmouse;
	struct input_dev *dev2;
	struct input_dev *dev3;
	char phys2[32];
	char phys3[32];
	struct delayed_work dev3_register_work;

	 
	const struct alps_nibble_commands *nibble_commands;
	int addr_command;
	u16 proto_version;
	u8 byte0, mask0;
	u8 dev_id[3];
	u8 fw_ver[3];
	int flags;
	int x_max;
	int y_max;
	int x_bits;
	int y_bits;
	unsigned int x_res;
	unsigned int y_res;

	int (*hw_init)(struct psmouse *psmouse);
	void (*process_packet)(struct psmouse *psmouse);
	int (*decode_fields)(struct alps_fields *f, unsigned char *p,
			      struct psmouse *psmouse);
	void (*set_abs_params)(struct alps_data *priv, struct input_dev *dev1);

	int prev_fin;
	int multi_packet;
	int second_touch;
	unsigned char multi_data[6];
	struct alps_fields f;
	u8 quirks;
	struct timer_list timer;
};

#define ALPS_QUIRK_TRACKSTICK_BUTTONS	1  

int alps_detect(struct psmouse *psmouse, bool set_properties);
int alps_init(struct psmouse *psmouse);

#endif
