






#include <media/rc-map.h>
#include <linux/module.h>

 

static struct rc_map_table behold_columbus[] = {

	 

	{ 0x13, KEY_MUTE },
	{ 0x11, KEY_VIDEO },
	{ 0x1C, KEY_TUNER },	 
	{ 0x12, KEY_POWER },

	 
	{ 0x01, KEY_NUMERIC_1 },
	{ 0x02, KEY_NUMERIC_2 },
	{ 0x03, KEY_NUMERIC_3 },
	{ 0x0D, KEY_SETUP },	   
	{ 0x04, KEY_NUMERIC_4 },
	{ 0x05, KEY_NUMERIC_5 },
	{ 0x06, KEY_NUMERIC_6 },
	{ 0x19, KEY_CAMERA },	 
	{ 0x07, KEY_NUMERIC_7 },
	{ 0x08, KEY_NUMERIC_8 },
	{ 0x09, KEY_NUMERIC_9 },
	{ 0x10, KEY_ZOOM },

	 
	{ 0x0A, KEY_AGAIN },
	{ 0x00, KEY_NUMERIC_0 },
	{ 0x0B, KEY_CHANNELUP },
	{ 0x0C, KEY_VOLUMEUP },

	 

	{ 0x1B, KEY_TIME },
	{ 0x1D, KEY_RECORD },
	{ 0x15, KEY_CHANNELDOWN },
	{ 0x18, KEY_VOLUMEDOWN },

	 

	{ 0x0E, KEY_STOP },
	{ 0x1E, KEY_PAUSE },
	{ 0x0F, KEY_PREVIOUS },
	{ 0x1A, KEY_NEXT },

};

static struct rc_map_list behold_columbus_map = {
	.map = {
		.scan     = behold_columbus,
		.size     = ARRAY_SIZE(behold_columbus),
		.rc_proto = RC_PROTO_UNKNOWN,	 
		.name     = RC_MAP_BEHOLD_COLUMBUS,
	}
};

static int __init init_rc_map_behold_columbus(void)
{
	return rc_map_register(&behold_columbus_map);
}

static void __exit exit_rc_map_behold_columbus(void)
{
	rc_map_unregister(&behold_columbus_map);
}

module_init(init_rc_map_behold_columbus)
module_exit(exit_rc_map_behold_columbus)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mauro Carvalho Chehab");
