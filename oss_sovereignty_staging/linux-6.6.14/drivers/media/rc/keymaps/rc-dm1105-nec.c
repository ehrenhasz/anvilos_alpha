






#include <media/rc-map.h>
#include <linux/module.h>

 

static struct rc_map_table dm1105_nec[] = {
	{ 0x0a, KEY_POWER2},		 
	{ 0x0c, KEY_MUTE},		 
	{ 0x11, KEY_NUMERIC_1},
	{ 0x12, KEY_NUMERIC_2},
	{ 0x13, KEY_NUMERIC_3},
	{ 0x14, KEY_NUMERIC_4},
	{ 0x15, KEY_NUMERIC_5},
	{ 0x16, KEY_NUMERIC_6},
	{ 0x17, KEY_NUMERIC_7},
	{ 0x18, KEY_NUMERIC_8},
	{ 0x19, KEY_NUMERIC_9},
	{ 0x10, KEY_NUMERIC_0},
	{ 0x1c, KEY_CHANNELUP},		 
	{ 0x0f, KEY_CHANNELDOWN},	 
	{ 0x1a, KEY_VOLUMEUP},		 
	{ 0x0e, KEY_VOLUMEDOWN},	 
	{ 0x04, KEY_RECORD},		 
	{ 0x09, KEY_CHANNEL},		 
	{ 0x08, KEY_BACKSPACE},		 
	{ 0x07, KEY_FASTFORWARD},	 
	{ 0x0b, KEY_PAUSE},		 
	{ 0x02, KEY_ESC},		 
	{ 0x03, KEY_TAB},		 
	{ 0x00, KEY_UP},		 
	{ 0x1f, KEY_ENTER},		 
	{ 0x01, KEY_DOWN},		 
	{ 0x05, KEY_RECORD},		 
	{ 0x06, KEY_STOP},		 
	{ 0x40, KEY_ZOOM},		 
	{ 0x1e, KEY_TV},		 
	{ 0x1b, KEY_B},			 
};

static struct rc_map_list dm1105_nec_map = {
	.map = {
		.scan     = dm1105_nec,
		.size     = ARRAY_SIZE(dm1105_nec),
		.rc_proto = RC_PROTO_UNKNOWN,	 
		.name     = RC_MAP_DM1105_NEC,
	}
};

static int __init init_rc_map_dm1105_nec(void)
{
	return rc_map_register(&dm1105_nec_map);
}

static void __exit exit_rc_map_dm1105_nec(void)
{
	rc_map_unregister(&dm1105_nec_map);
}

module_init(init_rc_map_dm1105_nec)
module_exit(exit_rc_map_dm1105_nec)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mauro Carvalho Chehab");
