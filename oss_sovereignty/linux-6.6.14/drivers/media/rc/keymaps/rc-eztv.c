






#include <media/rc-map.h>
#include <linux/module.h>

 

static struct rc_map_table eztv[] = {
	{ 0x12, KEY_POWER },
	{ 0x01, KEY_TV },	 
	{ 0x15, KEY_DVD },	 
	{ 0x17, KEY_AUDIO },	 
				 

	{ 0x1b, KEY_MUTE },	 
	{ 0x02, KEY_LANGUAGE },	 
	{ 0x1e, KEY_SUBTITLE },	 
	{ 0x16, KEY_ZOOM },	 
	{ 0x1c, KEY_VIDEO },	 
	{ 0x1d, KEY_RESTART },	 
	{ 0x2f, KEY_SEARCH },	 
	{ 0x30, KEY_CHANNEL },	 

	{ 0x31, KEY_HELP },	 
	{ 0x32, KEY_MODE },	 
	{ 0x33, KEY_ESC },	 

	{ 0x0c, KEY_UP },	 
	{ 0x10, KEY_DOWN },	 
	{ 0x08, KEY_LEFT },	 
	{ 0x04, KEY_RIGHT },	 
	{ 0x03, KEY_SELECT },	 

	{ 0x1f, KEY_REWIND },	 
	{ 0x20, KEY_PLAYPAUSE }, 
	{ 0x29, KEY_FORWARD },	 
	{ 0x14, KEY_AGAIN },	 
	{ 0x2b, KEY_RECORD },	 
	{ 0x2c, KEY_STOP },	 
	{ 0x2d, KEY_PLAY },	 
	{ 0x2e, KEY_CAMERA },	 

	{ 0x00, KEY_NUMERIC_0 },
	{ 0x05, KEY_NUMERIC_1 },
	{ 0x06, KEY_NUMERIC_2 },
	{ 0x07, KEY_NUMERIC_3 },
	{ 0x09, KEY_NUMERIC_4 },
	{ 0x0a, KEY_NUMERIC_5 },
	{ 0x0b, KEY_NUMERIC_6 },
	{ 0x0d, KEY_NUMERIC_7 },
	{ 0x0e, KEY_NUMERIC_8 },
	{ 0x0f, KEY_NUMERIC_9 },

	{ 0x2a, KEY_VOLUMEUP },
	{ 0x11, KEY_VOLUMEDOWN },
	{ 0x18, KEY_CHANNELUP }, 
	{ 0x19, KEY_CHANNELDOWN }, 

	{ 0x13, KEY_ENTER },	 
	{ 0x21, KEY_DOT },	 
};

static struct rc_map_list eztv_map = {
	.map = {
		.scan     = eztv,
		.size     = ARRAY_SIZE(eztv),
		.rc_proto = RC_PROTO_UNKNOWN,	 
		.name     = RC_MAP_EZTV,
	}
};

static int __init init_rc_map_eztv(void)
{
	return rc_map_register(&eztv_map);
}

static void __exit exit_rc_map_eztv(void)
{
	rc_map_unregister(&eztv_map);
}

module_init(init_rc_map_eztv)
module_exit(exit_rc_map_eztv)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mauro Carvalho Chehab");
