






#include <media/rc-map.h>
#include <linux/module.h>

 

static struct rc_map_table behold[] = {

	 
	{ 0x866b1c, KEY_TUNER },	 
	{ 0x866b12, KEY_POWER },

	 
	{ 0x866b01, KEY_NUMERIC_1 },
	{ 0x866b02, KEY_NUMERIC_2 },
	{ 0x866b03, KEY_NUMERIC_3 },
	{ 0x866b04, KEY_NUMERIC_4 },
	{ 0x866b05, KEY_NUMERIC_5 },
	{ 0x866b06, KEY_NUMERIC_6 },
	{ 0x866b07, KEY_NUMERIC_7 },
	{ 0x866b08, KEY_NUMERIC_8 },
	{ 0x866b09, KEY_NUMERIC_9 },

	 
	{ 0x866b0a, KEY_AGAIN },
	{ 0x866b00, KEY_NUMERIC_0 },
	{ 0x866b17, KEY_MODE },

	 
	{ 0x866b14, KEY_SCREEN },
	{ 0x866b10, KEY_ZOOM },

	 
	{ 0x866b0b, KEY_CHANNELUP },
	{ 0x866b18, KEY_VOLUMEDOWN },
	{ 0x866b16, KEY_OK },		 
	{ 0x866b0c, KEY_VOLUMEUP },
	{ 0x866b15, KEY_CHANNELDOWN },

	 
	{ 0x866b11, KEY_MUTE },
	{ 0x866b0d, KEY_INFO },

	 
	{ 0x866b0f, KEY_RECORD },
	{ 0x866b1b, KEY_PLAYPAUSE },
	{ 0x866b1a, KEY_STOP },
	{ 0x866b0e, KEY_TEXT },
	{ 0x866b1f, KEY_RED },	 
	{ 0x866b1e, KEY_VIDEO },

	 
	{ 0x866b1d, KEY_SLEEP },
	{ 0x866b13, KEY_GREEN },
	{ 0x866b19, KEY_BLUE },	 

	 
	{ 0x866b58, KEY_SLOW },
	{ 0x866b5c, KEY_CAMERA },

};

static struct rc_map_list behold_map = {
	.map = {
		.scan     = behold,
		.size     = ARRAY_SIZE(behold),
		.rc_proto = RC_PROTO_NECX,
		.name     = RC_MAP_BEHOLD,
	}
};

static int __init init_rc_map_behold(void)
{
	return rc_map_register(&behold_map);
}

static void __exit exit_rc_map_behold(void)
{
	rc_map_unregister(&behold_map);
}

module_init(init_rc_map_behold)
module_exit(exit_rc_map_behold)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mauro Carvalho Chehab");
