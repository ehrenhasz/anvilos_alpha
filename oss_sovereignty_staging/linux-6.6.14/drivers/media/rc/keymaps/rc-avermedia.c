






#include <media/rc-map.h>
#include <linux/module.h>

 

static struct rc_map_table avermedia[] = {
	{ 0x28, KEY_NUMERIC_1 },
	{ 0x18, KEY_NUMERIC_2 },
	{ 0x38, KEY_NUMERIC_3 },
	{ 0x24, KEY_NUMERIC_4 },
	{ 0x14, KEY_NUMERIC_5 },
	{ 0x34, KEY_NUMERIC_6 },
	{ 0x2c, KEY_NUMERIC_7 },
	{ 0x1c, KEY_NUMERIC_8 },
	{ 0x3c, KEY_NUMERIC_9 },
	{ 0x22, KEY_NUMERIC_0 },

	{ 0x20, KEY_TV },		 
	{ 0x10, KEY_CD },		 
	{ 0x30, KEY_TEXT },		 
	{ 0x00, KEY_POWER },		 

	{ 0x08, KEY_VIDEO },		 
	{ 0x04, KEY_AUDIO },		 
	{ 0x0c, KEY_ZOOM },		 

	{ 0x12, KEY_SUBTITLE },		 
	{ 0x32, KEY_REWIND },		 
	{ 0x02, KEY_PRINT },		 

	{ 0x2a, KEY_SEARCH },		 
	{ 0x1a, KEY_SLEEP },		 
	{ 0x3a, KEY_CAMERA },		 
	{ 0x0a, KEY_MUTE },		 

	{ 0x26, KEY_RECORD },		 
	{ 0x16, KEY_PAUSE },		 
	{ 0x36, KEY_STOP },		 
	{ 0x06, KEY_PLAY },		 

	{ 0x2e, KEY_RED },		 
	{ 0x21, KEY_GREEN },		 
	{ 0x0e, KEY_YELLOW },		 
	{ 0x01, KEY_BLUE },		 

	{ 0x1e, KEY_VOLUMEDOWN },	 
	{ 0x3e, KEY_VOLUMEUP },		 
	{ 0x11, KEY_CHANNELDOWN },	 
	{ 0x31, KEY_CHANNELUP }		 
};

static struct rc_map_list avermedia_map = {
	.map = {
		.scan     = avermedia,
		.size     = ARRAY_SIZE(avermedia),
		.rc_proto = RC_PROTO_UNKNOWN,	 
		.name     = RC_MAP_AVERMEDIA,
	}
};

static int __init init_rc_map_avermedia(void)
{
	return rc_map_register(&avermedia_map);
}

static void __exit exit_rc_map_avermedia(void)
{
	rc_map_unregister(&avermedia_map);
}

module_init(init_rc_map_avermedia)
module_exit(exit_rc_map_avermedia)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mauro Carvalho Chehab");
