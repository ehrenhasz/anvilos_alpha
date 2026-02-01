






#include <media/rc-map.h>
#include <linux/module.h>

static struct rc_map_table cinergy[] = {
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

	{ 0x0a, KEY_POWER },
	{ 0x0b, KEY_MEDIA },		 
	{ 0x0c, KEY_ZOOM },		 
	{ 0x0d, KEY_CHANNELUP },	 
	{ 0x0e, KEY_CHANNELDOWN },	 
	{ 0x0f, KEY_VOLUMEUP },
	{ 0x10, KEY_VOLUMEDOWN },
	{ 0x11, KEY_TUNER },		 
	{ 0x12, KEY_NUMLOCK },		 
	{ 0x13, KEY_AUDIO },		 
	{ 0x14, KEY_MUTE },
	{ 0x15, KEY_UP },
	{ 0x16, KEY_DOWN },
	{ 0x17, KEY_LEFT },
	{ 0x18, KEY_RIGHT },
	{ 0x19, BTN_LEFT, },
	{ 0x1a, BTN_RIGHT, },
	{ 0x1b, KEY_WWW },		 
	{ 0x1c, KEY_REWIND },
	{ 0x1d, KEY_FORWARD },
	{ 0x1e, KEY_RECORD },
	{ 0x1f, KEY_PLAY },
	{ 0x20, KEY_PREVIOUSSONG },
	{ 0x21, KEY_NEXTSONG },
	{ 0x22, KEY_PAUSE },
	{ 0x23, KEY_STOP },
};

static struct rc_map_list cinergy_map = {
	.map = {
		.scan     = cinergy,
		.size     = ARRAY_SIZE(cinergy),
		.rc_proto = RC_PROTO_UNKNOWN,	 
		.name     = RC_MAP_CINERGY,
	}
};

static int __init init_rc_map_cinergy(void)
{
	return rc_map_register(&cinergy_map);
}

static void __exit exit_rc_map_cinergy(void)
{
	rc_map_unregister(&cinergy_map);
}

module_init(init_rc_map_cinergy)
module_exit(exit_rc_map_cinergy)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mauro Carvalho Chehab");
