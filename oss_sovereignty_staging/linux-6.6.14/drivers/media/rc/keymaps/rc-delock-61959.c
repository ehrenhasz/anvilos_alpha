
 

#include <media/rc-map.h>
#include <linux/module.h>

 
static struct rc_map_table delock_61959[] = {
	{ 0x866b16, KEY_POWER2 },	 
	{ 0x866b0c, KEY_POWER },	 

	{ 0x866b00, KEY_NUMERIC_1},
	{ 0x866b01, KEY_NUMERIC_2},
	{ 0x866b02, KEY_NUMERIC_3},
	{ 0x866b03, KEY_NUMERIC_4},
	{ 0x866b04, KEY_NUMERIC_5},
	{ 0x866b05, KEY_NUMERIC_6},
	{ 0x866b06, KEY_NUMERIC_7},
	{ 0x866b07, KEY_NUMERIC_8},
	{ 0x866b08, KEY_NUMERIC_9},
	{ 0x866b14, KEY_NUMERIC_0},

	{ 0x866b0a, KEY_ZOOM},		 
	{ 0x866b10, KEY_CAMERA},	 
	{ 0x866b0e, KEY_CHANNEL},	 
	{ 0x866b13, KEY_ESC},            

	{ 0x866b20, KEY_UP},
	{ 0x866b21, KEY_DOWN},
	{ 0x866b42, KEY_LEFT},
	{ 0x866b43, KEY_RIGHT},
	{ 0x866b0b, KEY_OK},

	{ 0x866b11, KEY_CHANNELUP},
	{ 0x866b1b, KEY_CHANNELDOWN},

	{ 0x866b12, KEY_VOLUMEUP},
	{ 0x866b48, KEY_VOLUMEDOWN},
	{ 0x866b44, KEY_MUTE},

	{ 0x866b1a, KEY_RECORD},
	{ 0x866b41, KEY_PLAY},
	{ 0x866b40, KEY_STOP},
	{ 0x866b19, KEY_PAUSE},
	{ 0x866b1c, KEY_FASTFORWARD},	 
	{ 0x866b1e, KEY_REWIND},	 

};

static struct rc_map_list delock_61959_map = {
	.map = {
		.scan     = delock_61959,
		.size     = ARRAY_SIZE(delock_61959),
		.rc_proto = RC_PROTO_NECX,
		.name     = RC_MAP_DELOCK_61959,
	}
};

static int __init init_rc_map_delock_61959(void)
{
	return rc_map_register(&delock_61959_map);
}

static void __exit exit_rc_map_delock_61959(void)
{
	rc_map_unregister(&delock_61959_map);
}

module_init(init_rc_map_delock_61959)
module_exit(exit_rc_map_delock_61959)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jakob Haufe <sur5r@sur5r.net>");
MODULE_DESCRIPTION("Delock 61959 remote keytable");
