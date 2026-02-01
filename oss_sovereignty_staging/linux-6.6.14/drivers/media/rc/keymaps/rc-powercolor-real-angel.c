






#include <media/rc-map.h>
#include <linux/module.h>

 

static struct rc_map_table powercolor_real_angel[] = {
	{ 0x38, KEY_SWITCHVIDEOMODE },	 
	{ 0x0c, KEY_MEDIA },		 
	{ 0x00, KEY_NUMERIC_0 },
	{ 0x01, KEY_NUMERIC_1 },
	{ 0x02, KEY_NUMERIC_2 },
	{ 0x03, KEY_NUMERIC_3 },
	{ 0x04, KEY_NUMERIC_4 },
	{ 0x05, KEY_NUMERIC_5 },
	{ 0x06, KEY_NUMERIC_6 },
	{ 0x07, KEY_NUMERIC_7 },
	{ 0x08, KEY_NUMERIC_8 },
	{ 0x09, KEY_NUMERIC_9 },
	{ 0x0a, KEY_DIGITS },		 
	{ 0x29, KEY_PREVIOUS },		 
	{ 0x12, KEY_BRIGHTNESSUP },
	{ 0x13, KEY_BRIGHTNESSDOWN },
	{ 0x2b, KEY_MODE },		 
	{ 0x2c, KEY_TEXT },		 
	{ 0x20, KEY_CHANNELUP },	 
	{ 0x21, KEY_CHANNELDOWN },	 
	{ 0x10, KEY_VOLUMEUP },		 
	{ 0x11, KEY_VOLUMEDOWN },	 
	{ 0x0d, KEY_MUTE },
	{ 0x1f, KEY_RECORD },
	{ 0x17, KEY_PLAY },
	{ 0x16, KEY_PAUSE },
	{ 0x0b, KEY_STOP },
	{ 0x27, KEY_FASTFORWARD },
	{ 0x26, KEY_REWIND },
	{ 0x1e, KEY_SEARCH },		 
	{ 0x0e, KEY_CAMERA },		 
	{ 0x2d, KEY_SETUP },
	{ 0x0f, KEY_SCREEN },		 
	{ 0x14, KEY_RADIO },		 
	{ 0x25, KEY_POWER },		 
};

static struct rc_map_list powercolor_real_angel_map = {
	.map = {
		.scan     = powercolor_real_angel,
		.size     = ARRAY_SIZE(powercolor_real_angel),
		.rc_proto = RC_PROTO_UNKNOWN,	 
		.name     = RC_MAP_POWERCOLOR_REAL_ANGEL,
	}
};

static int __init init_rc_map_powercolor_real_angel(void)
{
	return rc_map_register(&powercolor_real_angel_map);
}

static void __exit exit_rc_map_powercolor_real_angel(void)
{
	rc_map_unregister(&powercolor_real_angel_map);
}

module_init(init_rc_map_powercolor_real_angel)
module_exit(exit_rc_map_powercolor_real_angel)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mauro Carvalho Chehab");
