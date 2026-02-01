






#include <media/rc-map.h>
#include <linux/module.h>

 

static struct rc_map_table kworld_plus_tv_analog[] = {
	{ 0x0c, KEY_MEDIA },		 
	{ 0x16, KEY_CLOSECD },		 
	{ 0x1d, KEY_POWER2 },

	{ 0x00, KEY_NUMERIC_1 },
	{ 0x01, KEY_NUMERIC_2 },

	 
	{ 0x02, KEY_NUMERIC_3 },

	 
	{ 0x03, KEY_NUMERIC_4 },
	{ 0x04, KEY_NUMERIC_5 },
	{ 0x05, KEY_NUMERIC_6 },
	{ 0x06, KEY_NUMERIC_7 },
	{ 0x07, KEY_NUMERIC_8 },
	{ 0x08, KEY_NUMERIC_9 },
	{ 0x0a, KEY_NUMERIC_0 },

	{ 0x09, KEY_AGAIN },
	{ 0x14, KEY_MUTE },

	{ 0x20, KEY_UP },
	{ 0x21, KEY_DOWN },
	{ 0x0b, KEY_ENTER },

	{ 0x10, KEY_CHANNELUP },
	{ 0x11, KEY_CHANNELDOWN },

	 

	{ 0x13, KEY_VOLUMEUP },
	{ 0x12, KEY_VOLUMEDOWN },

	 
	{ 0x19, KEY_TIME},		 
	{ 0x1a, KEY_STOP},
	{ 0x1b, KEY_RECORD},

	{ 0x22, KEY_TEXT},

	{ 0x15, KEY_AUDIO},		 
	{ 0x0f, KEY_ZOOM},
	{ 0x1c, KEY_CAMERA},		 

	{ 0x18, KEY_RED},		 
	{ 0x23, KEY_GREEN},		 
};

static struct rc_map_list kworld_plus_tv_analog_map = {
	.map = {
		.scan     = kworld_plus_tv_analog,
		.size     = ARRAY_SIZE(kworld_plus_tv_analog),
		.rc_proto = RC_PROTO_UNKNOWN,	 
		.name     = RC_MAP_KWORLD_PLUS_TV_ANALOG,
	}
};

static int __init init_rc_map_kworld_plus_tv_analog(void)
{
	return rc_map_register(&kworld_plus_tv_analog_map);
}

static void __exit exit_rc_map_kworld_plus_tv_analog(void)
{
	rc_map_unregister(&kworld_plus_tv_analog_map);
}

module_init(init_rc_map_kworld_plus_tv_analog)
module_exit(exit_rc_map_kworld_plus_tv_analog)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mauro Carvalho Chehab");
